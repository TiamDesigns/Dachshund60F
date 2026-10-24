/*
 * Key processing engine: turns debounced key events into HID state.
 *
 * Hardware- and transport-independent. The firmware feeds it key presses
 * and releases with millisecond timestamps (which may wrap) and calls
 * kb_engine_tick() regularly; the engine calls back with the new HID state
 * whenever it changes, and the USB (later BLE or dongle) layer formats and
 * sends the reports.
 *
 * It handles basic keys, modifiers and modded keys (LSFT(KC_1)), consumer
 * and system keys, layers (MO, TG, TO, DF, OSL), one-shot modifiers,
 * layer-tap and mod-tap, and from the tables in kb/features.h: combos, tap
 * dance, key overrides, Leader sequences, macros (stored and recorded),
 * Caps Word, Auto Shift, the Repeat key, mouse keys and encoder mapping.
 * Other firmware actions (QK_BOOT, connection keys, ...) go to a callback.
 *
 * Order of processing: a key event first meets the combo stage (keys that
 * belong to a combo wait there until the combo fires or can't), then the
 * tap-hold stage (events wait while a tap-hold, Auto Shift or tap dance
 * key is undecided), then the keymap. Encoders and fired combos enter as
 * events of a virtual key row, so they take part in tap-hold decisions.
 *
 * Tap-hold rule: a layer-tap or mod-tap key is a tap if it is released
 * within the tapping term, and a hold once the term has passed. Keys
 * pressed meanwhile wait until that is decided, so they see the right layer
 * and modifiers. Options, globally or per key (struct kb_tap_hold_rule):
 * - permissive hold: another key pressed and released while the tap-hold
 *   key is down makes it a hold, even within the term;
 * - hold on other key press: any other key pressed makes it a hold;
 * - quick tap: pressed again within quick_tap_ms of a tap, it taps at once
 *   (so holding it auto-repeats the tap key);
 * - retro tapping: held past the term and released with no other key
 *   pressed meanwhile, it still sends its tap.
 */
#ifndef KB_ENGINE_H
#define KB_ENGINE_H

#include <stdbool.h>
#include <stdint.h>

#include "kb/features.h"
#include "kb/keycodes.h"
#include "kb/keymap.h"
#include "kb/matrix.h"

#define KB_BOOT_KEYS 6u
#define KB_ENGINE_QUEUE_LEN 16u
#define KB_TAPPING_TERM_DEFAULT_MS 200u
#define KB_DYN_MACRO_LEN 64u

/* Everything a keyboard HID report needs, independent of the transport. */
struct kb_hid_state {
    /* HID modifier byte: bit 0 Left Ctrl ... bit 7 Right GUI. */
    uint8_t mods;
    /* Up to six held keys in press order, for the boot report (0 = empty). */
    uint8_t boot_keys[KB_BOOT_KEYS];
    /* Every held key usage (not modifiers) as a bitmap, for NKRO. */
    uint8_t keys[32];
    /* Held consumer usage (page 0x0C) and system usage (page 0x01), 0 = none. */
    uint16_t consumer;
    uint16_t system;
};

/* 8-byte boot-protocol keyboard report: mods, reserved, six keys. */
void kb_hid_boot_report(const struct kb_hid_state *hid, uint8_t out[8]);

static inline bool kb_hid_key_held(const struct kb_hid_state *hid, uint8_t usage)
{
    return (hid->keys[usage >> 3] >> (usage & 7u)) & 1u;
}

/* A mouse report: buttons held, and movement since the last report. */
struct kb_mouse_report {
    uint8_t buttons; /* bit 0 = button 1 */
    int8_t x;
    int8_t y;
    int8_t wheel; /* positive = up */
    int8_t pan;   /* positive = right */
};

struct kb_engine_ops {
    /* The HID state changed. Required. */
    void (*report)(const struct kb_hid_state *hid, void *ctx);
    /* A firmware action key (0x7000 range) the engine doesn't run itself. Optional. */
    void (*action)(kb_keycode_t kc, bool pressed, void *ctx);
    /*
     * The active layers changed: a bitmask including the default layer, and
     * the highest active layer. Optional.
     */
    void (*layers)(uint32_t layer_mask, uint8_t highest, void *ctx);
    /* A mouse report (buttons changed, or movement). Optional. */
    void (*mouse)(const struct kb_mouse_report *report, void *ctx);
};

