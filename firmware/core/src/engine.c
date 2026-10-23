#include "kb/engine.h"

#include <string.h>

#define MOD_USAGE_FIRST KC_LCTL

void kb_hid_boot_report(const struct kb_hid_state *hid, uint8_t out[8])
{
    out[0] = hid->mods;
    out[1] = 0;
    memcpy(&out[2], hid->boot_keys, KB_BOOT_KEYS);
}

/* HID usages */

static void usage_press(struct kb_engine *e, uint8_t usage)
{
    if (usage == 0 || e->usage_holds[usage]++ != 0) {
        return;
    }

    if (usage >= MOD_USAGE_FIRST) {
        e->hid.mods |= (uint8_t)(1u << (usage - MOD_USAGE_FIRST));
        return;
    }

    e->hid.keys[usage >> 3] |= (uint8_t)(1u << (usage & 7u));
    /* Boot report: keep press order; a seventh key only reaches NKRO. */
    for (unsigned i = 0; i < KB_BOOT_KEYS; i++) {
        if (e->hid.boot_keys[i] == 0) {
            e->hid.boot_keys[i] = usage;
            break;
        }
    }
}

static void usage_release(struct kb_engine *e, uint8_t usage)
{
    if (usage == 0 || e->usage_holds[usage] == 0 || --e->usage_holds[usage] != 0) {
        return;
    }

    if (usage >= MOD_USAGE_FIRST) {
        e->hid.mods &= (uint8_t)~(1u << (usage - MOD_USAGE_FIRST));
        return;
    }

    e->hid.keys[usage >> 3] &= (uint8_t)~(1u << (usage & 7u));
    for (unsigned i = 0; i < KB_BOOT_KEYS; i++) {
        if (e->hid.boot_keys[i] == usage) {
            memmove(&e->hid.boot_keys[i], &e->hid.boot_keys[i + 1], KB_BOOT_KEYS - 1 - i);
            e->hid.boot_keys[KB_BOOT_KEYS - 1] = 0;
            break;
        }
    }
}

/* mods is a HID modifier byte. */
static void mods_press(struct kb_engine *e, uint8_t mods)
{
    for (uint8_t i = 0; i < 8; i++) {
        if (mods & (1u << i)) {
            usage_press(e, (uint8_t)(MOD_USAGE_FIRST + i));
        }
    }
}

static void mods_release(struct kb_engine *e, uint8_t mods)
{
    for (uint8_t i = 0; i < 8; i++) {
        if (mods & (1u << i)) {
            usage_release(e, (uint8_t)(MOD_USAGE_FIRST + i));
        }
    }
}

/* Layers */

uint32_t kb_engine_layer_mask(const struct kb_engine *e)
{
    uint32_t mask = e->layers_on | (1u << e->default_layer);
    for (uint8_t l = 0; l < KB_MAX_LAYERS; l++) {
        if (e->layer_holds[l]) {
            mask |= 1u << l;
        }
    }
    if (e->oneshot_layer >= 0) {
        mask |= 1u << e->oneshot_layer;
    }
    return mask & ((1u << e->keymap->layers) - 1u);
}

uint8_t kb_engine_highest_layer(const struct kb_engine *e)
{
    uint32_t mask = kb_engine_layer_mask(e);
    uint8_t highest = 0;
    for (uint8_t l = 0; l < KB_MAX_LAYERS; l++) {
        if (mask & (1u << l)) {
            highest = l;
        }
    }
    return highest;
}

static void layer_hold(struct kb_engine *e, uint8_t layer)
{
    if (layer < KB_MAX_LAYERS && e->layer_holds[layer] < UINT8_MAX) {
        e->layer_holds[layer]++;
    }
}

static void layer_unhold(struct kb_engine *e, uint8_t layer)
{
    if (layer < KB_MAX_LAYERS && e->layer_holds[layer] > 0) {
        e->layer_holds[layer]--;
    }
}

/* Report HID and layer changes. */
static void flush(struct kb_engine *e)
{
    if (memcmp(&e->hid, &e->reported_hid, sizeof(e->hid)) != 0) {
        e->reported_hid = e->hid;
        e->ops->report(&e->hid, e->ctx);
    }

    uint32_t layers = kb_engine_layer_mask(e);
    if (layers != e->reported_layers) {
        e->reported_layers = layers;
        if (e->ops->layers) {
            e->ops->layers(layers, kb_engine_highest_layer(e), e->ctx);
        }
    }
}

/* Keycodes */

/* A key that types something takes the pending one-shot modifiers with it. */
static void take_oneshot_mods(struct kb_engine *e, uint8_t row, uint8_t col)
{
    if (e->oneshot_mods) {
        e->key_oneshot_mods[row][col] = e->oneshot_mods;
        mods_press(e, e->oneshot_mods);
        e->oneshot_mods = 0;
    }
}

