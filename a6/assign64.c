/* Integrated Car Control System
 *
 * Brendan MacDonell (100777952) And Imran Iqbal (100794182)
 *
 * Interpretation of instructions:
 * 1. Heating attempts to reach an undisplayed set point, which defaults to
 *    20 C, when the heat is on; the set point may be adjusted using the '5'
 *    and '8' keys. The fahrenheit display and _fixed_ set point from assignment
 *    62 are discarded.
 * 2. The current car speed is displayed in km / h, based on the value read from
 *    the pulse accumulator. This speed is based on a presumed wheel radius of
 *    24 cm.
 * 3. The accelerate / brake keys increase or decrease the duty cycle.
 *    They don't offer the same granularity as the CCS. 
 * 4. Similarly to the heating system, the CCS maintains a target RPM set point.
 *    This set point is copied from the current RPS when the CCS is enabled, and
 *    can be adjusted with the '4' and '7' keys. The set point is not displayed 
 *    on the the LCD panel. Additionally, each adjustment increases or decreases
 *    by one RPM, up to a maximum of 30RPS.
 * 5. There is no 'Fan on' or 'Fan off' buttons, only the 'Fan toggle' button
 *    described in the specification.
 */
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

#define FAN_ON 100
#define FAN_OFF  0

#define WHEEL_SIZE  24
// 2 * PI * 60
#define PI_2_60    377

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
int     ccs_rpm = 0;             // Fulfill silly dictation of representation.
boolean heat_enabled = false;
int     target_temperature = 20; // In Celsius.
int     temperature = 0;
boolean vent_enabled = false;
boolean emergency_stop = false;
boolean window_going_up;

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
    return ((long)RPM) * PI_2_60 * wheelR / (10000 * 10);
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
#define MAX_DUTY_CYCLE   30
#define MAX_RPM        1800
#define MIN_DUTY_CYCLE    0

void accelerate(void) {
  if( (PWMDTY7) < MAX_DUTY_CYCLE) (PWMDTY7)++;
}

void brake(void) {
  if( (PWMDTY7) > MIN_DUTY_CYCLE) (PWMDTY7)--;
}

/** CRUISE CONTROL ****************************************************/


void set_ccs(boolean enabled) {
  ccs_enabled = enabled;       /* Update the CCS condition. */
  ccs_rpm     = rps * 60;      /* Copy the current speed. */
  setLED(YELLOW_LED, enabled); /* Update the green LED. */
}

void toggle_ccs(void) {
  set_ccs(!ccs_enabled);
}

void increase_ccs(void) {
  if (ccs_rpm >= MAX_RPM) return;
  ccs_rpm++;
}

void decrease_ccs(void) {
  if (ccs_rpm == 0) return;
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
  if (target_temperature == 99) return;
  target_temperature++;
}

void decrease_heat(void) {
  if (target_temperature == 0) return;
  target_temperature--;
}

void toggle_vent(void) {
  vent_enabled = !vent_enabled;    /* Toggle the vent state */
  setLED(GREEN_LED, vent_enabled); /* Update the yellow LED. */
  
  if (vent_enabled) PWMDTY4 = FAN_ON;
  else              PWMDTY4 = FAN_OFF;
}

/** HEATER CONTROL ****************************************************/
#pragma interrupt_handler check_temperature
void check_temperature(void){
	temperature = (((ATD0DR6 & 0x03FF)) - 296) * 5 / 72;
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

#pragma interrupt_handler clock
void clock(void) {
  static unsigned int ticks = 0;
  
  if (++ticks % 5 == 0) {
    ++uptime_seconds;
	
	rps = PACNT;
    PACNT = 0;
	
	// Only slow every 2 seconds to stop the motor from slowing down too fast
	if (ccs_enabled) {
	  unsigned rpm = rps * 60;
	  if (rpm < ccs_rpm)                                 (PWMDTY7)++;
	  else if (uptime_seconds % 2 == 0 && rpm > ccs_rpm) (PWMDTY7)--;
	}
  }

  // Only read from the temperature every two seconds, to minimize
  // the number of interrupts produced.
  if( ticks % 10 == 0 ) {
	ATD0CTL5 = 0x86;
  }
  
  TC0 += TC0_TICK_LENGTH; // Prepare for a new tick.
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
  
  // Setup the motor and fan.
  // Output for all channels is high at beginning 
  // of Period and low when the duty count is reached
  PWMPOL = 0xFF;
  
  SETMSK(PWMCLK,0x10);	// Select Clock SB for channel 7
  PWMPRCLK = 0x77;		// Prescale Clock A and B by busclock/128

  PWMSCLA = 0;			// Total Divide: EClock/512
  PWMCAE  = 0xFF;		// Make sure Chan7 is in left aligned output mode
  PWMCTL  = 0x00;		// Combine PWMs 6 and 7.

  PWME = 0x90;			// Enable PWM Channels 7 and 4.

  //For Motor Direction Control
  SETMSK(DDRP,0x60);
	
  //Setup Pulse Accumulator A for Optical Sensor Input
  SETMSK(PAFLG, 0x03);			// Clear out the interrupt flag
  PACTL  = 0x50;		// Enable PACA
  
  // Set up the period and duty cycle of the motor.
  PWMPER7 = 100;
  PWMDTY7 = 0;
  
  // Set up the period and duty cycle of the fan.
  PWMPER4 = 100;
  PWMDTY4 = FAN_OFF;
  
  // Allow the heater to work?
  SETMSK(DDRM, 0x80);
  
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
  	
  SETMSK(DDRT, 0x70);			// This keeps the accumulator from messing up
  CLRMSK(DDRT, 0x80);			// Actually lets us read from the optical sensor
  SETMSK(PTT, 0x70); 			// This keeps the accumulator from messing up
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
  
  int_to_ascii(calc_speed(rps * 60, WHEEL_SIZE), buffer, ' ', 4);
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
  LoadStrLCD("T:");
  int_to_ascii(temperature, buffer, ' ', 3);
  LoadStrLCD(buffer);
  LoadStrLCD("C");
}

int main(int argc, char **argv) {
  init();
    
  /* Query for a keypress until the user kills the program. */
  while(!emergency_stop) {
    update_display();
	
	if(temperature <= target_temperature && heat_enabled) SETMSK(PTM, 0x80);
	else                                                  CLRMSK(PTM, 0x80);
  }
  
  // Display the emergency exit message.
  clearLCD();
  moveLCDCursor(LCD_LINE_1);
  LoadStrLCD("Emergency stop");
  
  PWMDTY7 = 0; // Stop the car's motor.
  PWME    = 0;
  
  ATD0CTL2 = 0; // Turn off the ATD
  
  SETMSK(DDRK, 0x20);  /* Enable output to the buzzer. */
  SETMSK(PORTK, 0x20); /* Turn on the buzzer. */

  delay();             /* Delay for ~1 second. */

  CLRMSK(PORTK, 0x20); /* Turn off the buzzer. */

  return 0; /* Satisfy the compiler. */
}