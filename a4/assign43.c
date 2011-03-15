//Brendan MacDonell (100777952) And Imran Iqbal (100794182)
#include <stdlib.h>
#include <stdio.h>
#include "hcs12dp256.h"

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

#define RED_LED    0x01
#define ORANGE_LED 0x02
#define YELLOW_LED 0x04
#define GREEN_LED  0x08

#define LCD_LINE_1 	   0x80
#define LCD_LINE_2 	   0xC0

#define CCS_ADDRESS	   0x8C
#define HEATER_ADDRESS 0xC2
#define SPEED_ADDRESS  0x85
#define TEMP_ADDRESS   0xCC

/* XXX: WRITE THIS SUBROUTINE IN ASSEMBLY. */
void delay(void);
void LCD2PP_Init(void);

/* Changes the spot you are writing to. */
void DELAY50M(void);
void clearLCD(void);
void updateLCD(byte address, char *s);
void moveLCDCursor(byte address);
void LoadStrLCD( char *s );

/*Global Variables*/
int rpm = 100;
int speed = 0;
boolean ccs_enabled = false;
int ccs_rpm = 0;
boolean heat_enabled = false;
byte    temperature = 22;
boolean vent_enabled = false;

/*
LCDs DDRAM

 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F
 S  P  E  E  D  X  X  X     C  C  S  O  F  F
 
 H  :  O  F  F                 T  :  X  X  C
 C1 C2 C3 C4 C5 C6 C7 C8 C9 CA CB CC CD CE CF

addresses that need to be changed: 0x06, 0x07, 0x08, 0x0D, 0x0E, 0x0F
				 : 0xC3, 0xC4, 0xC5, 0xCD, 0xCE

*/
char lcd_line_1[] = "SPEED    CCS   ";
char lcd_line_2[] = "H:        T:  C";

/** LCD STUFF ******************************************************/
void setupLCD(void){
	 byte ptt = PTT;
	 byte ptp = PTP;
	 LCD2PP_Init();

	 LoadStrLCD(lcd_line_1);
	 moveLCDCursor(0xC0);
	 LoadStrLCD(lcd_line_2);
	 
 	 PTP = ptp;
	 PTT = ptt;
}

//Rough int to ascii function
void int_to_ascii(int num, char *s, int strlen){
	 s[--strlen] = '\0';
	 strlen--;
	 if( num == 0 ){
	 	 s[strlen--] = '0';
	 }
	 for( ; strlen >= 0; strlen-- ){
	 	 int r = num % 10;
		 
		 if (num) s[strlen] = r + '0';
		 else     s[strlen] = ' ';
		 
		 num /= 10;
	 }
}

void updateLCD(byte address, char *s) {
	 byte ptt = PTT;
	 byte ptp = PTP;
	 
	 moveLCDCursor(address);
	 LoadStrLCD(s);
	 
 	 PTP = ptp;
	 PTT = ptt; 
}

void setOnOff(boolean bool, byte address){
	 if (bool) updateLCD(address, " ON");
	 else      updateLCD(address, "OFF");
}

void updateTemp(void){
  char tmp[3];
  int_to_ascii(temperature, tmp, 3);
  updateLCD(TEMP_ADDRESS, tmp);
}

/* Rough function to calculate speed ripped from assignment 3; Replace with your own if you need to*/
boolean calc_speed(int RPM, int wheelR, int *speed ){
	 *speed = RPM * wheelR * 2 * 3.14 * 60/1000;
	if( *speed > 200 ){
		return true;
	}
	return false;
}

void updateSpeed(void){
  char spd[4];
  if (ccs_enabled) calc_speed(ccs_rpm, 1, &speed);
  else             calc_speed(rpm, 1, &speed);

  int_to_ascii(speed, spd, 4);
  updateLCD(SPEED_ADDRESS, spd);
}

void setLCDVariables(void){
	 updateSpeed();
	 updateTemp();
	 setOnOff(ccs_enabled, CCS_ADDRESS);
	 setOnOff(heat_enabled, HEATER_ADDRESS);
}

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

/** SPEED CONTROL *****************************************************/

void accelerate(void) {
  rpm++;
  updateSpeed();
}

void brake(void) {
  if( rpm == 0 ) { return; }
  rpm--;
  updateSpeed();
}

/** CRUISE CONTROL ****************************************************/


void set_ccs(boolean enabled) {
  ccs_enabled = enabled;      /* Update the CCS condition. */
  setLED(GREEN_LED, enabled); /* Update the green LED. */
  ccs_rpm = rpm;
  setOnOff(enabled, CCS_ADDRESS);
}

void toggle_ccs(void) {
  set_ccs(!ccs_enabled);
}

void increase_ccs(void) {
  ccs_rpm++;
  updateSpeed();
}

void decrease_ccs(void) {
  if( ccs_rpm == 0 ){ return; }
  ccs_rpm--;
  updateSpeed();
}

