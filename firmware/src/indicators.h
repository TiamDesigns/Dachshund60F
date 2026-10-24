/*
 * Indicator LEDs on the SK6812 chain: layer, battery gauge and Caps Lock.
 * Colours come from kb/indicators.h; this module drives the strip and the
 * LED power switch. Safe to call from any thread.
 */
#ifndef INDICATORS_H
#define INDICATORS_H

#include <stdbool.h>
#include <stdint.h>

/* The power-on self-test walk along the chain (if selftest), then live indicators. */
int indicators_start(bool selftest);

void indicators_set_locks(uint8_t locks);
void indicators_set_layer(uint8_t layer);
/* Percent, or KB_BATTERY_UNKNOWN (kb/indicators.h). */
void indicators_set_battery(uint8_t percent);
/* The USB host is suspended: all dark. */
void indicators_set_asleep(bool asleep);
/* Idle on battery: all dark (power management). */
void indicators_set_dark(bool dark);
/* Deep sleep: dark and the LED power off before this returns. Not from an interrupt. */
void indicators_off(void);
struct kb_indicator_config;

/*
 * The colours to show (copied), at a brightness from the settings, 0-100 %.
 * Until the first call the compiled-in defaults show.
 */
void indicators_set_config(const struct kb_indicator_config *cfg, uint8_t brightness_percent);

/* Blink every LED for this many seconds, so the user can spot the board. */
void indicators_identify(uint8_t seconds);

#endif /* INDICATORS_H */