struct kb_key_event {
    uint32_t time;
    uint8_t row;
    uint8_t col;
    bool pressed;
};

/* The virtual key row: fired combos, encoder detents and internal taps. */
#define KB_VROW KB_MATRIX_MAX_ROWS
#define KB_VCOL_COMBO 0u                                 /* + combo index */
#define KB_VCOL_ENCODER (KB_VCOL_COMBO + KB_COMBO_MAX)   /* + encoder * 2 + ccw */
#define KB_VCOL_TAP (KB_VCOL_ENCODER + 2 * KB_ENCODER_MAX)
#define KB_VCOL_HOLD (KB_VCOL_TAP + 1)                   /* + slot */
#define KB_VHOLD_SLOTS 8u
#define KB_ENGINE_ROWS (KB_MATRIX_MAX_ROWS + 1)

enum kb_pending_kind {
    KB_PENDING_NONE,
    KB_PENDING_TAP_HOLD,
    KB_PENDING_AUTO_SHIFT,
    KB_PENDING_TAP_DANCE,
};

struct kb_dyn_step {
    kb_keycode_t kc;
    bool pressed;
};

struct kb_engine {
    const struct kb_keymap *keymap;
    const struct kb_engine_ops *ops;
    void *ctx;
    /* Feature tables; never NULL (an empty set by default). Not copied. */
    const struct kb_features *features;
    /* Tap-hold rules: the default, and per-key exceptions (not copied). */
    struct kb_tap_hold_rule th_default;
    const struct kb_tap_hold_rule *th_keys;
    uint8_t th_key_count;

    /* Layers */
    uint8_t default_layer;
    uint16_t layers_on; /* toggled with TG or set with TO */
    uint8_t layer_holds[KB_MAX_LAYERS]; /* MO, OSL and layer-tap keys holding each layer */
    int8_t oneshot_layer; /* -1 = none */
    uint32_t reported_layers;

    /* One-shot modifiers waiting for the next key, as a HID modifier byte. */
    uint8_t oneshot_mods;

    /* Per key: the keycode it was resolved to when pressed (KC_NO = up). */
    kb_keycode_t active[KB_ENGINE_ROWS][KB_MATRIX_MAX_COLS];
    /* Per key: extra modifiers it pressed (one-shot, Caps Word, Auto Shift), released with it. */
    uint8_t key_extra_mods[KB_ENGINE_ROWS][KB_MATRIX_MAX_COLS];
    /* Per key: press counter value at its press, to tell a lone tap. */
    uint32_t key_press_seq[KB_ENGINE_ROWS][KB_MATRIX_MAX_COLS];
    uint32_t press_seq;
    /* Per key: retro tapping applies on release (a hold with the flag). */
    bool key_retro[KB_ENGINE_ROWS][KB_MATRIX_MAX_COLS];

    /* Number of keys holding each HID usage (0xE0-0xE7 are the modifiers). */
    uint8_t usage_holds[256];
    /* Modifiers a key override hides while its replacement is held, and that key. */
    uint8_t mods_suppressed;
    struct kb_pos override_key;

    struct kb_hid_state hid;
    struct kb_hid_state reported_hid;

    /* Combo stage: presses waiting to become a combo, and fired combos' keys still down. */
    struct kb_key_event combo_buf[KB_COMBO_KEYS];
    uint8_t combo_buf_len;
    uint8_t combo_down[KB_COMBO_MAX]; /* bit k: keys[k] still held */

    /* Tap-hold stage: the undecided key and the events waiting on it. */
    enum kb_pending_kind pending;
    struct kb_key_event pending_ev;
    kb_keycode_t pending_kc;
    struct kb_tap_hold_rule tap_hold_rule;
    uint8_t dance_taps;     /* taps so far */
    bool dance_held;        /* the dance key is down */
    uint32_t dance_last;    /* time of its last press or release */
    struct kb_key_event queue[KB_ENGINE_QUEUE_LEN];
    uint8_t queue_len;

    /* The last tap-hold key decided as a tap, for quick tap. */
    bool last_tap_valid;
    uint8_t last_tap_row, last_tap_col;
    uint32_t last_tap_time;

    uint32_t now; /* time of the latest event or tick */

