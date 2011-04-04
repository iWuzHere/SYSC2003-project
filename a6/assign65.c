/* BONUS PROJECTL DIGITAL CLOCK
 *
 * Brendan MacDonell (100777952) And Imran Iqbal (100794182)
 * 
 * Instructions:
 * 1. Enter time on start-up as HH:MM using the keypad.
 * 2. Program alarms and timers through menus (accessed from 0 in the main menu.)
 *    (Keys are prompted on the top of the LCD in all modes.)
 * 3. NOTE: A short tone will sound if a key is pressed which the current view
 *    cannot handle.
 * 4. Use 'E' to return to the main screen from any view, or 'F' to disable
 *    any alarms which are currently sounding.
 *
 * Issues:
 * 1. Excessive key bounces occur, but there is insufficient time to remedy this.
 * 2. The source code is disorganized: attempts have been made to group code
 *    by function, but this has been made more difficult by the requirement to
 *    force all code into a single source file.
 */

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
#define MAX_DECISECONDS 36000
#define MAX_HOURS       24

time_t time      = { 0,         0 };
time_t alarm     = { MAX_HOURS, 0 };
time_t timer     = { MAX_HOURS, 0 };
time_t stopwatch = { 0,         0 };
unsigned end_beep_deciseconds = MAX_DECISECONDS;

int temperature = 0;
boolean alarm_on = false;
boolean timer_on = false;
boolean stopwatch_on = false;

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

boolean time_equals(time_t *a, time_t *b) {
  return a->hours == b->hours && a->deci_seconds == b->deci_seconds;
}

#define BUZZER_MASK 0x20

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

void error_beep(void) {
  end_beep_deciseconds = (time.deci_seconds + 5) % MAX_DECISECONDS;
  SETMSK(PORTK, BUZZER_MASK);
}

void disable_all_alarms(void) {
  alarm_on = false;
  timer_on = false;
  CLRMSK(PORTK, BUZZER_MASK);
}

/** Clcok handler thingies ********************************************/

#define LOCK_CLOCK()   CLRMSK(TIE, 0x01) // Disable interrupts on OC0.
#define UNLOCK_CLOCK() SETMSK(TIE, 0x01) // Enable interrupts on OC0.

void write_time(time_t *sometime, char *line) {
  	unsigned hrs, ds;
	
	LOCK_CLOCK();
	hrs = sometime->hours;
	ds = sometime->deci_seconds;
	UNLOCK_CLOCK();
	
	int_to_ascii(hrs, line, '0', 3);
	line[2] = ':';
	int_to_ascii(ds / 600, line + 3, '0', 3);
	line[5] = ':';
	int_to_ascii( (ds % 600) / 10, line + 6, '0', 3);
	line[8] = ' ';
}

/* Handle showing the clock. */
void show_clock_display( char *a, char *b ){
	strcpy(a, "Press 0 for menu" );
	
	write_time(&time, b);
	
	int_to_ascii(temperature, b + 11, ' ', 4);
	b[14] = ' ';
	b[15] = 'F';
}

void keypress_clock_display(char key) {
  switch (key) {
	case '0': state = VIEW_MENU;
	          break;
	default:  error_beep();
			  break;
  }
}

/* Handle setting the clock. */
char set_time_buffer[5] = "";
unsigned int set_time_position = 0;

void set_time_t(time_t *sometime, char *buffer) {
  LOCK_CLOCK();
  sometime->hours = char_to_digit(buffer[0]) * 10 + char_to_digit(buffer[1]);
  sometime->deci_seconds = char_to_digit(buffer[2]) * 10 + char_to_digit(buffer[3]);  
  sometime->deci_seconds *= 600;
  UNLOCK_CLOCK();
  state = VIEW_CLOCK;
}

boolean enter_time(char key, time_t *sometime, char *buffer, unsigned *positionptr, boolean validate) {
  unsigned digit = char_to_digit(key);
  unsigned position = *positionptr;

  if (!isdigit(key)) {
    error_beep(); // Show that the entry is invalid.
    return false;
  }
  
  if (validate) {
    if ((position == 0 && digit > 2) ||
	    (position == 1 && buffer[position-1] == '2' && digit > 3) ||
	    (digit > 5 && position == 2)) {
      error_beep(); // Show that the entry is invalid.
      return false;  
	}
  }
  
  buffer[position++] = key;
  buffer[position] = '\0';

  if (position == 4) {
    *positionptr = 0;
    return true;
  } else {
    *positionptr = position;
    return false;
  }
}

