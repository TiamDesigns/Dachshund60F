/*
 * The keyboard loop: scans the matrix at a fixed rate, debounces it, runs
 * the keymap engine from firmware/core and hands reports to USB.
 */
#ifndef KEYBOARD_H
#define KEYBOARD_H

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

/* Saved keymap: save copies live to saved, revert saved to live; factory
 * reset puts the compiled-in default in both. RAM only until phase 6.
 */
void keyboard_save(void);
void keyboard_revert(void);
void keyboard_factory_reset(void);

#endif /* KEYBOARD_H */
