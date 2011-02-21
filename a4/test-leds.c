#include <unistd.h>
#include <stdio.h>

unsigned char LEDS[] = {0, 0, 0, 0};

void delay(void) {
  sleep(1);
}

void init(void) {
}

void setLED(char index, unsigned char on) {
  LEDS[index] = on;
  if (on) printf("%d%d%d%d\n", LEDS[0], LEDS[1], LEDS[2], LEDS[3]);
}
