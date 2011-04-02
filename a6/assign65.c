//Brendan MacDonell (100777952) And Imran Iqbal (100794182)
#include <stdlib.h>
#include <stdio.h>
#include "hcs12dp256.h"
#include <string.h>
#include <ctype.h>

/* Define an unsigned byte type. */
typedef unsigned char byte;

/* Define the boolean type. */
typedef unsigned char boolean;
#define true  1
#define false 0

/* Bit-set and clear operations. Could alternatively *
 * use the ASM directive if these are too slow.      */
#define SETMSK(var, msk) ((var) |= (msk))
#define CLRMSK(var, msk) ((var) &= ~(msk))
#define TOGGLEMASK(var, msk) ((var) ^= (msk))


#define LCD_LINE_1 	   0x80
#define LCD_LINE_2 	   0xC0


/* Assembly functions */
void delay(void);
void LCD2PP_Init(void);
void delay50ms(void);
void clearLCD(void);
void updateLCD(byte address, char *s);
void moveLCDCursor(byte address);
void LoadStrLCD( char *s );

typedef struct {
  byte hours;
  unsigned deci_seconds;
} time_t;

/* Global variables ***************************************************/
time_t time = { 0, 0 };
time_t alarm = { 25, 0 };
int temperature = 0;

/* Different Mode definitions go here */
typedef enum {
	SET_CLOCK = 0,
	VIEW_CLOCK,
	VIEW_MENU,
	VIEW_SET_ALARM,
	VIEW_SET_TIMER,
	VIEW_SET_STOPWATCH,
	NUM_STATES
} state_t;

state_t state = SET_CLOCK;

/* These are our state display / keypress handlers stuff */

typedef struct{
	void (* display)(char *, char *);
	void (*key_pressed)(char key);
} state_handler_t;

//Rough int to ascii function
void int_to_ascii(int num, char *s, char padding, int strlen){
	 s[--strlen] = '\0';
	 strlen--;
	 if( num == 0 ){
	 	 s[strlen--] = '0';
	 }
	 for( ; strlen >= 0; strlen-- ){
	 	 int r = num % 10;
		 
		 if (num) s[strlen] = r + '0';
		 else     s[strlen] = padding;
		 
		 num /= 10;
	 }
}

/** NOTE: YOU MUST Guard all calls by isdigit! */
#define char_to_digit(c) (c - '0')

int get_celsius( void ){
	return ((temperature -32) * 5 / 9);
}

/* LED STUFFS    *******************************************************/
#define RED_LED    0x01
#define ORANGE_LED 0x02
#define YELLOW_LED 0x04
#define GREEN_LED  0x08

void clearLEDs(void) {
  SETMSK(DDRK, 0x0F);  /* Enable output to the LEDs. */
  CLRMSK(PORTK, 0x0F); /* Disable all of the LEDs. */
}

void setLED(byte mask, boolean on) {
  SETMSK(DDRK, 0x0F); /* Enable output to the LEDs. */

  /* Set the signal to the LEDs appropriately. */
  if (on) SETMSK(PORTK, mask);
  else    CLRMSK(PORTK, mask);
}
/** Clcok handler thingies ********************************************/

#define LOCK_CLOCK()   CLRMSK(TIE, 0x01) // Disable interrupts on OC0.
#define UNLOCK_CLOCK() SETMSK(TIE, 0x01) // Enable interrupts on OC0.

/* Handle showing the clock. */
void show_clock_display( char *a, char *b ){
	unsigned hrs, ds;
	
	LOCK_CLOCK();
	hrs = time.hours;
	ds = time.deci_seconds;
	UNLOCK_CLOCK();
	
	strcpy(a, "Press 0 for menu" );
	int_to_ascii(hrs, b, '0', 3);
	b[2] = ':';
	int_to_ascii(ds / 600, b + 3, '0', 3);
	b[5] = ':';
	int_to_ascii( (ds % 600) / 10, b + 6, '0', 3);
	b[8] = ' ';
	int_to_ascii(temperature, b + 11, ' ', 4);
	b[14] = ' ';
	b[15] = 'F';
}

void keypress_clock_display(char key) {
  switch (key) {
	case '0': state = VIEW_MENU;
	          break;
	default:  // XXX: Annoying noise.
			  break;
  }
}

