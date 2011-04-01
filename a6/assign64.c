//Brendan MacDonell (100777952) And Imran Iqbal (100794182)
#include <stdlib.h>
#include <stdio.h>
#include "hcs12dp256.h"

/* Bit-set and clear operations. Could alternatively *
 * use the ASM directive if these are too slow.      */
#define SETMSK(var, msk) ((var) |= (msk))
#define CLRMSK(var, msk) ((var) &= ~(msk))
#define TOGGLEMASK(var, msk) ((var) ^= (msk))

/* Define an unsigned byte type. */
typedef unsigned char byte;

/* Define the boolean type. */
typedef unsigned char boolean;
#define true  1
#define false 0

/** FAN CONTROL     ***************************************************/
#define FAN_ON 100
#define FAN_OFF  0
void fan_on(void){
	PWMDTY4 = FAN_ON;
}

void fan_off(void){
	PWMDTY4 = FAN_OFF;
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
  fan_on,     /* 1 */
  fan_off,    /* 2 */
  NULL,   	  /* 3 */
  NULL,       /* A */
  NULL,   	  /* 4 */
  NULL,  	  /* 5 */
  NULL,   	  /* 6 */
  NULL,       /* B */
  NULL,   	  /* 7 */
  NULL,  	  /* 8 */
  NULL,       /* 9 */
  NULL,       /* C */
  NULL,    	  /* E */
  NULL,    	  /* 0 */
  NULL,       /* F */
  NULL        /* D */
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

  INTR_ON();
  
  //Setup up fan
  PWMPOL = 0xFF;
  PWMCLK = 0x00;
  PWMPRCLK = 0x07;
  PWMCAE &= 0xEF;
  PWMCTL &= 0xF3;

  PWMPER4 = 100;
  PWME = 0x10;
}

int main(void){
	init();
	while(true){};
}