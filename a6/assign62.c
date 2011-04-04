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

void delay(void);

int     temperature  = 0;
boolean heat_enabled = false;

#define MIN_TEMPERATURE 100

/** CLIMATE CONTROL ***************************************************/
#pragma interrupt_handler check_temperature
void check_temperature(void){
  temperature = ((ATD0DR6 & 0x03FF))/8 - 5;
}

void init(void) {
  SETMSK(DDRM, 0x80);

  // This sets up the temperature sensor thing
  ATD0CTL2 = 0xFA; // Enables ATD
  ATD0CTL3 = 0x00; // Continue conversions
  ATD0CTL4 = 0x60; // Select 10-bit operation
  						  // Set sample time to 18 ATD clock period
						  // Set clock prescale to 0
  ATD0CTL5 = 0x86; // Right justified, Unsigned and single scan
  
  INTR_ON();  
}

int main(int argc, char **argv) {
  init();
    
  /* Query for a keypress until the user kills the program. */
  while (true) {
	// Enable another pass on the ATD.
	ATD0CTL5 = 0x86;
	
    while (!(ATD0STAT1 & 0x40)) ; // Wait for the conversion to complete.
	
	// Print the temperature.
    printf("%d F\n", temperature);
    if(temperature <= MIN_TEMPERATURE)      SETMSK(PTM, 0x80);
	else if (temperature > MIN_TEMPERATURE) CLRMSK(PTM, 0x80);
  }

  return 0; /* Satisfy the compiler. */
}