void disable_ccs(void) {
  set_ccs(false);
}

/** CLIMATE CONTROL ***************************************************/

void toggle_heat(void) {
  heat_enabled = !heat_enabled;  /* Toggle the heating condition */
  setLED(RED_LED, heat_enabled); /* Update the red LED. */
  setOnOff(heat_enabled, HEATER_ADDRESS);
}

void increase_heat(void) {
  if( temperature == 99 ){ return; }
  temperature++;
  updateTemp();
}

void decrease_heat(void) {
  if( temperature == 0 ) { return; }
  temperature--;
  updateTemp();
}

void toggle_vent(void) {
  vent_enabled = !vent_enabled;     /* Toggle the vent state */
  setLED(YELLOW_LED, vent_enabled); /* Update the yellow LED. */
}

/** WINDOW CONTROL ****************************************************/
void raise_window(void) {
}

void lower_window(void) {
}


/** MISC CONTROL ******************************************************/
void emergency(void) {
  clearLCD();
  updateLCD(LCD_LINE_1, "Emergency stop");
  
  SETMSK(DDRK, 0x20);  /* Enable output to the buzzer. */
  SETMSK(PORTK, 0x20); /* Turn on the buzzer. */

  delay();             /* Delay for ~1 second. */

  CLRMSK(PORTK, 0x20); /* Turn off the buzzer. */

  exit(0);
}

void finish(void) {
  exit(0);
}

/** KEYPAD HANDLERS ***************************************************/
#define ROWS 4
#define COLS 4

const char keypad_characters[] = { '1', '2', '3', 'A',
                                   '4', '5', '6', 'B',
                                   '7', '8', '9', 'C',
                                   'E', '0', 'F', 'D' };

typedef void (*keypad_handler_t)(void);

const keypad_handler_t keypad_handlers[] = {
  toggle_ccs,     /* 1 */
  toggle_heat,    /* 2 */
  raise_window,   /* 3 */
  accelerate,     /* A */
  increase_ccs,   /* 4 */
  increase_heat,  /* 5 */
  lower_window,   /* 6 */
  brake,          /* B */
  decrease_ccs,   /* 7 */
  decrease_heat,  /* 8 */
  finish,         /* 9 */
  NULL,           /* C */
  disable_ccs,    /* E */
  toggle_vent,    /* 0 */
  emergency,      /* F */
  NULL            /* D */
};

#pragma interrupt_handler KISR()
void KISR(void) {
  byte ptp = PTP;
  byte row, col, col_mask = PIFH; // Save the column pressed.
  byte SPI = SPI1CR1;
  
  /* Ensure that the ports are primed to receive keypresses. */
  SETMSK(DDRP, 0x0F); /* Set row polling bits (PTP0..3) as outputs. */
  CLRMSK(DDRH, 0xF0); /* Set column bits (PTH4..7) as inputs. */
  SPI1CR1 = 0;        /* Disable the serial interface. */

  /* Find the column (break on the first set bit.) */
  for (col = 0; col < 4; col++) {
    if (col_mask & (1 << (col + 4))) break;
  }
  
  /* Probe all of the rows. */
  for (row = 0; row < ROWS; row++) {
	PTP = 0x01 << row; /* Query the current keypad row. */
	SETMSK(PTM, 0x08);
	CLRMSK(PTM, 0x08);

	/* Test the selected column. */
    if (PTH & col_mask) {
	    byte table_index = row * ROWS + col;
        keypad_handler_t key_handler = keypad_handlers[table_index];

        /* Additional requirement: Display the character entered. */
        putchar(keypad_characters[table_index]);
		
        /* Execute the key handler, if one is present. */
        if (key_handler != NULL) key_handler();
    }
  }
	
  /* Restore the serial interface. */
  SPI1CR1 = SPI;
  
  SETMSK(PTM, 0x08);
  PTP   = ptp;  // Restore scan row(s)
  PIFH = PIFH; 
  PIEH |= 0xF0; // Allow column inputs.
}

int main(int argc, char **argv) {
  DDRP |= 0x0F; // bitset PP0-3 as outputs (rows)
  DDRH &= 0x0F; // bitclear PH4..7 as inputs (columns)
  PTP   = 0x0F; // Set scan row(s)
  PIFH  = 0xFF; // Clear previous interrupt flags
  PPSH  = 0xF0; // Rising Edge
  PERH  = 0x00; // Disable pulldowns
  PIEH |= 0xF0; // Local enable on columns inputs
  SETMSK(PTM, 0x08); /* Pass through the latch to the keypad. */
  asm("cli");
  
  clearLEDs();
  setupLCD();
  setLCDVariables();
    
  /* Query for a keypress until the user kills the program. */
  while(true) ;

  return 0; /* Satisfy the compiler. */
}