/* Handle setting the clock. */
char set_time_buffer[5] = "";
unsigned int set_time_position = 0;

void enter_time(char key, time_t *sometime, char *buffer, unsigned *positionptr) {
  unsigned digit = char_to_digit(key);
  unsigned position = *positionptr;
  
  if (!isdigit(key) ||
      (position == 0 && digit > 2) ||
	  (position == 1 && buffer[position-1] == '2' && digit > 3) ||
	  (digit > 5 && position == 2)) {
    // TODO: Short beep to show that entry is invalid.
	return;
  }
  
  buffer[position++] = key;
  buffer[position] = '\0';
  *positionptr = position;
  
  if (position == 4) { // We're done
   	LOCK_CLOCK();
	
	sometime->hours = char_to_digit(buffer[0]) * 10 + char_to_digit(buffer[1]);
	sometime->deci_seconds = char_to_digit(buffer[2]) * 10 + char_to_digit(buffer[3]);
	sometime->deci_seconds *= 600;
	
	UNLOCK_CLOCK();
	
	state = VIEW_CLOCK;
  }
}

void keypress_setup(char key) {
  enter_time(key, &time, set_time_buffer, &set_time_position); 
}

void show_time(const char *prompt, char *a, char *b, char *buffer) {
	strcpy(a, prompt);
	strncpy(b, buffer, 2);
	b[2] = ':';
	if (strlen(buffer) > 2) strncpy(b+3, buffer+2, 2);
}

void show_setup( char *a, char *b ){
	show_time("Enter time HH:MM", a, b, set_time_buffer);
}

/* Handle alarm entry */
char set_alarm_buffer[5];
unsigned set_alarm_position = 0;

void keypress_alarm(char key) {
  enter_time(key, &alarm, set_alarm_buffer, &set_alarm_position); 
}

void show_alarm( char *a, char *b ){
	show_time("Alarm time HH:MM", a, b, set_alarm_buffer);
}

/* Handle the menu. */
void keypress_menu(char key) {
  switch(key) {
    case '1': state = VIEW_SET_ALARM;
			  set_alarm_position = 0;
	          break;
	case '2': state = VIEW_SET_TIMER;
			  break;
	case '3': state = VIEW_SET_STOPWATCH;
			  break;
	default:  // TODO: MAKE BEEPING NOISE.
			  break;
  }
}

void show_menu(char *a, char *b) {
	strcpy(a, "1:Alarm 2:Timer");
	strcpy(b, "3:Stopwatch");
}

state_handler_t state_handlers[NUM_STATES] = {
    // display,            key_pressed
	{ show_setup,          keypress_setup }, // Set the clock.
	{ show_clock_display,  keypress_clock_display }, // View the clock / temperature.
	{ show_menu,           keypress_menu }, // Show the menu.          
	{ show_alarm,          keypress_alarm }, // View / set the alarm.
	{ NULL,                NULL }, // View / set the timer.
	{ NULL,                NULL }, // View / set the stopwatch.
};


/** KEYPAD HANDLERS ***************************************************/
#define ROWS 4
#define COLS 4

const char keypad_characters[] = { '1', '2', '3', 'A',
                                   '4', '5', '6', 'B',
                                   '7', '8', '9', 'C',
                                   'E', '0', 'F', 'D' };

