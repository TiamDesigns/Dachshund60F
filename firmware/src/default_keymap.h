/*
 * Compiled-in default keymap: the keymap after a factory reset. This file's
 * sizes and default_keymap.c are the Dachshund60F's; a board with its own
 * (boards/<name>/firmware/, picked by CMakeLists.txt) defines KB_BOARD_KEYMAP
 * and brings board_keymap.h with the sizes and its own default_keymap.c.
 */
#ifndef DEFAULT_KEYMAP_H
#define DEFAULT_KEYMAP_H

#include "kb/features.h"
#include "kb/keycodes.h"

#ifdef KB_BOARD_KEYMAP
#include "board_keymap.h"
#else
#define DEFAULT_KEYMAP_LAYERS 4
#define DEFAULT_KEYMAP_ROWS 6
#define DEFAULT_KEYMAP_COLS 15
#endif

extern const kb_keycode_t
    default_keymap[DEFAULT_KEYMAP_LAYERS][DEFAULT_KEYMAP_ROWS][DEFAULT_KEYMAP_COLS];

/* Default advanced-key tables: none, apart from the encoder mapping. */
void default_features(struct kb_features *f);

#endif /* DEFAULT_KEYMAP_H */