void keypress_setup(char key) {
  if(enter_time(key, &time, set_time_buffer, &set_time_position, true)) {
    set_time_t(&time, set_time_buffer);
  } 
}

void show_time_prompt(const char *prompt, char *a, char *b, char *buffer) {
	strcpy(a, prompt);
	strncpy(b, buffer, 2);
	b[2] = ':';
	if (strlen(buffer) > 2) strncpy(b+3, buffer+2, 2);
}

void clear_time_prompt(char *buffer) {
  unsigned i;
  for (i = 0; i < 6; i++) buffer[i] = '\0';
}

void show_setup( char *a, char *b ){
	show_time_prompt("Enter time HH:MM", a, b, set_time_buffer);
}

/* Handle alarm entry */
char set_alarm_buffer[5];
unsigned set_alarm_position = 0;

void keypress_alarm(char key) {
  if(enter_time(key, &alarm, set_alarm_buffer, &set_alarm_position, true)) {
    set_time_t(&alarm, set_alarm_buffer);
  } 
}

void show_alarm( char *a, char *b ){
	show_time_prompt("Alarm time HH:MM", a, b, set_alarm_buffer);
}

/* Handle the menu. */
void keypress_menu(char key) {
  switch(key) {
    case 'A': state = VIEW_SET_ALARM;
			  set_alarm_position = 0;
	          break;
	case 'B': state = VIEW_SET_TIMER;
			  break;
	case 'C': state = VIEW_SET_STOPWATCH;
			  break;
	default:  error_beep(); // Show that the entry is invalid.
			  break;
  }
}

void show_menu(char *a, char *b) {
	strcpy(a, "A:Alarm B:Timer");
	strcpy(b, "C:Stopwatch");
}

/* Handle the timer. */
char set_timer_buffer[5];
unsigned set_timer_position = 0;

void keypress_timer(char key) {
  int minutes, seconds;
  int ds, hrs;
  
  if (enter_time(key, &timer, set_timer_buffer, &set_timer_position, false)) {
	minutes = char_to_digit(set_timer_buffer[0]) * 10 + char_to_digit(set_timer_buffer[1]);
	seconds = char_to_digit(set_timer_buffer[2]) * 10 + char_to_digit(set_timer_buffer[3]);
	
    LOCK_CLOCK();
	ds = time.deci_seconds + minutes * 600 + seconds * 10;
	hrs = time.hours;
	timer.hours = (hrs + ds / MAX_DECISECONDS) % 24;
	timer.deci_seconds = ds % MAX_DECISECONDS;
    UNLOCK_CLOCK();
	
	clear_time_prompt(set_timer_buffer);
  } 
}

void show_timer( char *a, char *b ){
    if (set_timer_position == 0 && timer.hours != MAX_HOURS) {
	  time_t difference;
	  
	  LOCK_CLOCK();
	  difference.hours = timer.hours - time.hours;
  	  difference.deci_seconds = timer.deci_seconds - time.deci_seconds;
	  UNLOCK_CLOCK();
	  
	  strcpy(a, "Time left:");
	
      write_time(&difference, b);
      b[8] = '.';
      int_to_ascii((difference.deci_seconds % 600) % 10, b + 9, ' ', 2);
	} else {
      show_time_prompt("New Timer MM:SS", a, b, set_timer_buffer);
	}
}

/* Handle stopwatch. */
void keypress_stopwatch(char key) {
  switch(key) {
  case '1': stopwatch_on = true;
            break;
  case '2': stopwatch_on = false;
            break;
  case '3': stopwatch.hours = 0;
			stopwatch.deci_seconds = 0;
			break;
  default:  error_beep(); // Show that the entry is invalid.
            break;
  }
}