static void press_keycode(struct kb_engine *e, uint8_t row, uint8_t col, kb_keycode_t kc)
{
    enum kb_keycode_kind kind = kb_keycode_kind(kc, e->keymap->layers);
    uint8_t layer = kb_keycode_key(kc);

    e->active[row][col] = kc;

    switch (kind) {
    case KB_KIND_BASIC:
        take_oneshot_mods(e, row, col);
        usage_press(e, kb_keycode_key(kc));
        break;
    case KB_KIND_MODDED:
        take_oneshot_mods(e, row, col);
        mods_press(e, kb_mods_to_hid(kb_keycode_mods(kc)));
        usage_press(e, kb_keycode_key(kc));
        break;
    case KB_KIND_CONSUMER:
        take_oneshot_mods(e, row, col);
        e->hid.consumer = kc & 0x3FFu;
        break;
    case KB_KIND_SYSTEM:
        take_oneshot_mods(e, row, col);
        e->hid.system = kb_keycode_key(kc);
        break;
    case KB_KIND_LAYER:
        switch ((kc >> 8) & 0xFu) {
        case KB_LAYER_OP_MO:
        case KB_LAYER_OP_OSL:
            layer_hold(e, layer);
            break;
        case KB_LAYER_OP_TG:
            e->layers_on ^= (uint16_t)(1u << layer);
            break;
        case KB_LAYER_OP_TO:
            e->layers_on = (uint16_t)(1u << layer);
            break;
        case KB_LAYER_OP_DF:
            e->default_layer = layer;
            break;
        default:
            break;
        }
        break;
    case KB_KIND_OSM:
        mods_press(e, kb_mods_to_hid((uint8_t)(kc & 0x1Fu)));
        break;
    case KB_KIND_LT:
        /* Decided as a hold. */
        layer_hold(e, (uint8_t)((kc >> 8) & 0xFu));
        break;
    case KB_KIND_MT:
        mods_press(e, kb_mods_to_hid(kb_keycode_mods(kc)));
        break;
    case KB_KIND_ACTION:
        if (e->ops->action) {
            e->ops->action(kc, true, e->ctx);
        }
        break;
    default:
        e->active[row][col] = KC_NO;
        break;
    }
}

static void release_keycode(struct kb_engine *e, uint8_t row, uint8_t col)
{
    kb_keycode_t kc = e->active[row][col];
    bool lone = e->key_press_seq[row][col] == e->press_seq;
    uint8_t layer = kb_keycode_key(kc);

    e->active[row][col] = KC_NO;

    switch (kb_keycode_kind(kc, e->keymap->layers)) {
    case KB_KIND_BASIC:
        usage_release(e, kb_keycode_key(kc));
        break;
    case KB_KIND_MODDED:
        usage_release(e, kb_keycode_key(kc));
        mods_release(e, kb_mods_to_hid(kb_keycode_mods(kc)));
        break;
    case KB_KIND_CONSUMER:
        if (e->hid.consumer == (kc & 0x3FFu)) {
            e->hid.consumer = 0;
        }
        break;
    case KB_KIND_SYSTEM:
        if (e->hid.system == kb_keycode_key(kc)) {
            e->hid.system = 0;
        }
        break;
    case KB_KIND_LAYER:
        if (((kc >> 8) & 0xFu) == KB_LAYER_OP_MO) {
            layer_unhold(e, layer);
        } else if (((kc >> 8) & 0xFu) == KB_LAYER_OP_OSL) {
            layer_unhold(e, layer);
            /* Tapped on its own: the layer stays on for the next key. */
            if (lone) {
                e->oneshot_layer = (int8_t)layer;
            }
        }
        break;
    case KB_KIND_OSM:
        mods_release(e, kb_mods_to_hid((uint8_t)(kc & 0x1Fu)));
        if (lone) {
            e->oneshot_mods |= kb_mods_to_hid((uint8_t)(kc & 0x1Fu));
        }
        break;
    case KB_KIND_LT:
        layer_unhold(e, (uint8_t)((kc >> 8) & 0xFu));
        break;
    case KB_KIND_MT:
        mods_release(e, kb_mods_to_hid(kb_keycode_mods(kc)));
        break;
    case KB_KIND_ACTION:
        if (e->ops->action) {
            e->ops->action(kc, false, e->ctx);
        }
        break;
    default:
        break;
    }

    if (e->key_oneshot_mods[row][col]) {
        mods_release(e, e->key_oneshot_mods[row][col]);
        e->key_oneshot_mods[row][col] = 0;
    }
}

/* Events */

