/* SYSC2003 Assignment 4.1
 *
 * Brendan MacDonell (100777952) and Imran Iqbal (100794182)
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


/* XXX: WRITE THIS SUBROUTINE IN ASSEMBLY. */
void delay(void);

int putcharSc0(char);

/* Work around the linker issue which prevents putchar from *
 * being written in ASM instead of C.                       */
int putchar(char c) {
  return putcharSc0(c);
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
int rpm = 0;

void accelerate(void) {
  rpm++;
}

void brake(void) {
  rpm--;
}

/** CRUISE CONTROL ****************************************************/
boolean ccs_enabled = false;
int ccs_rpm = 0;

void set_ccs(boolean enabled) {
  ccs_enabled = enabled;      /* Update the CCS condition. */
  setLED(GREEN_LED, enabled); /* Update the green LED. */
}

void toggle_ccs(void) {
  set_ccs(!ccs_enabled);
}

void increase_ccs(void) {
  ccs_rpm++;
}

void decrease_ccs(void) {
  ccs_rpm--;
}

void disable_ccs(void) {
  set_ccs(false);
}

/** CLIMATE CONTROL ***************************************************/
boolean heat_enabled = false;
byte    temperature = 22;
boolean vent_enabled = false;

void toggle_heat(void) {
  heat_enabled = !heat_enabled;  /* Toggle the heating condition */
  setLED(RED_LED, heat_enabled); /* Update the red LED. */
}

void increase_heat(void) {
  temperature++;
}

void decrease_heat(void) {
  temperature--;
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

/* Track keys pressed after each iteration, so we don't produce multiple *
 * actions for a single keypress. Initialized to all be false.           */
boolean key_was_pressed[sizeof(keypad_handlers) / sizeof(keypad_handler_t)] = { false };

void keypress(void) {
  byte row;
  byte SPI = SPI1CR1;

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
      if (is_pressed && !key_was_pressed[table_index]) {
        keypad_handler_t key_handler = keypad_handlers[table_index];

        /* Execute the key handler, if one is present. */
        if (key_handler != NULL) key_handler();
 
        /* Additional requirement: Display the character entered. */
        putchar(keypad_characters[table_index]);
      }

      key_was_pressed[table_index] = is_pressed;
    }
  }
	
  /* Restore the serial interface. */
  SPI1CR1 = SPI;
}

int main(int argc, char **argv) {
  clearLEDs();

  /* Query for a keypress until the user kills the program. */
  while(true) {
    keypress();
  }

  return 0; /* Satisfy the compiler. */
}