void show_stopwatch( char *a, char *b ){
	unsigned ds; 
    strcpy(a, "1:On 2:Off 3:Clr");
	
	LOCK_CLOCK();
	ds = stopwatch.deci_seconds;
	UNLOCK_CLOCK();
	
	write_time(&stopwatch, b);
	b[8] = '.';
	int_to_ascii((ds % 600) % 10, b + 9, ' ', 2);
}

state_handler_t state_handlers[NUM_STATES] = {
    // display,            key_pressed
	{ show_setup,          keypress_setup }, // Set the clock.
	{ show_clock_display,  keypress_clock_display }, // View the clock / temperature.
	{ show_menu,           keypress_menu }, // Show the menu.          
	{ show_alarm,          keypress_alarm }, // View / set the alarm.
	{ show_timer,          keypress_timer }, // View / set the timer.
	{ show_stopwatch,      keypress_stopwatch }, // View / set the stopwatch.
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
  byte ptp, ptt, pts;
  byte row;
  
  ptp = PTP;
  ptt = PTT;
  pts = PTS & 0x80;
  
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
	    char character = keypad_characters[table_index];
		void (*key_handler)(char) = state_handlers[state].key_pressed;

        /* Return to the main view if 'E' is pressed. */
        if (character == 'E') {
		  state = VIEW_CLOCK; // Return to the main view.
		  continue;           // Don't allow this to fall through into key handlers.
		} else if (character == 'F') { // Global alarm disable.
		  disable_all_alarms();
		  continue;
		}
		  
        /* Execute the key handler, if one is present. */
        if (key_handler != NULL) key_handler(character);
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
void next_hour(time_t *sometime) {
  if(sometime->deci_seconds % MAX_DECISECONDS == 0 ){
  	sometime->hours = (sometime->hours + 1) % MAX_HOURS;
	sometime->deci_seconds = 0;
  }
}

void clock(void) {
  ++time.deci_seconds;
  next_hour(&time);
	
  if (stopwatch_on) {
    ++stopwatch.deci_seconds;
	next_hour(&stopwatch);
  }
  
  if(time.deci_seconds % 10 == 0) {
    ATD0CTL5 = 0x86;  //Check the temp, yo.
  }
  
  // Stop beeping, if necessary.
  if (time.deci_seconds == end_beep_deciseconds) {
    end_beep_deciseconds = MAX_DECISECONDS;
	CLRMSK(PORTK, BUZZER_MASK);
  }
  
  // Activate the alarm beep if appropriate.
  if (time_equals(&time, &alarm)) alarm_on = true;
  
  // Activate the timer beep if necessary.
  if (time_equals(&time, &timer)) {
    timer_on = true;
	timer.hours = MAX_HOURS;
  }
  
  if (alarm_on ) {
    if (time.deci_seconds % 5 == 0) {
      TOGGLEMASK(PORTK, BUZZER_MASK);
	}
  } if (timer_on) {
    if (time.deci_seconds % 3) {
      TOGGLEMASK(PORTK, BUZZER_MASK);
	}
  }
  
  TC0 += TC0_TICK_LENGTH; // Prepare for a new tick.
}

/** TEMP SENSOR     ****************************************************/
#pragma interrupt_handler check_temperature
void check_temperature(void){
	temperature = ((ATD0DR6 & 0x03FF))/8 - 5;
}

/** GENERIC DISPLAY ****************************************************/

void display_line(byte address, char *line) {
  unsigned last = strlen(line);
  
  for (; last < 16; last++) line[last] = ' ';

  moveLCDCursor(address);
  LoadStrLCD(line);
}

void clear_line(char *line) {
  memset(line, ' ', 16);
  line[16] = '\0';
}

void update_display(void) {
  void (*display_handler)(char*,char*) = state_handlers[state].display;
  char line_1[17];
  char line_2[17];
  
  clear_line(line_1);
  clear_line(line_2);
  
  if (display_handler == NULL) return;
  
  display_handler(line_1, line_2);
  display_line(LCD_LINE_1, line_1);
  display_line(LCD_LINE_2, line_2);
}

/** INITIALIZATION  ****************************************************/
void init(void) {
  // Set up the RTI.
  RTICTL = 0x17;// Generate an. intr. every 2ms.
  
  // Set up the buzzer.
  SETMSK(DDRK, BUZZER_MASK);
  
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