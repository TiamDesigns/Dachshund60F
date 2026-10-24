/*
 * The keyboard loop: scans the matrix at a fixed rate, debounces it, runs
 * the keymap engine from firmware/core and hands reports to USB.
 */
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdbool.h>
#include <stdint.h>

#include "kb/features.h"
#include "kb/indicators.h"
#include "kb/keymap.h"
#include "kb/protocol_gen.h"
#include "kb/settings.h"

/* Set up the matrix and keymap and start the scan thread. */
int keyboard_start(void);

/*
 * Run a scan soon, even while idle: a host changed something the scan
 * passes on (lock LEDs, a connection, the output). Safe from any thread.
 */
void keyboard_poke(void);

/* A host request: counts as use, so the board doesn't sleep under it. */
void keyboard_note_activity(void);

/* The get_timing counters; reset zeroes them after reading. */
void keyboard_timing(struct kb_proto_get_timing_resp *out, bool reset);

/*
 * The live keymap. Hold keyboard_lock() while changing it, so the scan
 * thread never sees a half-written change.
 */
struct kb_keymap *keyboard_keymap(void);
void keyboard_lock(void);
void keyboard_unlock(void);

/*
 * The live advanced-key tables and the macro buffer the host edits (see
 * kb/protocol.h). Change them under keyboard_lock(), then call
 * keyboard_features_changed() (still locked).
 */
struct kb_features *keyboard_features(void);
uint8_t *keyboard_macro_edit(void);
void keyboard_features_changed(void);

/*
 * The live global settings and indicator colours. Change them under
 * keyboard_lock() (with valid values), then call keyboard_settings_changed()
 * (still locked) to put them into effect.
 */
struct kb_settings *keyboard_settings(void);
struct kb_indicator_config *keyboard_indicators(void);
void keyboard_settings_changed(void);

/*
 * Save writes the live keymap and settings to flash; revert reads them
 * back (or the defaults, if nothing is saved); factory reset erases what's
 * saved and goes back to the compiled-in defaults. Return 0 or a negative
 * errno (-ENOSPC, -EIO).
 */
int keyboard_save(void);
int keyboard_revert(void);
int keyboard_factory_reset(void);

#endif /* KEYBOARD_H */