#pragma interrupt_handler KISR()
void KISR(void) {
  byte ptp = PTP, ptt = PTT, pts = PTS & 0x80;
  byte row;
  
  CLRMSK(PTS, 0x80);
  
  // Ensure we don't update the seven-segment display.
  PTT = 0;

  /* Ensure that the ports are primed to receive keypresses. */
  SETMSK(DDRP, 0x0F); /* Set row polling bits (PTP0..3) as outputs. */
  CLRMSK(DDRH, 0xF0); /* Set column bits (PTH4..7) as inputs. */
  SPI1CR1 = 0;        /* Disable the serial interface. */
  
  /* Probe all of the rows. */
  for (row = 0; row < ROWS; row++) {
    byte col, col_mask;

	SETMSK(PTM, 0x08); /* Pass through the latch to the keypad. */
    PTP = 0x01 << row; /* Query the current keypad row. */
    CLRMSK(PTM, 0x08); /* Re-enable latching on keypad. */

    col_mask = PTH;    /* Copy the polling result. */

    /* Test all of the columns in the row. */
    for (col = 0; col < COLS; col++) {
      byte table_index = row * ROWS + col;
      boolean is_pressed = col_mask & (0x10 << col);

      /* Check if the given column is _newly_ set. */
      if (is_pressed) {
        void (*key_handler)(char) = state_handlers[state].key_pressed;
        char character = keypad_characters[table_index];
		
		//putchar(character); // FUUUUUUUUUUUUUUUUUUUUUUUUUU!
		
        /* Execute the key handler, if one is present. */
        if (key_handler != NULL) key_handler(character);
		
		if (character == 'E') { // 'E' is always escape.
		  state = VIEW_CLOCK;
		}
      }
    }
  }
  
  SETMSK(PTM, 0x08);
  PTP = 0x0F;  // Restore scan row(s)
  CLRMSK(PTM, 0x08);
  
  PTP = ptp;   // Restore the value on Port P.  
  PTT = ptt;
  PIFH = PIFH;
  
  SETMSK(PTS, pts);
}

/** CLOCK FUNCTIONS ****************************************************/
#define TC0_TICK_LENGTH 25000

#pragma interrupt_handler clock
void clock(void) {

  ++time.deci_seconds;
  if(time.deci_seconds % 10 == 0) { ATD0CTL5 = 0x86; } //Check the temp, yo.
  if(time.deci_seconds % 36000 == 0 ){
  	time.hours = (time.hours + 1) % 24;
	time.deci_seconds = 0;
  }
  
  TC0 += TC0_TICK_LENGTH; // Prepare for a new tick.
}

/** TEMP SENSOR     ****************************************************/
#pragma interrupt_handler check_temperature
void check_temperature(void){
	temperature = ((ATD0DR6 & 0x03FF))/8 - 5;
}

/** GENERIC DISPLAY ****************************************************/
#define EMPTY_STRING16 "                "

void display_line(byte address, char *line) {
  unsigned last = strlen(line);
  
  for (; last < 16; last++) line[last] = ' ';

  moveLCDCursor(address);
  LoadStrLCD(line);
}

void update_display(void) {
  char line_1[16] = EMPTY_STRING16;
  char line_2[16] = EMPTY_STRING16;
  
  void (*display_handler)(char*,char*) = state_handlers[state].display;
  
  if (display_handler == NULL) return;
  
  display_handler(line_1, line_2);
  display_line(LCD_LINE_1, line_1);
  display_line(LCD_LINE_2, line_2);
}

/** INITIALIZATION  ****************************************************/
void init(void) {
  // Set up the KISR.
  DDRP |= 0x0F; // bitset PP0-3 as outputs (rows)
  DDRH &= 0x0F; // bitclear PH4..7 as inputs (columns)

  PIFH  = 0xFF; // Clear previous interrupt flags
  PPSH  = 0xF0; // Rising Edge
  PERH  = 0x00; // Disable pulldowns
  PIEH |= 0xF0; // Local enable on columns inputs

  SETMSK(PTM, 0x08); /* Pass through the latch to the keypad. */
  PTP   = 0x0F; // Set scan row(s)
  CLRMSK(PTM, 0x08);

  // Set up the clock (TC0) ISR.
  TSCR1 = 0x90;        // Enable TCNT and fast-flags clear.
  TSCR2 = 0x05;        // Set a pre-scale factor of 32x, no overflow interrupts.
  TIOS  = 0x01;        // Enable OC0.
  SETMSK(TIE, 0x01);   // Enable interrupts on OC0.
  TC0   = TCNT + TC0_TICK_LENGTH; // Set up the timeout on the output compare.

  //This sets up the temperature sensor thing
  ATD0CTL2 = 0xFA; // Enables ATD
  ATD0CTL3 = 0x00; // Continue conversions
  ATD0CTL4 = 0x60; // Select 10-bit operation
  						  // Set sample time to 16 ATD clock period
						  // Set clock prescale to 0
  ATD0CTL5 = 0x86; // Right justified, Unsigned and single scan
  
  INTR_ON();  
  
  // Set up the LCD.
  clearLEDs();
  LCD2PP_Init();
}

int main(int argc, char **argv) {
  init();
  
  while(true) {
    update_display();
  }
  
  return 0; /* Satisfy the compiler. */
}