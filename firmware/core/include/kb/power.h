/*
 * Power states: when the board stops scanning, turns its lights out,
 * stops advertising and goes into deep sleep.
 *
 * Everything counts from the last time the board was in use: a key held or
 * changing, the engine waiting on a timer (tap-hold, combos, macros, mouse
 * keys...), or a host request (kb_power_activity). The firmware calls
 * kb_power_update() after every scan and does what the status says:
 *
 *   ACTIVE  scan every period
 *   IDLE    stop the scan timer and wait for a key interrupt; the first
 *           scan runs as soon as a key goes down, so idling adds no delay
 *   OFF     deep sleep (System OFF on the nRF52840); a key wakes the board
 *           with a reset
 *
 * On USB power the board never sleeps and keeps its lights and radio on.
 * On battery, lights_off and radio_off say when to turn the indicators
 * off and stop Bluetooth advertising (a connected host stays connected),
 * and a critically low cell sends the board to sleep at once.
 *
 * Millisecond timestamps may wrap; the firmware updates at least every
 * next_ms, which is far less than the 49 days of a wrap.
 */
#ifndef KB_POWER_H
#define KB_POWER_H

#include <stdbool.h>
#include <stdint.h>

enum kb_power_state {
    KB_POWER_ACTIVE,
    KB_POWER_IDLE,
    KB_POWER_OFF,
};

struct kb_power_config {
    uint32_t idle_ms;          /* quiet time before the scan stops; 0 = never */
    uint32_t lights_off_ms;    /* on battery, before the lights go out; 0 = never */
    uint32_t radio_off_ms;     /* on battery, before advertising stops; 0 = never */
    uint8_t sleep_timeout_min; /* on battery, before deep sleep; 0 = never */
};

struct kb_power_inputs {
    bool quiet;            /* nothing held, nothing pending */
    bool usb_power;        /* USB powers the board */
    bool battery_critical; /* the cell is too low to keep going */
};

struct kb_power_status {
    enum kb_power_state state;
    bool lights_off;
    bool radio_off;
    /* Milliseconds until the status changes by time alone; UINT32_MAX for never. */
    uint32_t next_ms;
};

struct kb_power {
    struct kb_power_config cfg;
    uint32_t last_activity;
};

/* Start as if the board was just used. */
void kb_power_init(struct kb_power *p, const struct kb_power_config *cfg, uint32_t now_ms);

/* New thresholds (a settings change); the quiet time so far still counts. */
void kb_power_configure(struct kb_power *p, const struct kb_power_config *cfg);

/* The board was used now (a host request, a resume). */
void kb_power_activity(struct kb_power *p, uint32_t now_ms);

/* After each scan: what the board should do now. */
struct kb_power_status kb_power_update(struct kb_power *p, const struct kb_power_inputs *in,
                                       uint32_t now_ms);

#endif /* KB_POWER_H */