static void process(struct kb_engine *e, const struct kb_key_event *ev)
{
    if (!ev->pressed) {
        if (e->active[ev->row][ev->col] != KC_NO) {
            release_keycode(e, ev->row, ev->col);
        }
        return;
    }

    if (e->active[ev->row][ev->col] != KC_NO) {
        return; /* already down */
    }

    e->key_press_seq[ev->row][ev->col] = ++e->press_seq;

    kb_keycode_t kc = kb_keymap_resolve(e->keymap, kb_engine_layer_mask(e), ev->row, ev->col);
    enum kb_keycode_kind kind = kb_keycode_kind(kc, e->keymap->layers);

    /* A one-shot layer lasts for one key, unless that key is another one-shot. */
    bool oneshot = kind == KB_KIND_OSM ||
                   (kind == KB_KIND_LAYER && ((kc >> 8) & 0xFu) == KB_LAYER_OP_OSL);
    if (!oneshot) {
        e->oneshot_layer = -1;
    }

    if (kind == KB_KIND_LT || kind == KB_KIND_MT) {
        e->tap_hold_pending = true;
        e->tap_hold = *ev;
        e->tap_hold_kc = kc;
        return;
    }

    press_keycode(e, ev->row, ev->col, kc);
}

static void decide_tap_hold(struct kb_engine *e, bool hold)
{
    uint8_t row = e->tap_hold.row;
    uint8_t col = e->tap_hold.col;

    e->tap_hold_pending = false;
    if (hold) {
        press_keycode(e, row, col, e->tap_hold_kc);
    } else {
        /* The key's release is still queued and releases the tap. */
        press_keycode(e, row, col, kb_keycode_key(e->tap_hold_kc));
    }
    flush(e);
}

/* Process queued events until the queue is empty or a tap-hold key is undecided. */
static void run(struct kb_engine *e)
{
    for (;;) {
        if (e->tap_hold_pending) {
            int decision = -1; /* -1 undecided, 0 tap, 1 hold */
            for (uint8_t i = 0; i < e->queue_len && decision < 0; i++) {
                const struct kb_key_event *ev = &e->queue[i];
                if ((uint32_t)(ev->time - e->tap_hold.time) >= e->tapping_term_ms) {
                    decision = 1;
                } else if (!ev->pressed && ev->row == e->tap_hold.row &&
                           ev->col == e->tap_hold.col) {
                    decision = 0;
                }
            }
            if (decision < 0) {
                return;
            }
            decide_tap_hold(e, decision == 1);
        }

        if (e->queue_len == 0) {
            return;
        }

        struct kb_key_event ev = e->queue[0];
        e->queue_len--;
        memmove(&e->queue[0], &e->queue[1], e->queue_len * sizeof(e->queue[0]));
        process(e, &ev);
        flush(e);
    }
}

void kb_engine_init(struct kb_engine *e, const struct kb_keymap *keymap,
                    const struct kb_engine_ops *ops, void *ctx)
{
    memset(e, 0, sizeof(*e));
    e->keymap = keymap;
    e->ops = ops;
    e->ctx = ctx;
    e->tapping_term_ms = KB_TAPPING_TERM_DEFAULT_MS;
    e->oneshot_layer = -1;
    e->reported_layers = kb_engine_layer_mask(e);
}

void kb_engine_set_tapping_term(struct kb_engine *e, uint16_t ms)
{
    e->tapping_term_ms = ms;
}

void kb_engine_key(struct kb_engine *e, uint8_t row, uint8_t col, bool pressed, uint32_t now_ms)
{
    if (row >= e->keymap->rows || col >= e->keymap->cols) {
        return;
    }

    /* A full queue forces the undecided key to a hold. */
    while (e->queue_len == KB_ENGINE_QUEUE_LEN) {
        if (e->tap_hold_pending) {
            decide_tap_hold(e, true);
        }
        run(e);
    }

    e->queue[e->queue_len++] = (struct kb_key_event){
        .time = now_ms, .row = row, .col = col, .pressed = pressed};
    run(e);
}

void kb_engine_matrix(struct kb_engine *e, const kb_matrix_row_t *prev,
                      const kb_matrix_row_t *now, uint32_t now_ms)
{
    for (uint8_t r = 0; r < e->keymap->rows; r++) {
        kb_matrix_row_t diff = prev[r] ^ now[r];
        for (uint8_t c = 0; diff != 0 && c < e->keymap->cols; c++) {
            kb_matrix_row_t bit = (kb_matrix_row_t)1u << c;
            if (diff & bit) {
                diff &= ~bit;
                kb_engine_key(e, r, c, (now[r] & bit) != 0, now_ms);
            }
        }
    }
}

void kb_engine_tick(struct kb_engine *e, uint32_t now_ms)
{
    if (e->tap_hold_pending &&
        (uint32_t)(now_ms - e->tap_hold.time) >= e->tapping_term_ms) {
        decide_tap_hold(e, true);
        run(e);
    }
}
