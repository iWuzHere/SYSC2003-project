#include "hcs12dp256.h"

typedef unsigned char byte;
typedef unsigned char boolean;

/* Bit-set and clear operations. Could alternatively *
 * use the ASM directive if these are too slow.      */
#define SETMSK(var, msk) ((var) |= (msk))
#define CLRMSK(var, msk) ((var) &= ~(msk))
#define TOGGLEMASK(var, msk) ((var) ^= (msk))

#define MOTOR_OUTPUT_MASK 0x60
#define MOTOR_ENABLE_MASK 0x20
#define SEVEN_SEG_EN_MASK 0x04

static const char stepper_motor_values[] = {
  0x60, // Up and left.
  0x40, // Up and right.
  0x00, // Down and right.
  0x20  // Down and left.
};

/* step_motor(int steps, boolean clockwise):                    *
 * Move the stepper motor 'steps' steps in the given direction. */
void step_motor(int steps, boolean clockwise) {
  static unsigned char step = 0;
  const byte ddrp = DDRP, ptp = PTP;
  const byte ptm = PTM;
  const byte ddrt = DDRT, ptt = PTT;
  signed char direction = clockwise ? 1 : -1;

  /* Set the direction and step output. */
  SETMSK(DDRT, MOTOR_OUTPUT_MASK);

  /* Disable the 7-segment display, and enable the stepper motor. */
  CLRMSK(PTM, 0x04);
  SETMSK(DDRP, MOTOR_ENABLE_MASK);
  SETMSK(PTP, MOTOR_ENABLE_MASK);

  while(steps-- > 0) {
    /* Set the value of the motor output. */
    CLRMSK(PTT, MOTOR_OUTPUT_MASK);
    SETMSK(PTT, stepper_motor_values[step]);

    /* Update the step we are on. */
    step = (step + direction) & 0x03;

    delay10ms();
  }

  /* Restore the ports we altered. */
  DDRP = ddrp; PTP = ptp;
  PTM = ptm;
  DDRT = ddrt; PTT = ptt;
}

/* 3 seconds * 1000 ms / second * 1 step / 10ms = 300 steps */
#define step_motor_3s(clkwise) step_motor(300, clkwise)
