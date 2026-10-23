/*
 * Indicator LEDs: what colour each LED shows, from the lock state the host
 * sends, the active layer and the battery level.
 *
 * Hardware-independent. The firmware describes its LED chain with a table
 * of kb_led_def (which must match the board's keyboard.json indicators),
 * renders it here and writes the colours to the strip.
 *
 * Phase 4 has compiled-in colours; phase 10 makes them settable from the
 * host, and phase 13 supplies the battery level.
 */
#ifndef KB_INDICATORS_H
#define KB_INDICATORS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "kb/keymap.h"

struct kb_rgb {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

enum kb_led_role {
    KB_LED_ROLE_NONE,
    KB_LED_ROLE_LAYER,   /* colour of the highest active layer */
    KB_LED_ROLE_CAPS,    /* Caps Lock */
    KB_LED_ROLE_NUM,     /* Num Lock */
    KB_LED_ROLE_SCROLL,  /* Scroll Lock */
    KB_LED_ROLE_BATTERY, /* one step of a battery gauge, with a level from 1 */
};

struct kb_led_def {
    uint8_t role;  /* enum kb_led_role */
    uint8_t level; /* battery: step k of n lights above (k - 1) * 100 / n percent */
};

#define KB_BATTERY_UNKNOWN 0xFFu

struct kb_indicator_config {
    /* Colour per layer; black leaves the layer LED off on that layer. */
    struct kb_rgb layer[KB_MAX_LAYERS];
    struct kb_rgb caps;
    struct kb_rgb num;
    struct kb_rgb scroll;
    struct kb_rgb battery;
    struct kb_rgb battery_low; /* the whole gauge's colour at or below battery_low_pct */
    uint8_t battery_low_pct;
    /* Scales every colour: 255 is full, 0 is off. */
    uint8_t brightness;
};

struct kb_indicator_state {
    uint8_t locks;   /* HID lock-LED bits (KB_HID_LED_*) */
    uint8_t layer;   /* highest active layer */
    uint8_t battery; /* percent, or KB_BATTERY_UNKNOWN */
    bool asleep;     /* host suspended: everything off */
};

/* Warm defaults: layer 0 dark, Caps a soft white, a low brightness. */
void kb_indicators_default_config(struct kb_indicator_config *cfg);

/*
 * Fill out[0..n-1] for the LED chain described by defs. Returns true if
 * any LED is lit, so the firmware can cut LED power when none is.
 */
bool kb_indicators_render(const struct kb_led_def *defs, size_t n,
                          const struct kb_indicator_config *cfg,
                          const struct kb_indicator_state *state, struct kb_rgb *out);

#endif /* KB_INDICATORS_H */
