/*
 * Indicator LEDs on the SK6812 chain: layer, battery gauge and Caps Lock.
 * Colours come from kb/indicators.h; this module drives the strip and the
 * LED power switch. Safe to call from any thread.
 */
#ifndef INDICATORS_H
#define INDICATORS_H

#include <stdbool.h>
#include <stdint.h>

/* Power-on self-test walk along the chain, then live indicators. */
int indicators_start(void);

void indicators_set_locks(uint8_t locks);
void indicators_set_layer(uint8_t layer);
void indicators_set_asleep(bool asleep);

#endif /* INDICATORS_H */
