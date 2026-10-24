/*
 * What the keyboard's USB device (firmware/src/kb_usb.c) calls back into.
 * On the keyboard these reach the scan loop, the indicators and the
 * battery gauge; here they reach the radio and the status LED.
 */

#include "indicators.h"
#include "keyboard.h"
#include "power.h"

#include "led.h"
#include "radio.h"

/* The host changed something the keyboard should hear about (lock LEDs, its state). */
void keyboard_poke(void)
{
    radio_status_changed();
}

void indicators_set_asleep(bool asleep)
{
    led_set_asleep(asleep);
}

/* No battery here. */
void power_poke(void)
{
}
