/*
 * The Pup's default keymap: its one button types "a", so a test shows each
 * press arriving (and auto-repeat when held). Remap it in Doghouse.
 *
 * Must match host-lib/src/default-keymaps.ts; host-lib's firmware-keymap
 * test compares the two.
 */

#include "default_keymap.h"

const kb_keycode_t default_keymap[DEFAULT_KEYMAP_LAYERS][DEFAULT_KEYMAP_ROWS][DEFAULT_KEYMAP_COLS] = {
    /* 0: Base */
    {
        {KC_A},
    },
    /* 1-3: free for the user */
    {
        {_______},
    },
    {
        {_______},
    },
    {
        {_______},
    },
};

void default_features(struct kb_features *f)
{
    kb_features_default(f);
}