    /* Caps Word */
    bool caps_word;
    uint32_t caps_word_last;

    /* Auto Shift (on unless toggled off; the tables say whether it starts on). */
    bool auto_shift;

    /* Repeat key: the last key sent, with the modifiers held then. */
    kb_keycode_t repeat_kc;
    uint8_t repeat_mods;

    /* Leader */
    bool leader;
    uint8_t leader_keys[KB_LEADER_KEYS];
    uint8_t leader_len;
    uint32_t leader_last;

    /* Stored macro being played: offset of its next step, or -1. */
    int macro_off;
    uint32_t macro_wait_until;
    bool macro_waiting;

    /* Keycodes held by macros and playback, by virtual hold slot. */
    kb_keycode_t vhold[KB_VHOLD_SLOTS];

    /* Dynamic macros: recording slot (-1 none) and the two recordings. */
    int8_t dyn_recording;
    struct kb_dyn_step dyn[2][KB_DYN_MACRO_LEN];
    uint8_t dyn_len[2];

    /* Mouse keys */
    uint8_t mouse_buttons;
    uint8_t mouse_dirs;  /* bit 0 up, 1 down, 2 left, 3 right */
    uint8_t wheel_dirs;  /* bit 0 up, 1 down, 2 left, 3 right */
    uint32_t mouse_start;
    uint32_t mouse_next;
    uint32_t wheel_next;
};

/* Start with every key up, the default layer 0, no feature tables and nothing reported. */
void kb_engine_init(struct kb_engine *e, const struct kb_keymap *keymap,
                    const struct kb_engine_ops *ops, void *ctx);

/*
 * Feature tables (combos, tap dance, ...); they must outlive the engine's
 * use of them and be valid (kb_features_valid). Also sets the per-key
 * tap-hold rules and Auto Shift's state from them. NULL clears them. Call
 * it again after changing the tables: keys waiting to become a combo go on
 * as plain keys, fired combos are released and a playing macro stops.
 */
void kb_engine_set_features(struct kb_engine *e, const struct kb_features *features);

/* The default tapping term (the default rule's). */
void kb_engine_set_tapping_term(struct kb_engine *e, uint16_t ms);

/*
 * The default tap-hold rule, and per-key rules that replace it for their
 * keys (NULL and 0 for none). The list must outlive the engine's use of it.
 */
void kb_engine_set_tap_hold(struct kb_engine *e, const struct kb_tap_hold_rule *def,
                            const struct kb_tap_hold_rule *keys, uint8_t count);

/* One key changed. Events must come in time order; out-of-range keys are ignored. */
void kb_engine_key(struct kb_engine *e, uint8_t row, uint8_t col, bool pressed, uint32_t now_ms);

/*
 * Feed the debounced matrix: sends an event for every key that differs
 * from prev (presses and releases in row, then column order).
 */
void kb_engine_matrix(struct kb_engine *e, const kb_matrix_row_t *prev,
                      const kb_matrix_row_t *now, uint32_t now_ms);

/* An encoder moved one detent: taps its mapped keycode on the active layer. */
void kb_engine_encoder(struct kb_engine *e, uint8_t encoder, bool clockwise, uint32_t now_ms);

/*
 * Let time pass: decides undecided keys and combos, times out Caps Word and
 * Leader, plays macro delays and moves the mouse. Call it at least every
 * few milliseconds.
 */
void kb_engine_tick(struct kb_engine *e, uint32_t now_ms);

/*
 * Nothing waits on time: no undecided key or combo, no Caps Word or Leader
 * timeout, no macro playing and no mouse movement. The firmware may then
 * stop calling kb_engine_tick until the next key event (held keys that
 * wait on nothing, such as a plain letter or a layer key, don't count).
 */
bool kb_engine_idle(const struct kb_engine *e);

/* Active layers (including the default layer) and the highest of them. */
uint32_t kb_engine_layer_mask(const struct kb_engine *e);
uint8_t kb_engine_highest_layer(const struct kb_engine *e);

static inline const struct kb_hid_state *kb_engine_hid(const struct kb_engine *e)
{
    return &e->hid;
}

static inline bool kb_engine_caps_word(const struct kb_engine *e)
{
    return e->caps_word;
}

#endif /* KB_ENGINE_H */
