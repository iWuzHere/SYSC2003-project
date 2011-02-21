#include <assert.h>


/* Define an unsigned byte type. */
typedef unsigned char byte;

/* Define the boolean type. */
typedef unsigned char boolean;
#define true  1
#define false 0


/* Prototypes of assembly language procedures. */
void init(void);                     /* Initialize Ports and I/O settings. */
void delay(void);                    /* Delay for one second. */
void setLED(char index, boolean on); /* Turn LED at 0-based index on or off. */


/* Version of signum operating on single-byte integral values. *
 * Returns 0 if the value = 0, 1 if the value is > 0, or       *
 * -1 if value < 0.                                            */
static signed char integral_signum(signed char value) {
  if (value == 0) return 0;          /* Avoid divide-by-zero. */
  return (value > 0) ? 1 : -1;
}

/* Constrain the possible values that the floor make take. */
#define MIN_FLOOR 1
#define MAX_FLOOR 4

/* Move an elevator from floor 'start' to 'end', updating the  *
 * indicator LEDs and pausing as appropriate.                  */
static void move_elevator(byte start, byte end) {
  signed char direction = integral_signum(end - start);

  /* Assert that the floor is valid. Disable assertion genertion *
   * using #define NDEBUG.                                       */
  assert(start >= MIN_FLOOR && end >= MIN_FLOOR &&
         start <= MAX_FLOOR && end <= MAX_FLOOR);

  --start;                           /* Offset start and end to be zero-indexed. */
  --end;

  setLED(start, true);               /* Ensure the floor is shown. */

  while (start != end) {
    delay();                         /* Wait ~1 second. */
    setLED(start, false);            /* Disable the last floor. */
    start += direction;              /* Move to the next floor. */
    setLED(start, true);             /* Enable the new indicator. */
  }

  delay();                           /* Wait ~3 seconds more. */
  delay();
  delay();
}

int main(int argc, char **argv) {
  init();

  move_elevator(1, 4);               /* Floor 1 -> Floor 4. */
  move_elevator(4, 2);               /* Floor 4 -> Floor 2. */
  move_elevator(2, 3);               /* Floor 2 -> Floor 3. */
  move_elevator(3, 1);               /* Floor 3 -> Floor 1. */

  return 0;
}
