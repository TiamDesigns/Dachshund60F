/*
 * The status LED (RGB). Pairing blinks blue until it ends; the other
 * patterns show once and go dark, so the dongle doesn't glow on the desk.
 */
#ifndef LED_H
#define LED_H

#include <stdbool.h>

enum led_pattern {
    LED_OFF,
    LED_PAIRING,  /* blue, blinking, until LED_OFF or another pattern */
    LED_PAIRED,   /* green, three blinks */
    LED_LINK_UP,  /* green for a second: the keyboard is in range */
    LED_LINK_DOWN, /* red for a second: the keyboard went quiet */
    LED_HELLO,    /* white for a moment at power-on */
};

int led_init(void);

/* Safe from any thread. */
void led_show(enum led_pattern p);

/* The USB host is asleep: dark until it wakes. */
void led_set_asleep(bool asleep);

#endif /* LED_H */
