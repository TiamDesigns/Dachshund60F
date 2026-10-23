/*
 * Key processing engine: turns debounced key events into HID state.
 *
 * Hardware- and transport-independent. The firmware feeds it key presses
 * and releases with millisecond timestamps (which may wrap) and calls
 * kb_engine_tick() regularly; the engine calls back with the new HID state
 * whenever it changes, and the USB (later BLE or dongle) layer formats and
 * sends the reports.
 *
 * Phase 3 handles: basic keys, modifiers and modded keys (LSFT(KC_1)),
 * consumer and system keys, layers (MO, TG, TO, DF, OSL), one-shot
 * modifiers, layer-tap and mod-tap with a tapping term, and hands firmware
 * actions (QK_BOOT, ...) to a callback.
 *
 * Tap-hold rule: a layer-tap or mod-tap key is a tap if it is released
 * within the tapping term, and a hold once the term has passed. Keys
 * pressed meanwhile wait until that is decided, so they see the right layer
 * and modifiers. Permissive hold and the other options come in phase 9.
 */
#ifndef KB_ENGINE_H
#define KB_ENGINE_H

#include <stdbool.h>
#include <stdint.h>

#include "kb/keycodes.h"
#include "kb/keymap.h"
#include "kb/matrix.h"

#define KB_BOOT_KEYS 6u
#define KB_ENGINE_QUEUE_LEN 16u
#define KB_TAPPING_TERM_DEFAULT_MS 200u

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

struct kb_engine_ops {
    /* The HID state changed. Required. */
    void (*report)(const struct kb_hid_state *hid, void *ctx);
    /* A firmware action key (0x7000 range) was pressed or released. Optional. */
    void (*action)(kb_keycode_t kc, bool pressed, void *ctx);
    /*
     * The active layers changed: a bitmask including the default layer, and
     * the highest active layer. Optional.
     */
    void (*layers)(uint32_t layer_mask, uint8_t highest, void *ctx);
};

struct kb_key_event {
    uint32_t time;
    uint8_t row;
    uint8_t col;
    bool pressed;
};

struct kb_engine {
    const struct kb_keymap *keymap;
    const struct kb_engine_ops *ops;
    void *ctx;
    uint16_t tapping_term_ms;

    /* Layers */
    uint8_t default_layer;
    uint16_t layers_on; /* toggled with TG or set with TO */
    uint8_t layer_holds[KB_MAX_LAYERS]; /* MO, OSL and layer-tap keys holding each layer */
    int8_t oneshot_layer; /* -1 = none */
    uint32_t reported_layers;

    /* One-shot modifiers waiting for the next key, as a HID modifier byte. */
    uint8_t oneshot_mods;

    /* Per key: the keycode it was resolved to when pressed (KC_NO = up). */
    kb_keycode_t active[KB_MATRIX_MAX_ROWS][KB_MATRIX_MAX_COLS];
    /* Per key: one-shot mods it consumed, released with it. */
    uint8_t key_oneshot_mods[KB_MATRIX_MAX_ROWS][KB_MATRIX_MAX_COLS];
    /* Per key: press counter value at its press, to tell a lone tap. */
    uint32_t key_press_seq[KB_MATRIX_MAX_ROWS][KB_MATRIX_MAX_COLS];
    uint32_t press_seq;

    /* Number of keys holding each HID usage (0xE0-0xE7 are the modifiers). */
    uint8_t usage_holds[256];

    struct kb_hid_state hid;
    struct kb_hid_state reported_hid;

    /* Undecided layer-tap or mod-tap key, and the events waiting on it. */
    bool tap_hold_pending;
    struct kb_key_event tap_hold;
    kb_keycode_t tap_hold_kc;
    struct kb_key_event queue[KB_ENGINE_QUEUE_LEN];
    uint8_t queue_len;
};

/* Start with every key up, the default layer 0 and nothing reported. */
void kb_engine_init(struct kb_engine *e, const struct kb_keymap *keymap,
                    const struct kb_engine_ops *ops, void *ctx);

void kb_engine_set_tapping_term(struct kb_engine *e, uint16_t ms);

/* One key changed. Events must come in time order; out-of-range keys are ignored. */
void kb_engine_key(struct kb_engine *e, uint8_t row, uint8_t col, bool pressed, uint32_t now_ms);

/*
 * Feed the debounced matrix: sends an event for every key that differs
 * from prev (presses and releases in row, then column order).
 */
void kb_engine_matrix(struct kb_engine *e, const kb_matrix_row_t *prev,
                      const kb_matrix_row_t *now, uint32_t now_ms);

/* Let time pass: decides a tap-hold key once its tapping term is over. */
void kb_engine_tick(struct kb_engine *e, uint32_t now_ms);

/* Active layers (including the default layer) and the highest of them. */
uint32_t kb_engine_layer_mask(const struct kb_engine *e);
uint8_t kb_engine_highest_layer(const struct kb_engine *e);

static inline const struct kb_hid_state *kb_engine_hid(const struct kb_engine *e)
{
    return &e->hid;
}

#endif /* KB_ENGINE_H */
