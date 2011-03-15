// Brendan MacDonell (100777952) And Imran Iqbal (100794182)
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
#define LCD_LINE_2 	   0xC0

/* XXX: WRITE THIS SUBROUTINE IN ASSEMBLY. */
void delay(void);
void LCD2PP_Init(void);

/* Changes the spot you are writing to. */
void DELAY50M(void);
void clearLCD(void);
void updateLCD(byte address, char *s);
void moveLCDCursor(byte address);
void LoadStrLCD( char *s );

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
void setupLCD(void){
	 byte ptt = PTT;
	 byte ptp = PTP;
	 LCD2PP_Init();
 	 PTP = ptp;
	 PTT = ptt;
}

void updateLCD(byte address, char *s) {
	 byte ptt = PTT;
	 byte ptp = PTP;
	 
	 moveLCDCursor(address);
	 LoadStrLCD(s);
	 
 	 PTP = ptp;
	 PTT = ptt; 
}

/** KEYPAD HANDLERS ***************************************************/
#define ROWS 4
#define COLS 4

const char keypad_characters[] = { '1', '2', '3', 'A',
                                   '4', '5', '6', 'B',
                                   '7', '8', '9', 'C',
                                   'E', '0', 'F', 'D' };

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
	    char output_character[2] = "";
	    byte table_index = row * ROWS + col;

        /* Additional requirement: Display the character entered on the LCD. */
		output_character[0] = keypad_characters[table_index];
		updateLCD(LCD_LINE_1, output_character);
    }
  }
	
  /* Restore the serial interface. */
  SPI1CR1 = SPI;
  
  SETMSK(PTM, 0x08);
  PTP   = ptp;  // Restore scan row(s)
  PIFH  = PIFH; 
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
  
  setupLCD();
    
  /* Query for a keypress until the user kills the program. */
  while(true) ;

  return 0; /* Satisfy the compiler. */
}
