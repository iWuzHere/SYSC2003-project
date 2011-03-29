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


/* lol? ************************************/
#define _I(off) (*(unsigned int volatile *)(_IO_BASE + off))
#define PACNT     _I(0x62)

/* XXX: WRITE THIS SUBROUTINE IN ASSEMBLY. */
void delay(void);
void LCD2PP_Init(void);

/* Changes the spot you are writing to. */
void delay50ms(void);
void clearLCD(void);
void updateLCD(byte address, char *s);
void moveLCDCursor(byte address);
void LoadStrLCD( char *s );

/* Global Variables */
int     rps = 0;
boolean ccs_enabled = false;
int     ccs_rpm = 0;
boolean heat_enabled = false;
byte    temperature = 22;
boolean vent_enabled = false;
boolean emergency_stop = false;
boolean window_going_up;

/*
LCDs DDRAM

 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F
 S  P  E  E  D  X  X  X     C  C  S  O  F  F
 
 H  :  O  F  F                 T  :  X  X  C
 C1 C2 C3 C4 C5 C6 C7 C8 C9 CA CB CC CD CE CF

addresses that need to be changed: 0x06, 0x07, 0x08, 0x0D, 0x0E, 0x0F
				 : 0xC3, 0xC4, 0xC5, 0xCD, 0xCE

*/

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

