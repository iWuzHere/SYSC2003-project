// Brendan MacDonell (100777952) And Imran Iqbal (100794182)

/* Define an unsigned byte type. */
typedef unsigned char byte;

/* Define the boolean type. */
typedef unsigned char boolean;
#define true  1
#define false 0

/* Prototypes of assembly language procedures. */
void init(void);                     /* Initialize Ports and I/O settings. */
void delay(int i);                   /* Delay i * 0.125 seconds. */
void setLED(byte index, boolean on); /* Turn LED at 0-based index on or off. */
void set7Segment(char number,boolean on);

int main(int argc, char **argv) {
  signed int i;
  init();

  // Count from 0 to 9 on the 7 seg., waiting ~1 second between increments.
  for (i = 0; i < 10; i++) {
    set7Segment(i, true);
	delay(10);
  }
  
  // Count from 9 to 0 on the 7 seg., waiting ~0.5 seconds between decrements.
  for (i = 9; i >= 0; i--) {
    set7Segment(i, true);
	delay(5);
  }

  return 0;
}