/*
 * Advanced key features the engine runs from tables: combos, tap dance, key
 * overrides, Leader sequences, macros, encoder mapping, and the settings of
 * Caps Word, Auto Shift and mouse keys. The firmware owns one of these
 * (loaded from flash, edited over the protocol) and hands it to the engine.
 *
 * Every table has a fixed size so the whole thing can live in RAM and be
 * saved as is. An entry with result KC_NO (or no keys) is unused.
 */
#ifndef KB_FEATURES_H
#define KB_FEATURES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "kb/keycodes.h"

#define KB_COMBO_MAX 16u
#define KB_COMBO_KEYS 4u
#define KB_TAP_DANCE_MAX 16u
#define KB_KEY_OVERRIDE_MAX 16u
#define KB_LEADER_MAX 16u
#define KB_LEADER_KEYS 4u
#define KB_TAP_HOLD_RULE_MAX 16u
#define KB_ENCODER_MAX 2u
#define KB_MACRO_BUFFER 1024u
#define KB_MACRO_MAX 32u

/* A key position; row 0xFF marks an unused slot. */
struct kb_pos {
    uint8_t row;
    uint8_t col;
};

#define KB_POS_NONE 0xFFu

/*
 * Pressing every key of a combo within timeout_ms acts as result. Keys are
 * matrix positions (the first unused slot ends the list); layers is a mask
 * of layers the combo works on (0 = all).
 */
struct kb_combo {
    struct kb_pos keys[KB_COMBO_KEYS];
    kb_keycode_t result;
    uint16_t timeout_ms; /* 0 = KB_COMBO_TIMEOUT_DEFAULT_MS */
    uint16_t layers;
};

#define KB_COMBO_TIMEOUT_DEFAULT_MS 50u

/*
 * TD(n): tapped once, twice or three times (each tap within term_ms of the
 * last), or tapped then held (hold on the final press). KC_NO actions fall
 * back to the one-tap action.
 */
struct kb_tap_dance {
    kb_keycode_t tap1;
    kb_keycode_t hold1;
    kb_keycode_t tap2;
    kb_keycode_t tap3;
    uint16_t term_ms; /* 0 = the default tapping term */
};

/*
 * With exactly the trigger modifiers held (as a HID modifier byte, either
 * side counts), the trigger key sends replacement instead, without those
 * modifiers. E.g. Shift + Backspace = Delete.
 */
struct kb_key_override {
    uint8_t trigger_mods;
    uint8_t trigger_key; /* HID usage */
    kb_keycode_t replacement;
    uint16_t layers; /* 0 = all */
};

/* QK_LEAD, then these keys (basic usages, 0 ends the list), taps result. */
struct kb_leader_seq {
    uint8_t keys[KB_LEADER_KEYS];
    kb_keycode_t result;
};

/* Per layer and encoder: what a detent clockwise and counter-clockwise taps. */
struct kb_encoder_action {
    kb_keycode_t cw;
    kb_keycode_t ccw;
};

/*
 * Macro buffer: macros one after another, each a list of steps ending with
 * KB_MACRO_END. M(n) plays the n-th. Steps:
 *   KB_MACRO_TAP kc_lo kc_hi       press and release a keycode
 *   KB_MACRO_DOWN kc_lo kc_hi      press a keycode (released at the end if no UP)
 *   KB_MACRO_UP kc_lo kc_hi        release a keycode
 *   KB_MACRO_DELAY ms_lo ms_hi     wait
 *   0x20-0x7E                      type that ASCII character (US layout)
 */
#define KB_MACRO_END 0x00u
#define KB_MACRO_TAP 0x01u
#define KB_MACRO_DOWN 0x02u
#define KB_MACRO_UP 0x03u
#define KB_MACRO_DELAY 0x04u

/* How a tap-hold key decides (see kb/engine.h). In a per-key list, row
 * KB_POS_NONE marks an unused slot. */
#define KB_TH_PERMISSIVE_HOLD 0x01u
#define KB_TH_HOLD_ON_OTHER_PRESS 0x02u
#define KB_TH_RETRO_TAP 0x04u
#define KB_TH_FLAGS_ALL 0x07u

struct kb_tap_hold_rule {
    uint8_t row;
    uint8_t col;
    uint8_t flags;           /* KB_TH_* */
    uint16_t tapping_term_ms;
    uint16_t quick_tap_ms;   /* 0 = off */
};

/* Mouse keys: speed in pixels (or wheel steps) per report. */
struct kb_mouse_config {
    uint8_t interval_ms;     /* time between movement reports */
    uint8_t move_delta;      /* starting speed */
    uint8_t max_speed;       /* top speed */
    uint16_t time_to_max_ms; /* acceleration time */
    uint8_t wheel_interval_ms;
};

struct kb_features {
    struct kb_combo combos[KB_COMBO_MAX];
    struct kb_tap_dance tap_dances[KB_TAP_DANCE_MAX];
    struct kb_key_override overrides[KB_KEY_OVERRIDE_MAX];
    struct kb_tap_hold_rule tap_hold[KB_TAP_HOLD_RULE_MAX];
    struct kb_leader_seq leader[KB_LEADER_MAX];
    uint16_t leader_timeout_ms; /* per key */

    bool caps_word_on_both_shifts; /* both Shifts toggle Caps Word */
    uint16_t caps_word_idle_ms;    /* 0 = never times out */

    bool auto_shift;               /* on at power-up; AS_TOGG flips it */
    uint16_t auto_shift_ms;

    struct kb_mouse_config mouse;

    /* encoders[layer][encoder]; KC_TRNS falls through to lower layers. */
    struct kb_encoder_action encoders[16][KB_ENCODER_MAX];

    uint8_t macros[KB_MACRO_BUFFER];
};

/* Empty tables and default settings. */
void kb_features_default(struct kb_features *f);

/*
 * Whether every keycode and index in f is valid for a board with these
 * dimensions. The firmware checks this before using tables from the host
 * or from flash.
 */
bool kb_features_valid(const struct kb_features *f, uint8_t layers, uint8_t rows, uint8_t cols);

/* Offset of macro n in the buffer, or -1 if there is no such macro. */
int kb_macro_offset(const uint8_t *buf, uint16_t size, uint8_t n);

/* Whether the macro buffer is well formed (every step whole, valid keycodes). */
bool kb_macros_valid(const uint8_t *buf, uint16_t size, uint8_t layers);

/*
 * One table entry in the byte layout of protocol.yaml's feature_tables
 * (tables KB_PROTO_TABLE_*), for the protocol and for saving.
 */

/* Entries table has on a keymap with this many layers; 0 for no such table. */
uint16_t kb_feature_entries(uint8_t table, uint8_t layers);

/* Entry size in bytes; 0 for no such table. */
uint8_t kb_feature_entry_size(uint8_t table);

/* Write entry index of table into out (kb_feature_entry_size bytes). False if there's no such entry. */
bool kb_feature_get(const struct kb_features *f, uint8_t table, uint16_t index, uint8_t *out);

/*
 * Replace entry index of table from len bytes; a shorter entry keeps the
 * current values of the fields it leaves out. Nothing is validated beyond
 * the table and index (check the result with kb_features_valid). False if
 * there's no such entry or len is longer than the entry.
 */
bool kb_feature_put(struct kb_features *f, uint8_t table, uint16_t index, const uint8_t *in,
                    size_t len);

/* US-layout HID usage and Shift for an ASCII character, or false. */
bool kb_ascii_to_key(char c, uint8_t *usage, bool *shift);

#endif /* KB_FEATURES_H */