/* Rough function to calculate speed ripped from assignment 3; Replace with your own if you need to*/
unsigned calc_speed(int RPM, int wheelR){
	return RPM * wheelR * 2 * 3.14 * 60/1000;
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
#define MAX_DUTY_CYCLE 30
#define MIN_DUTY_CYCLE  0

void accelerate(void) {
  if( (PWMDTY7) < MAX_DUTY_CYCLE) (PWMDTY7)++;
}

void brake(void) {
  if( (PWMDTY7) > MIN_DUTY_CYCLE) (PWMDTY7)--;
}

/** CRUISE CONTROL ****************************************************/


void set_ccs(boolean enabled) {
  ccs_enabled = enabled;      /* Update the CCS condition. */
  setLED(GREEN_LED, enabled); /* Update the green LED. */
  ccs_rpm = rps*60;
}

void toggle_ccs(void) {
  set_ccs(!ccs_enabled);
}

void increase_ccs(void) {
  ccs_rpm++;
}

void decrease_ccs(void) {
  if( ccs_rpm == 0 ){ return; }
  ccs_rpm--;
}

void disable_ccs(void) {
  set_ccs(false);
}

/** CLIMATE CONTROL ***************************************************/

void toggle_heat(void) {
  heat_enabled = !heat_enabled;  /* Toggle the heating condition */
  setLED(RED_LED, heat_enabled); /* Update the red LED. */
}

void increase_heat(void) {
  if( temperature == 99 ){ return; }
  temperature++;
}

void decrease_heat(void) {
  if( temperature == 0 ) { return; }
  temperature--;
}

void toggle_vent(void) {
  vent_enabled = !vent_enabled;     /* Toggle the vent state */
  setLED(YELLOW_LED, vent_enabled); /* Update the yellow LED. */
}

/** WINDOW CONTROL ****************************************************/
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
void step_motor(boolean clockwise) {
  static unsigned char step = 0;
  signed char direction = clockwise ? 1 : -1;

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

void enable_rti(void);

void move_window(boolean up) {
  window_going_up = up;
  enable_rti();
}

void raise_window(void) {
  move_window(true);
}

void lower_window(void) {
  move_window(false);
}


/** MISC CONTROL ******************************************************/
void emergency(void) {
  emergency_stop = true;
}

void finish(void) {
  exit(0);
}

/** TIMING ISRS *****************************************************/
unsigned int rti_ticks = 0;

void enable_rti(void) {
  SETMSK(CRGINT, 0x80); // Enable the timer.
  rti_ticks = 0;        // 1500 ticks = 3 seconds.
}

void disable_rti(void) {
  CLRMSK(CRGINT, 0x80); // Disable the timer.
  CLRMSK(DDRP, MOTOR_ENABLE_MASK);
}

#pragma interrupt_handler RTI
void RTI(void) {
  ++rti_ticks;
  
  if (rti_ticks % 5 == 0) {
  	 step_motor(window_going_up);
  }
  
  if (rti_ticks == 1500) {
	disable_rti();
  }
  
  CRGFLG = 0x80;
}

/** CLOCK FUNCTIONS ****************************************************/
#define TC0_TICK_LENGTH 50000

int uptime_seconds = 0;
int paca_intrs = 0;

#define MAX_RPS 200
#pragma interrupt_handler clock
void clock(void) {
  static unsigned int ticks = 0;
  
  if (++ticks % 5 == 0) {
    ++uptime_seconds;
	
	rps = PACNT;
    PACNT = 0;
	
    if (rps > MAX_RPS) (PWMDTY7)--;
  }
  
  TC0 += TC0_TICK_LENGTH; // Prepare for a new tick.
}

/*#pragma interrupt_handler paca_isr
void paca_isr(void) {
  paca_intrs++;
  PAFLG |= 3;
}*/

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
        keypad_handler_t key_handler = keypad_handlers[table_index];

        /* Additional requirement: Display the character entered. */
        putchar(keypad_characters[table_index]);
		
        /* Execute the key handler, if one is present. */
        if (key_handler != NULL) key_handler();
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

void init(void) {
  // Set up the RTI.
  RTICTL = 0x17;// Generate an. intr. every 2ms.
  
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
  
  
  // Setup the motor
  // Output for all channels is high at beginning 
  // of Period and low when the duty count is reached
  PWMPOL = 0xFF;

  SETMSK(PWMCLK,0x10);		// Select Clock SB for channel 7
  PWMPRCLK = 0x70;		// Prescale ClockB by busclock/128

  PWMSCLA = 0;			// Total Divide: EClock/512
  PWMCAE  = 0xFF;		// Make sure Chan7 is in left aligned output mode
  PWMCTL  = 0x00;		// Combine PWMs 6 and 7.

  PWME = 0x80;			// Enable PWM Channel 7

  //For Motor Direction Control
  SETMSK(DDRP,0x60);
	
  //Setup Pulse Accumulator A for Optical Sensor Input
  SETMSK(PAFLG, 0x03);			// Clear out the interrupt flag
  PACTL  = 0x50;		// Enable PACA
  
  PWMPER7 = 100;
  PWMDTY7 = 0;
  
  INTR_ON();  
  
  // Set up the LCD.
  clearLEDs();
  LCD2PP_Init();
  	
  SETMSK(DDRT, 0x70);			// ???
  CLRMSK(DDRT, 0x80);
  SETMSK(PTT, 0x70);
}

#define ON_OR_OFF(b) ((b) ? " ON" : "OFF")

void update_display(void) {
  char buffer[6];
  const unsigned int seconds = uptime_seconds; // Ensure we have a consistent copy
                                               // of uptime_seconds (ex. it could
										       // change between the two conversions.)
											   
  moveLCDCursor(LCD_LINE_1);
  
  // Update the speed.
  LoadStrLCD("SPEED");
  
  int_to_ascii(rps/*calc_speed(ccs_enabled ? ccs_rpm : rps*60, 1)*/, buffer, ' ', 4);
  LoadStrLCD(buffer);
  
  // Update the CCS.
  LoadStrLCD(" CCS:");
  LoadStrLCD(ON_OR_OFF(ccs_enabled));
  
  moveLCDCursor(LCD_LINE_2);
  
  // Update the heat display.
  LoadStrLCD("H:");
  LoadStrLCD(ON_OR_OFF(heat_enabled));
  LoadStrLCD(" ");
  
  // Update the clock.
  int_to_ascii(seconds / 60, buffer, '0', 3);
  LoadStrLCD(buffer);
	
  LoadStrLCD(":");
	
  int_to_ascii(seconds % 60, buffer, '0', 3);
  LoadStrLCD(buffer);
  
  // Update the temperature.
  LoadStrLCD(" T:");
  int_to_ascii(temperature, buffer, ' ', 3);
  LoadStrLCD(buffer);
}

int main(int argc, char **argv) {
  init();
    
  /* Query for a keypress until the user kills the program. */
  while(!emergency_stop) {
    update_display();
  }
  
  // Display the emergency exit message.
  clearLCD();
  moveLCDCursor(LCD_LINE_1);
  LoadStrLCD("Emergency stop");
  
  SETMSK(DDRK, 0x20);  /* Enable output to the buzzer. */
  SETMSK(PORTK, 0x20); /* Turn on the buzzer. */

  delay();             /* Delay for ~1 second. */

  CLRMSK(PORTK, 0x20); /* Turn off the buzzer. */

  return 0; /* Satisfy the compiler. */
}
