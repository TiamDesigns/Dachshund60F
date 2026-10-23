/*
 * Dachshund60F default keymap, as [layer][row][col] over the full 6x15
 * matrix (positions in boards/Dachshund60F/README.md). Unused matrix
 * positions are XXXXXXX.
 *
 * Must match host-lib/src/default-keymaps.ts, which the mock board and
 * Doghouse use; host-lib's firmware-keymap test compares the two.
 */

#include "default_keymap.h"

const kb_keycode_t default_keymap[DEFAULT_KEYMAP_LAYERS][DEFAULT_KEYMAP_ROWS][DEFAULT_KEYMAP_COLS] = {
    /* 0: Base */
    {
        {KC_ESC, KC_F1, KC_F2, KC_F3, KC_F4, KC_F5, KC_F6, KC_F7, KC_F8, KC_F9, KC_F10, KC_F11, KC_F12, KC_DEL, KC_MUTE},
        {KC_GRV, KC_1, KC_2, KC_3, KC_4, KC_5, KC_6, KC_7, KC_8, KC_9, KC_0, KC_MINS, KC_EQL, KC_BSPC, XXXXXXX},
        {KC_TAB, KC_Q, KC_W, KC_E, KC_R, KC_T, KC_Y, KC_U, KC_I, KC_O, KC_P, KC_LBRC, KC_RBRC, KC_BSLS, XXXXXXX},
        {KC_CAPS, KC_A, KC_S, KC_D, KC_F, KC_G, KC_H, KC_J, KC_K, KC_L, KC_SCLN, KC_QUOT, XXXXXXX, KC_ENT, XXXXXXX},
        {KC_LSFT, KC_Z, KC_X, KC_C, KC_V, KC_B, KC_N, KC_M, KC_COMM, KC_DOT, KC_SLSH, XXXXXXX, KC_RSFT, KC_UP, XXXXXXX},
        {KC_LCTL, KC_LGUI, XXXXXXX, KC_LALT, MO(1), XXXXXXX, KC_SPC, XXXXXXX, KC_RALT, XXXXXXX, MO(1), KC_RCTL, KC_LEFT, KC_DOWN, KC_RGHT},
    },
    /* 1: Fn */
    {
        {QK_BOOT, KC_BRID, KC_BRIU, _______, _______, KC_MPRV, KC_MPLY, KC_MNXT, _______, _______, KC_MUTE, KC_VOLD, KC_VOLU, KC_INS, KC_MPLY},
        {_______, BT_SEL(0), BT_SEL(1), BT_SEL(2), OUT_USB, OUT_2G4, OUT_BLE, _______, _______, _______, BT_CLR, _______, _______, KC_DEL, XXXXXXX},
        {_______, _______, _______, _______, _______, _______, _______, _______, _______, _______, KC_PSCR, KC_SCRL, KC_PAUS, _______, XXXXXXX},
        {_______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, XXXXXXX, _______, XXXXXXX},
        {_______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, XXXXXXX, _______, KC_PGUP, XXXXXXX},
        {_______, _______, XXXXXXX, _______, _______, XXXXXXX, _______, XXXXXXX, _______, XXXXXXX, _______, _______, KC_HOME, KC_PGDN, KC_END},
    },
    /* 2: free for the user */
    {
        {_______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______},
        {_______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, XXXXXXX},
        {_______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, XXXXXXX},
        {_______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, XXXXXXX, _______, XXXXXXX},
        {_______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, XXXXXXX, _______, _______, XXXXXXX},
        {_______, _______, XXXXXXX, _______, _______, XXXXXXX, _______, XXXXXXX, _______, XXXXXXX, _______, _______, _______, _______, _______},
    },
    /* 3: free for the user */
    {
        {_______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______},
        {_______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, XXXXXXX},
        {_______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, XXXXXXX},
        {_______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, XXXXXXX, _______, XXXXXXX},
        {_______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, XXXXXXX, _______, _______, XXXXXXX},
        {_______, _______, XXXXXXX, _______, _______, XXXXXXX, _______, XXXXXXX, _______, XXXXXXX, _______, _______, _______, _______, _______},
    },
};
