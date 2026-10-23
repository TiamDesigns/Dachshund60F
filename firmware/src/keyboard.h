/*
 * The keyboard loop: scans the matrix at a fixed rate, debounces it, runs
 * the keymap engine from firmware/core and hands reports to USB.
 */
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

#include "kb/features.h"
#include "kb/keymap.h"

/* Set up the matrix and keymap and start the scan thread. */
int keyboard_start(void);

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
 * Save writes the live keymap and settings to flash; revert reads them
 * back (or the defaults, if nothing is saved); factory reset erases what's
 * saved and goes back to the compiled-in defaults. Return 0 or a negative
 * errno (-ENOSPC, -EIO).
 */
int keyboard_save(void);
int keyboard_revert(void);
int keyboard_factory_reset(void);

#endif /* KEYBOARD_H */
