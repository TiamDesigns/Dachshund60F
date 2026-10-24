/*
 * The rotary encoder's contacts, from encoder-a-gpios and encoder-b-gpios
 * in the board's devicetree (zephyr,user), sampled on the scan thread.
 */
#ifndef ENCODER_INPUT_H
#define ENCODER_INPUT_H

/* Configure the pins. Returns 0, or a negative errno. A board without an encoder has 0 of them. */
int encoder_input_init(void);

/* How many encoders the board has (0 or 1). */
int encoder_input_count(void);

/* Sample encoder i: +1 for a detent clockwise, -1 counter-clockwise, else 0. */
int encoder_input_poll(int i);

/*
 * Wake on a turn while the scan is idle: interrupts on both contacts
 * leaving the level they rest at, calling fn from the interrupt once. The
 * interrupt disarms itself. Set fn before arming.
 */
void encoder_input_set_wake_handler(void (*fn)(void));
void encoder_input_wake_arm(void);
void encoder_input_wake_disarm(void);

#endif /* ENCODER_INPUT_H */
