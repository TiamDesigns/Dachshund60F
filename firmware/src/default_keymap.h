/*
 * Compiled-in default keymap for the Dachshund60F: the keymap after a
 * factory reset, and the one the firmware runs until settings storage
 * arrives in phase 6.
 */
#ifndef DEFAULT_KEYMAP_H
#define DEFAULT_KEYMAP_H

#include "kb/features.h"
#include "kb/keycodes.h"

#define DEFAULT_KEYMAP_LAYERS 4
#define DEFAULT_KEYMAP_ROWS 6
#define DEFAULT_KEYMAP_COLS 15

extern const kb_keycode_t
    default_keymap[DEFAULT_KEYMAP_LAYERS][DEFAULT_KEYMAP_ROWS][DEFAULT_KEYMAP_COLS];

/* Default advanced-key tables: none, apart from the encoder mapping. */
void default_features(struct kb_features *f);

#endif /* DEFAULT_KEYMAP_H */
