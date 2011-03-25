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

#define LCD_LINE_1 	   0x80

void LCD2PP_Init(void);
void clearLCD(void);
void updateLCD(byte address, char *s);
void moveLCDCursor(byte address);
void LoadStrLCD( char *s );

/** LCD STUFF ******************************************************/
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

/** MOTOR CONTROL ****************************************************/
#define MOTOR_OUTPUT_MASK 0x60
#define MOTOR_ENABLE_MASK 0x20

static const char stepper_motor_values[] = {
  0x60, // Up and left.
  0x40, // Up and right.
  0x00, // Down and right.
  0x20  // Down and left.
};

/* step_motor(int steps, boolean clockwise):                    *
 * Move the stepper motor 'steps' steps in the given direction. */
void step_motor(void) {
  static unsigned char step = 0;
  signed char direction = 1;

  /* Set the direction and step output. */
  SETMSK(DDRT, MOTOR_OUTPUT_MASK);

  /* Disable the 7-segment display, and enable the stepper motor. */
  SETMSK(DDRP, MOTOR_ENABLE_MASK);
  SETMSK(PTP, MOTOR_ENABLE_MASK);

  /* Set the value of the motor output. */
  CLRMSK(PTT, MOTOR_OUTPUT_MASK);
  SETMSK(PTT, stepper_motor_values[step]);
  
  /* Update the step we are on. */
  step = (step + direction) & 0x03;
}

/** TIMING ISRS *****************************************************/
unsigned int rti_ticks = 0;

void enable_rti(void) {
  SETMSK(CRGINT, 0x80); // Enable the timer.
  rti_ticks = 0;        // 1000 ticks = 2 seconds.
}

void disable_rti(void) {
  CLRMSK(CRGINT, 0x80); // Disable the timer.
  //CLRMSK(DDRP, MOTOR_ENABLE_MASK);
}

#pragma interrupt_handler RTI
void RTI(void) {
  ++rti_ticks;
  
  if (rti_ticks % 42 == 0) {
  	 step_motor();
  }
  
  if (rti_ticks == 1000) {
	disable_rti();
	enable_rti();
  }
  
  CRGFLG = 0x80;
}

/** CLOCK FUNCTIONS ****************************************************/
#define TC0_TICK_LENGTH 50000

int uptime_seconds = 0;

#pragma interrupt_handler clock
void clock(void) {
  static unsigned int ticks = 0;
  
  if (++ticks % 5 == 0) {
    ++uptime_seconds;
  }
  
  TC0 += TC0_TICK_LENGTH; // Prepare for a new tick.
}

void init(void) {
  // Set up the RTI.
  RTICTL = 0x17;// Generate an. intr. every 2ms.
  // Set up the clock (TC0) ISR.
  TSCR1 = 0x90;        // Enable TCNT and fast-flags clear.
  TSCR2 = 0x05;        // Set a pre-scale factor of 32x, no overflow interrupts.
  TIOS  = 0x01;        // Enable OC0.
  SETMSK(TIE, 0x01);   // Enable interrupts on OC0.
  TC0   = TCNT + TC0_TICK_LENGTH; // Set up the timeout on the output compare.
  INTR_ON();
  
  enable_rti();
  
  
  // Set up the LCD.
  LCD2PP_Init();
}

void update_display(void) {
  char buffer[3];
  const unsigned int seconds = uptime_seconds; // Ensure we have a consistent copy
                                               // of uptime_seconds (ex. it could
										       // change between the two conversions.)
											   
  moveLCDCursor(LCD_LINE_1);
  
  LoadStrLCD("     ");
  
  // Update the clock.
  int_to_ascii(seconds / 60, buffer, '0', 3);
  LoadStrLCD(buffer);
	
  LoadStrLCD(":");
	
  int_to_ascii(seconds % 60, buffer, '0', 3);
  LoadStrLCD(buffer);
  
}

int main(int argc, char **argv) {
  init();
  while(true) {
    update_display();
  }
  
  return 0;
}