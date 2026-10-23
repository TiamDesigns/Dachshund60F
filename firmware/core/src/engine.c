#include "kb/engine.h"

#include <string.h>

#define MOD_USAGE_FIRST KC_LCTL
#define HID_LSHIFT 0x02u

/* Two Shifts as a HID modifier byte. */
#define HID_SHIFTS 0x22u

/* Empty tables for an engine without features (built on first use). */
static bool no_features_ready;
static struct kb_features no_features;

static const struct kb_features *empty_features(void)
{
    if (!no_features_ready) {
        kb_features_default(&no_features);
        no_features_ready = true;
    }
    return &no_features;
}

void kb_hid_boot_report(const struct kb_hid_state *hid, uint8_t out[8])
{
    out[0] = hid->mods;
    out[1] = 0;
    memcpy(&out[2], hid->boot_keys, KB_BOOT_KEYS);
}

static bool after(uint32_t now, uint32_t then, uint32_t ms)
{
    return (uint32_t)(now - then) >= ms;
}

/* Modifier byte folded to one side: bit 0 Ctrl ... bit 3 GUI. */
static uint8_t fold_mods(uint8_t mods)
{
    return (uint8_t)((mods | (mods >> 4)) & 0x0Fu);
}

/* HID usages */

static uint8_t held_mods(const struct kb_engine *e)
{
    uint8_t mods = 0;
    for (uint8_t i = 0; i < 8; i++) {
        if (e->usage_holds[MOD_USAGE_FIRST + i]) {
            mods |= (uint8_t)(1u << i);
        }
    }
    return mods;
}

static void update_mods(struct kb_engine *e)
{
    e->hid.mods = held_mods(e) & (uint8_t)~e->mods_suppressed;
}

static void usage_press(struct kb_engine *e, uint8_t usage)
{
    if (usage == 0 || e->usage_holds[usage]++ != 0) {
        return;
    }

    if (usage >= MOD_USAGE_FIRST) {
        update_mods(e);
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
        update_mods(e);
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

static bool on_layers(const struct kb_engine *e, uint16_t layers)
{
    return layers == 0 || (kb_engine_layer_mask(e) & layers) != 0;
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

/* Mouse keys */

static void mouse_send(struct kb_engine *e, int8_t x, int8_t y, int8_t wheel, int8_t pan)
{
    if (e->ops->mouse) {
        struct kb_mouse_report r = {
            .buttons = e->mouse_buttons, .x = x, .y = y, .wheel = wheel, .pan = pan};
        e->ops->mouse(&r, e->ctx);
    }
}

static int8_t mouse_speed(const struct kb_engine *e)
{
    const struct kb_mouse_config *m = &e->features->mouse;
    uint32_t held = e->now - e->mouse_start;
    if (m->time_to_max_ms == 0 || held >= m->time_to_max_ms) {
        return (int8_t)m->max_speed;
    }
    return (int8_t)(m->move_delta + (uint32_t)(m->max_speed - m->move_delta) * held / m->time_to_max_ms);
}

static void mouse_move(struct kb_engine *e)
{
    int8_t v = mouse_speed(e);
    int8_t x = 0, y = 0;
    if (e->mouse_dirs & 1u) y = (int8_t)(y - v);
    if (e->mouse_dirs & 2u) y = (int8_t)(y + v);
    if (e->mouse_dirs & 4u) x = (int8_t)(x - v);
    if (e->mouse_dirs & 8u) x = (int8_t)(x + v);
    mouse_send(e, x, y, 0, 0);
}

static void wheel_move(struct kb_engine *e)
{
    int8_t wheel = 0, pan = 0;
    if (e->wheel_dirs & 1u) wheel++;
    if (e->wheel_dirs & 2u) wheel--;
    if (e->wheel_dirs & 4u) pan--;
    if (e->wheel_dirs & 8u) pan++;
    mouse_send(e, 0, 0, wheel, pan);
}

static bool mouse_key(struct kb_engine *e, kb_keycode_t kc, bool pressed)
{
    uint8_t bit;
    switch (kc) {
    case MS_BTN1: case MS_BTN2: case MS_BTN3: case MS_BTN4: case MS_BTN5:
        bit = (uint8_t)(1u << (kc - MS_BTN1));
        if (pressed) {
            e->mouse_buttons |= bit;
        } else {
            e->mouse_buttons &= (uint8_t)~bit;
        }
        mouse_send(e, 0, 0, 0, 0);
        return true;
    case MS_UP: case MS_DOWN: case MS_LEFT: case MS_RGHT:
        bit = (uint8_t)(1u << (kc - MS_UP));
        if (pressed) {
            if (e->mouse_dirs == 0) {
                e->mouse_start = e->now;
            }
            e->mouse_dirs |= bit;
            mouse_move(e);
            e->mouse_next = e->now + e->features->mouse.interval_ms;
        } else {
            e->mouse_dirs &= (uint8_t)~bit;
        }
        return true;
    case MS_WHLU: case MS_WHLD: case MS_WHLL: case MS_WHLR:
        bit = (uint8_t)(1u << (kc - MS_WHLU));
        if (pressed) {
            e->wheel_dirs |= bit;
            wheel_move(e);
            e->wheel_next = e->now + e->features->mouse.wheel_interval_ms;
        } else {
            e->wheel_dirs &= (uint8_t)~bit;
        }
        return true;
    default:
        return false;
    }
}

static void mouse_tick(struct kb_engine *e)
{
    if (e->mouse_dirs && (int32_t)(e->now - e->mouse_next) >= 0) {
        mouse_move(e);
        e->mouse_next = e->now + e->features->mouse.interval_ms;
    }
    if (e->wheel_dirs && (int32_t)(e->now - e->wheel_next) >= 0) {
        wheel_move(e);
        e->wheel_next = e->now + e->features->mouse.wheel_interval_ms;
    }
}

/* Keycodes */

static void press_keycode(struct kb_engine *e, uint8_t row, uint8_t col, kb_keycode_t kc);
static void release_keycode(struct kb_engine *e, uint8_t row, uint8_t col);

/* Press and release a keycode with nothing else involved (results, macro steps). */
static void tap_keycode(struct kb_engine *e, kb_keycode_t kc, uint8_t extra_mods)
{
    if (e->active[KB_VROW][KB_VCOL_TAP] != KC_NO) {
        release_keycode(e, KB_VROW, KB_VCOL_TAP);
    }
    mods_press(e, extra_mods);
    press_keycode(e, KB_VROW, KB_VCOL_TAP, kc);
    flush(e);
    release_keycode(e, KB_VROW, KB_VCOL_TAP);
    mods_release(e, extra_mods);
    flush(e);
}

static void vhold_down(struct kb_engine *e, kb_keycode_t kc)
{
    for (uint8_t i = 0; i < KB_VHOLD_SLOTS; i++) {
        if (e->vhold[i] == KC_NO) {
            e->vhold[i] = kc;
            press_keycode(e, KB_VROW, (uint8_t)(KB_VCOL_HOLD + i), kc);
            return;
        }
    }
}

static void vhold_up(struct kb_engine *e, kb_keycode_t kc)
{
    for (uint8_t i = 0; i < KB_VHOLD_SLOTS; i++) {
        if (e->vhold[i] == kc) {
            e->vhold[i] = KC_NO;
            release_keycode(e, KB_VROW, (uint8_t)(KB_VCOL_HOLD + i));
            return;
        }
    }
}

static void vhold_release_all(struct kb_engine *e)
{
    for (uint8_t i = 0; i < KB_VHOLD_SLOTS; i++) {
        if (e->vhold[i] != KC_NO) {
            vhold_up(e, e->vhold[i]);
        }
    }
    flush(e);
}

/* Modifiers a key holds for as long as it is down (each counted once per key). */
static void add_extra_mods(struct kb_engine *e, uint8_t row, uint8_t col, uint8_t mods)
{
    mods &= (uint8_t)~e->key_extra_mods[row][col];
    e->key_extra_mods[row][col] |= mods;
    mods_press(e, mods);
}

/* A key that types something takes the pending one-shot modifiers with it. */
static void take_oneshot_mods(struct kb_engine *e, uint8_t row, uint8_t col)
{
    if (e->oneshot_mods) {
        add_extra_mods(e, row, col, e->oneshot_mods);
        e->oneshot_mods = 0;
    }
}

/* Caps Word: letters get Shift; digits, -, _, Backspace and Delete keep it going. */
static void caps_word_key(struct kb_engine *e, uint8_t row, uint8_t col, uint8_t usage)
{
    if (!e->caps_word || usage >= MOD_USAGE_FIRST) {
        return;
    }
    e->caps_word_last = e->now;
    if (usage >= KC_A && usage <= KC_Z) {
        add_extra_mods(e, row, col, HID_LSHIFT);
    } else if (usage == KC_MINS) {
        add_extra_mods(e, row, col, HID_LSHIFT); /* underscore */
    } else if (!((usage >= KC_1 && usage <= KC_0) || usage == KC_BSPC || usage == KC_DEL)) {
        e->caps_word = false;
    }
}

/* Key overrides: the trigger key with exactly the trigger modifiers held. */
static bool override_key(struct kb_engine *e, uint8_t row, uint8_t col, uint8_t usage)
{
    uint8_t held = fold_mods(held_mods(e));
    if (held == 0) {
        return false;
    }
    for (unsigned i = 0; i < KB_KEY_OVERRIDE_MAX; i++) {
        const struct kb_key_override *o = &e->features->overrides[i];
        if (o->replacement == KC_NO || o->trigger_key != usage ||
            fold_mods(o->trigger_mods) != held || !on_layers(e, o->layers)) {
            continue;
        }
        uint8_t side = fold_mods(o->trigger_mods);
        e->mods_suppressed = held_mods(e) & (uint8_t)(side | (side << 4));
        e->override_key = (struct kb_pos){row, col};
        update_mods(e);
        press_keycode(e, row, col, o->replacement);
        return true;
    }
    return false;
}

/* Keys Auto Shift applies to: letters, digits and the symbol keys. */
static bool auto_shiftable(uint8_t usage)
{
    return (usage >= KC_A && usage <= KC_0) || (usage >= KC_MINS && usage <= KC_SLSH && usage != KC_NUHS);
}

static void dyn_record(struct kb_engine *e, kb_keycode_t kc, bool pressed)
{
    /* The dynamic macro keys themselves aren't part of a recording. */
    if (e->dyn_recording < 0 || (kc >= DM_REC1 && kc <= DM_PLY2)) {
        return;
    }
    uint8_t s = (uint8_t)e->dyn_recording;
    if (e->dyn_len[s] < KB_DYN_MACRO_LEN) {
        e->dyn[s][e->dyn_len[s]++] = (struct kb_dyn_step){kc, pressed};
    }
}

static void dyn_play(struct kb_engine *e, uint8_t slot)
{
    for (uint8_t i = 0; i < e->dyn_len[slot]; i++) {
        const struct kb_dyn_step *st = &e->dyn[slot][i];
        if (st->pressed) {
            vhold_down(e, st->kc);
        } else {
            vhold_up(e, st->kc);
        }
        flush(e);
    }
    /* Anything still down at the end of the recording comes up. */
    vhold_release_all(e);
}

static void macro_run(struct kb_engine *e);

static void macro_start(struct kb_engine *e, uint8_t n)
{
    if (e->macro_off >= 0) {
        return; /* one at a time */
    }
    e->macro_off = kb_macro_offset(e->features->macros, KB_MACRO_BUFFER, n);
    e->macro_waiting = false;
    macro_run(e);
}

static void macro_run(struct kb_engine *e)
{
    const uint8_t *buf = e->features->macros;
    while (e->macro_off >= 0 && e->macro_off < (int)KB_MACRO_BUFFER) {
        if (e->macro_waiting) {
            if ((int32_t)(e->now - e->macro_wait_until) < 0) {
                return;
            }
            e->macro_waiting = false;
        }
        unsigned off = (unsigned)e->macro_off;
        uint8_t op = buf[off];
        if (op == KB_MACRO_END) {
            break;
        }
        if (op >= KB_MACRO_TAP && op <= KB_MACRO_DELAY) {
            if (off + 3 > KB_MACRO_BUFFER) {
                break;
            }
            uint16_t arg = (uint16_t)(buf[off + 1] | (buf[off + 2] << 8));
            e->macro_off += 3;
            switch (op) {
            case KB_MACRO_TAP:
                tap_keycode(e, arg, 0);
                break;
            case KB_MACRO_DOWN:
                vhold_down(e, arg);
                flush(e);
                break;
            case KB_MACRO_UP:
                vhold_up(e, arg);
                flush(e);
                break;
            default:
                e->macro_waiting = true;
                e->macro_wait_until = e->now + arg;
                break;
            }
            continue;
        }
        uint8_t usage;
        bool shift;
        e->macro_off++;
        if (kb_ascii_to_key((char)op, &usage, &shift)) {
            tap_keycode(e, usage, shift ? HID_LSHIFT : 0);
        }
    }
    e->macro_off = -1;
    e->macro_waiting = false;
    /* Keys the macro pressed and didn't release come up at its end. */
    vhold_release_all(e);
}

/* Leader: a captured key, and the sequence it completes or rules out. */
static void leader_finish(struct kb_engine *e, bool timed_out)
{
    for (unsigned i = 0; i < KB_LEADER_MAX; i++) {
        const struct kb_leader_seq *s = &e->features->leader[i];
        if (s->result == KC_NO) {
            continue;
        }
        unsigned len = 0;
        while (len < KB_LEADER_KEYS && s->keys[len]) {
            len++;
        }
        if (len == e->leader_len && memcmp(s->keys, e->leader_keys, len) == 0) {
            e->leader = false;
            tap_keycode(e, s->result, 0);
            return;
        }
    }
    if (timed_out) {
        e->leader = false;
    }
}

static void leader_key(struct kb_engine *e, uint8_t usage)
{
    e->leader_keys[e->leader_len++] = usage;
    e->leader_last = e->now;

    bool longer = false, exact = false;
    for (unsigned i = 0; i < KB_LEADER_MAX; i++) {
        const struct kb_leader_seq *s = &e->features->leader[i];
        if (s->result == KC_NO || memcmp(s->keys, e->leader_keys, e->leader_len) != 0) {
            continue;
        }
        if (e->leader_len < KB_LEADER_KEYS && s->keys[e->leader_len]) {
            longer = true;
        } else {
            exact = true;
        }
    }
    if (exact && !longer) {
        leader_finish(e, true);
    } else if (!exact && !longer) {
        e->leader = false; /* no sequence starts like this */
    } else if (e->leader_len == KB_LEADER_KEYS) {
        leader_finish(e, true);
    }
}

/* Actions the engine runs itself. Returns false for the firmware's. */
static bool engine_action(struct kb_engine *e, kb_keycode_t kc, bool pressed)
{
    if (mouse_key(e, kc, pressed)) {
        return true;
    }
    if (!pressed) {
        return kc == CW_TOGG || kc == QK_LEAD || kc == AS_TOGG || (kc >= DM_REC1 && kc <= DM_PLY2);
    }
    switch (kc) {
    case CW_TOGG:
        e->caps_word = !e->caps_word;
        e->caps_word_last = e->now;
        return true;
    case QK_LEAD:
        e->leader = true;
        e->leader_len = 0;
        e->leader_last = e->now;
        return true;
    case AS_TOGG:
        e->auto_shift = !e->auto_shift;
        return true;
    case DM_REC1:
    case DM_REC2:
        e->dyn_recording = (int8_t)(kc - DM_REC1);
        e->dyn_len[kc - DM_REC1] = 0;
        return true;
    case DM_RSTP:
        e->dyn_recording = -1;
        return true;
    case DM_PLY1:
    case DM_PLY2:
        if (e->dyn_recording < 0) {
            dyn_play(e, (uint8_t)(kc - DM_PLY1));
        }
        return true;
    default:
        return false;
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
        caps_word_key(e, row, col, kb_keycode_key(kc));
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
        if (!engine_action(e, kc, true) && e->ops->action) {
            e->ops->action(kc, true, e->ctx);
        }
        break;
    case KB_KIND_MACRO:
        macro_start(e, (uint8_t)(kc - KB_KC_MACRO));
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
        if (!engine_action(e, kc, false) && e->ops->action) {
            e->ops->action(kc, false, e->ctx);
        }
        break;
    default:
        break;
    }

    if (e->key_extra_mods[row][col]) {
        mods_release(e, e->key_extra_mods[row][col]);
        e->key_extra_mods[row][col] = 0;
    }

    if (e->override_key.row == row && e->override_key.col == col) {
        e->override_key.row = KB_POS_NONE;
        e->mods_suppressed = 0;
        update_mods(e);
    }

    /* Retro tapping: a hold that did nothing sends its tap after all. */
    if (e->key_retro[row][col]) {
        e->key_retro[row][col] = false;
        if (lone) {
            tap_keycode(e, kb_keycode_key(kc), 0);
        }
    }
}

static const struct kb_tap_hold_rule *tap_hold_rule(const struct kb_engine *e, uint8_t row,
                                                    uint8_t col)
{
    for (uint8_t i = 0; i < e->th_key_count; i++) {
        if (e->th_keys[i].row == row && e->th_keys[i].col == col) {
            return &e->th_keys[i];
        }
    }
    return &e->th_default;
}

/* The keycode a key stands for now: the keymap, or a fired combo or encoder detent. */
static kb_keycode_t resolve(const struct kb_engine *e, uint8_t row, uint8_t col)
{
    if (row != KB_VROW) {
        return kb_keymap_resolve(e->keymap, kb_engine_layer_mask(e), row, col);
    }
    if (col < KB_VCOL_COMBO + KB_COMBO_MAX) {
        return e->features->combos[col - KB_VCOL_COMBO].result;
    }
    if (col < KB_VCOL_ENCODER + 2 * KB_ENCODER_MAX) {
        uint8_t enc = (uint8_t)((col - KB_VCOL_ENCODER) / 2);
        bool ccw = ((col - KB_VCOL_ENCODER) & 1u) != 0;
        uint32_t mask = kb_engine_layer_mask(e);
        for (int l = (int)e->keymap->layers - 1; l >= 0; l--) {
            if (!(mask & (1u << l))) {
                continue;
            }
            const struct kb_encoder_action *a = &e->features->encoders[l][enc];
            kb_keycode_t kc = ccw ? a->ccw : a->cw;
            if (kc != KC_TRNS) {
                return kc;
            }
        }
        return KC_NO;
    }
    return KC_NO;
}

/* Events */

static void start_pending(struct kb_engine *e, enum kb_pending_kind kind,
                          const struct kb_key_event *ev, kb_keycode_t kc)
{
    e->pending = kind;
    e->pending_ev = *ev;
    e->pending_kc = kc;
}

static void process(struct kb_engine *e, const struct kb_key_event *ev)
{
    uint8_t row = ev->row, col = ev->col;

    if (!ev->pressed) {
        if (e->active[row][col] != KC_NO) {
            dyn_record(e, e->active[row][col], false);
            release_keycode(e, row, col);
        }
        return;
    }

    if (e->active[row][col] != KC_NO) {
        return; /* already down */
    }

    e->key_press_seq[row][col] = ++e->press_seq;

    kb_keycode_t kc = resolve(e, row, col);
    enum kb_keycode_kind kind = kb_keycode_kind(kc, e->keymap->layers);

    /* A one-shot layer lasts for one key, unless that key is another one-shot. */
    bool oneshot = kind == KB_KIND_OSM ||
                   (kind == KB_KIND_LAYER && ((kc >> 8) & 0xFu) == KB_LAYER_OP_OSL);
    if (!oneshot) {
        e->oneshot_layer = -1;
    }

    /* Leader: basic keys are captured, not typed. */
    if (e->leader && kind == KB_KIND_BASIC && kb_keycode_key(kc) < MOD_USAGE_FIRST) {
        leader_key(e, kb_keycode_key(kc));
        return;
    }

    if (kind == KB_KIND_LT || kind == KB_KIND_MT) {
        const struct kb_tap_hold_rule *rule = tap_hold_rule(e, row, col);
        /* Quick tap: tapped again soon after a tap, it's the tap key at once. */
        if (rule->quick_tap_ms && e->last_tap_valid && e->last_tap_row == row &&
            e->last_tap_col == col && !after(ev->time, e->last_tap_time, rule->quick_tap_ms)) {
            e->last_tap_time = ev->time;
            press_keycode(e, row, col, kb_keycode_key(kc));
            return;
        }
        start_pending(e, KB_PENDING_TAP_HOLD, ev, kc);
        e->tap_hold_rule = *rule;
        return;
    }

    if (kind == KB_KIND_TAP_DANCE) {
        start_pending(e, KB_PENDING_TAP_DANCE, ev, kc);
        e->dance_taps = 1;
        e->dance_held = true;
        e->dance_last = ev->time;
        return;
    }

    if (kc == QK_REP) {
        if (e->repeat_kc != KC_NO) {
            add_extra_mods(e, row, col, e->repeat_mods);
            press_keycode(e, row, col, e->repeat_kc);
        }
        return;
    }

    if (kind == KB_KIND_BASIC) {
        uint8_t usage = kb_keycode_key(kc);
        if (override_key(e, row, col, usage)) {
            return;
        }
        if (e->auto_shift && auto_shiftable(usage) && held_mods(e) == 0 && e->oneshot_mods == 0 &&
            !e->caps_word) {
            start_pending(e, KB_PENDING_AUTO_SHIFT, ev, kc);
            return;
        }
    }

    if ((kind == KB_KIND_BASIC && kb_keycode_key(kc) < MOD_USAGE_FIRST) || kind == KB_KIND_MODDED) {
        e->repeat_kc = kc;
        e->repeat_mods = held_mods(e);
    }
    dyn_record(e, kc, true);
    press_keycode(e, row, col, kc);

    /* Both Shifts down together toggle Caps Word, if set to. */
    if (e->features->caps_word_on_both_shifts && kind == KB_KIND_BASIC &&
        (kc == KC_LSFT || kc == KC_RSFT) && (held_mods(e) & HID_SHIFTS) == HID_SHIFTS) {
        e->caps_word = !e->caps_word;
        e->caps_word_last = e->now;
    }
}

/* Tap dance: what the dance is worth now. */
static kb_keycode_t dance_action(const struct kb_engine *e, bool held)
{
    const struct kb_tap_dance *t = &e->features->tap_dances[e->pending_kc - KB_KC_TAP_DANCE];
    kb_keycode_t kc = KC_NO;
    if (e->dance_taps >= 3) {
        kc = t->tap3 ? t->tap3 : t->tap2;
    } else if (e->dance_taps == 2) {
        kc = t->tap2;
    } else if (held) {
        kc = t->hold1;
    }
    return kc != KC_NO ? kc : t->tap1;
}

static uint16_t dance_term(const struct kb_engine *e)
{
    uint16_t term = e->features->tap_dances[e->pending_kc - KB_KC_TAP_DANCE].term_ms;
    return term ? term : e->th_default.tapping_term_ms;
}

static void decide_dance(struct kb_engine *e)
{
    uint8_t row = e->pending_ev.row, col = e->pending_ev.col;
    kb_keycode_t kc = dance_action(e, e->dance_held);
    e->pending = KB_PENDING_NONE;
    if (e->dance_held) {
        press_keycode(e, row, col, kc);
    } else {
        tap_keycode(e, kc, 0);
    }
    flush(e);
}

static void decide(struct kb_engine *e, bool hold)
{
    uint8_t row = e->pending_ev.row;
    uint8_t col = e->pending_ev.col;
    kb_keycode_t kc = e->pending_kc;

    if (e->pending == KB_PENDING_AUTO_SHIFT) {
        e->pending = KB_PENDING_NONE;
        e->repeat_kc = kc;
        e->repeat_mods = hold ? HID_LSHIFT : 0;
        dyn_record(e, kc, true);
        if (hold) {
            add_extra_mods(e, row, col, HID_LSHIFT);
        }
        press_keycode(e, row, col, kc);
        flush(e);
        return;
    }

    e->pending = KB_PENDING_NONE;
    if (hold) {
        press_keycode(e, row, col, kc);
        e->key_retro[row][col] = (e->tap_hold_rule.flags & KB_TH_RETRO_TAP) != 0;
        e->last_tap_valid = false;
    } else {
        /* The key's release is still queued and releases the tap. */
        press_keycode(e, row, col, kb_keycode_key(kc));
        e->last_tap_valid = true;
        e->last_tap_row = row;
        e->last_tap_col = col;
        e->last_tap_time = e->pending_ev.time;
    }
    flush(e);
}

/* Whether the queued events (or the clock) decide a tap-hold or Auto Shift key: -1 not yet, 0 tap, 1 hold. */
static int lookahead(const struct kb_engine *e)
{
    bool shift = e->pending == KB_PENDING_AUTO_SHIFT;
    const struct kb_tap_hold_rule *rule = &e->tap_hold_rule;
    uint16_t term = shift ? e->features->auto_shift_ms : rule->tapping_term_ms;

    for (uint8_t i = 0; i < e->queue_len; i++) {
        const struct kb_key_event *ev = &e->queue[i];
        bool self = ev->row == e->pending_ev.row && ev->col == e->pending_ev.col;
        if (after(ev->time, e->pending_ev.time, term)) {
            return 1;
        }
        if (self) {
            if (!ev->pressed) {
                return 0;
            }
        } else if (shift) {
            if (ev->pressed) {
                return 0; /* rolling on to the next key: a plain tap */
            }
        } else if (ev->pressed) {
            if (rule->flags & KB_TH_HOLD_ON_OTHER_PRESS) {
                return 1;
            }
        } else if (rule->flags & KB_TH_PERMISSIVE_HOLD) {
            /* Released here: was it pressed after the tap-hold key? */
            for (uint8_t j = 0; j < i; j++) {
                if (e->queue[j].pressed && e->queue[j].row == ev->row && e->queue[j].col == ev->col) {
                    return 1;
                }
            }
        }
    }
    return after(e->now, e->pending_ev.time, term) ? 1 : -1;
}

static void pop(struct kb_engine *e)
{
    e->queue_len--;
    memmove(&e->queue[0], &e->queue[1], e->queue_len * sizeof(e->queue[0]));
}

/* Process queued events until the queue is empty or a key is undecided. */
static void run(struct kb_engine *e)
{
    for (;;) {
        if (e->pending == KB_PENDING_TAP_DANCE) {
            /* The dance consumes its own key's events and ends on the clock or another press. */
            if (e->queue_len > 0) {
                const struct kb_key_event *ev = &e->queue[0];
                bool self = ev->row == e->pending_ev.row && ev->col == e->pending_ev.col;
                if (after(ev->time, e->dance_last, dance_term(e))) {
                    decide_dance(e);
                } else if (self) {
                    e->dance_held = ev->pressed;
                    e->dance_last = ev->time;
                    e->now = ev->time;
                    if (ev->pressed) {
                        e->dance_taps++;
                    }
                    pop(e);
                    if (ev->pressed && e->dance_taps >= 3) {
                        decide_dance(e);
                    }
                    continue;
                } else if (ev->pressed) {
                    decide_dance(e);
                }
                /* A release of another key passes the dance. */
            } else if (after(e->now, e->dance_last, dance_term(e))) {
                decide_dance(e);
            } else {
                return;
            }
        } else if (e->pending != KB_PENDING_NONE) {
            int decision = lookahead(e);
            if (decision < 0) {
                return;
            }
            decide(e, decision == 1);
        }

        if (e->queue_len == 0) {
            return;
        }

        struct kb_key_event ev = e->queue[0];
        pop(e);
        if (ev.time - e->now < 0x80000000u) {
            e->now = ev.time;
        }
        process(e, &ev);
        flush(e);
    }
}

static void enqueue(struct kb_engine *e, const struct kb_key_event *ev)
{
    /* A full queue forces the undecided key. */
    while (e->queue_len == KB_ENGINE_QUEUE_LEN) {
        if (e->pending == KB_PENDING_TAP_DANCE) {
            decide_dance(e);
        } else if (e->pending != KB_PENDING_NONE) {
            decide(e, true);
        }
        run(e);
    }
    e->queue[e->queue_len++] = *ev;
    run(e);
}

/* Combos */

static int combo_key_index(const struct kb_combo *c, uint8_t row, uint8_t col)
{
    for (unsigned k = 0; k < KB_COMBO_KEYS && c->keys[k].row != KB_POS_NONE; k++) {
        if (c->keys[k].row == row && c->keys[k].col == col) {
            return (int)k;
        }
    }
    return -1;
}

static unsigned combo_size(const struct kb_combo *c)
{
    unsigned n = 0;
    while (n < KB_COMBO_KEYS && c->keys[n].row != KB_POS_NONE) {
        n++;
    }
    return n;
}

static bool combo_usable(const struct kb_engine *e, unsigned i)
{
    const struct kb_combo *c = &e->features->combos[i];
    return c->result != KC_NO && e->combo_down[i] == 0 && on_layers(e, c->layers);
}

/* Does combo i contain every buffered key (plus optionally ev)? */
static bool combo_covers(const struct kb_engine *e, unsigned i, const struct kb_key_event *extra)
{
    const struct kb_combo *c = &e->features->combos[i];
    for (uint8_t b = 0; b < e->combo_buf_len; b++) {
        if (combo_key_index(c, e->combo_buf[b].row, e->combo_buf[b].col) < 0) {
            return false;
        }
    }
    return !extra || combo_key_index(c, extra->row, extra->col) >= 0;
}

static uint16_t combo_timeout(const struct kb_engine *e)
{
    uint16_t t = 0;
    for (unsigned i = 0; i < KB_COMBO_MAX; i++) {
        if (combo_usable(e, i) && combo_covers(e, i, NULL)) {
            uint16_t ti = e->features->combos[i].timeout_ms;
            ti = ti ? ti : KB_COMBO_TIMEOUT_DEFAULT_MS;
            t = ti > t ? ti : t;
        }
    }
    return t;
}

/* The combo the buffered keys make exactly, or -1. */
static int combo_exact(const struct kb_engine *e)
{
    for (unsigned i = 0; i < KB_COMBO_MAX; i++) {
        if (combo_usable(e, i) && combo_covers(e, i, NULL) &&
            combo_size(&e->features->combos[i]) == e->combo_buf_len) {
            return (int)i;
        }
    }
    return -1;
}

static void combo_fire(struct kb_engine *e, int i)
{
    const struct kb_combo *c = &e->features->combos[i];
    uint32_t time = e->combo_buf[e->combo_buf_len - 1].time;
    e->combo_down[i] = (uint8_t)((1u << combo_size(c)) - 1u);
    e->combo_buf_len = 0;
    struct kb_key_event ev = {.time = time, .row = KB_VROW, .col = (uint8_t)(KB_VCOL_COMBO + i), .pressed = true};
    enqueue(e, &ev);
}

/* Give up on a combo: the buffered presses go on as ordinary keys. */
static void combo_flush(struct kb_engine *e)
{
    struct kb_key_event buf[KB_COMBO_KEYS];
    uint8_t n = e->combo_buf_len;
    memcpy(buf, e->combo_buf, sizeof(buf));
    e->combo_buf_len = 0;
    for (uint8_t b = 0; b < n; b++) {
        enqueue(e, &buf[b]);
    }
}

/* Resolve the buffer: fire the exact combo if there is one, else flush. */
static void combo_resolve(struct kb_engine *e)
{
    int i = combo_exact(e);
    if (i >= 0) {
        combo_fire(e, i);
    } else {
        combo_flush(e);
    }
}

static bool combo_starts(const struct kb_engine *e, const struct kb_key_event *ev)
{
    for (unsigned i = 0; i < KB_COMBO_MAX; i++) {
        if (combo_usable(e, i) && combo_key_index(&e->features->combos[i], ev->row, ev->col) >= 0) {
            return true;
        }
    }
    return false;
}

static void combo_stage(struct kb_engine *e, const struct kb_key_event *ev)
{
    if (e->combo_buf_len > 0 && after(ev->time, e->combo_buf[0].time, combo_timeout(e))) {
        combo_resolve(e);
    }

    if (!ev->pressed) {
        /* A key of a fired combo: the first one up releases the combo. */
        for (unsigned i = 0; i < KB_COMBO_MAX; i++) {
            int k = combo_key_index(&e->features->combos[i], ev->row, ev->col);
            if (e->combo_down[i] && k >= 0 && (e->combo_down[i] & (1u << k))) {
                bool first = e->combo_down[i] == (uint8_t)((1u << combo_size(&e->features->combos[i])) - 1u);
                e->combo_down[i] &= (uint8_t)~(1u << k);
                if (first) {
                    struct kb_key_event up = {.time = ev->time, .row = KB_VROW,
                                              .col = (uint8_t)(KB_VCOL_COMBO + i), .pressed = false};
                    enqueue(e, &up);
                }
                return;
            }
        }
        for (uint8_t b = 0; b < e->combo_buf_len; b++) {
            if (e->combo_buf[b].row == ev->row && e->combo_buf[b].col == ev->col) {
                combo_resolve(e);
                /* Fired: this release belongs to the combo now. */
                if (e->combo_buf_len == 0) {
                    combo_stage(e, ev);
                    return;
                }
            }
        }
        enqueue(e, ev);
        return;
    }

    if (e->combo_buf_len == 0) {
        if (combo_starts(e, ev)) {
            e->combo_buf[e->combo_buf_len++] = *ev;
        } else {
            enqueue(e, ev);
        }
        return;
    }

    bool candidate = false, bigger = false;
    for (unsigned i = 0; i < KB_COMBO_MAX; i++) {
        if (combo_usable(e, i) && combo_covers(e, i, ev)) {
            candidate = true;
            if (combo_size(&e->features->combos[i]) > (unsigned)e->combo_buf_len + 1) {
                bigger = true;
            }
        }
    }
    if (!candidate || e->combo_buf_len == KB_COMBO_KEYS) {
        combo_resolve(e);
        combo_stage(e, ev);
        return;
    }
    e->combo_buf[e->combo_buf_len++] = *ev;
    if (!bigger) {
        combo_fire(e, combo_exact(e));
    }
}

/* API */

void kb_engine_init(struct kb_engine *e, const struct kb_keymap *keymap,
                    const struct kb_engine_ops *ops, void *ctx)
{
    memset(e, 0, sizeof(*e));
    e->keymap = keymap;
    e->ops = ops;
    e->ctx = ctx;
    e->features = empty_features();
    e->th_default.tapping_term_ms = KB_TAPPING_TERM_DEFAULT_MS;
    e->oneshot_layer = -1;
    e->override_key.row = KB_POS_NONE;
    e->macro_off = -1;
    e->dyn_recording = -1;
    e->reported_layers = kb_engine_layer_mask(e);
}

void kb_engine_set_features(struct kb_engine *e, const struct kb_features *features)
{
    /*
     * The tables may have changed under state that refers to them: let
     * waiting combo keys through as plain presses, release fired combos,
     * and stop a playing macro.
     */
    if (e->combo_buf_len > 0) {
        combo_flush(e);
    }
    for (unsigned i = 0; i < KB_COMBO_MAX; i++) {
        if (e->combo_down[i]) {
            e->combo_down[i] = 0;
            struct kb_key_event up = {.time = e->now, .row = KB_VROW,
                                      .col = (uint8_t)(KB_VCOL_COMBO + i), .pressed = false};
            enqueue(e, &up);
        }
    }
    if (e->macro_off >= 0) {
        e->macro_off = -1;
        e->macro_waiting = false;
        vhold_release_all(e);
    }

    e->features = features ? features : empty_features();
    e->th_keys = e->features->tap_hold;
    e->th_key_count = KB_TAP_HOLD_RULE_MAX;
    e->auto_shift = e->features->auto_shift;
}

void kb_engine_set_tapping_term(struct kb_engine *e, uint16_t ms)
{
    e->th_default.tapping_term_ms = ms;
}

void kb_engine_set_tap_hold(struct kb_engine *e, const struct kb_tap_hold_rule *def,
                            const struct kb_tap_hold_rule *keys, uint8_t count)
{
    if (def) {
        e->th_default = *def;
    }
    e->th_keys = keys;
    e->th_key_count = keys ? count : 0;
}

void kb_engine_key(struct kb_engine *e, uint8_t row, uint8_t col, bool pressed, uint32_t now_ms)
{
    if (row >= e->keymap->rows || col >= e->keymap->cols) {
        return;
    }
    struct kb_key_event ev = {.time = now_ms, .row = row, .col = col, .pressed = pressed};
    e->now = now_ms; /* the clock never runs behind the latest event */
    combo_stage(e, &ev);
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

void kb_engine_encoder(struct kb_engine *e, uint8_t encoder, bool clockwise, uint32_t now_ms)
{
    if (encoder >= KB_ENCODER_MAX) {
        return;
    }
    uint8_t col = (uint8_t)(KB_VCOL_ENCODER + encoder * 2 + (clockwise ? 0 : 1));
    e->now = now_ms;
    struct kb_key_event ev = {.time = now_ms, .row = KB_VROW, .col = col, .pressed = true};
    enqueue(e, &ev);
    ev.pressed = false;
    enqueue(e, &ev);
}

void kb_engine_tick(struct kb_engine *e, uint32_t now_ms)
{
    e->now = now_ms;

    if (e->combo_buf_len > 0 && after(now_ms, e->combo_buf[0].time, combo_timeout(e))) {
        combo_resolve(e);
    }
    if (e->pending != KB_PENDING_NONE) {
        run(e);
    }
    if (e->caps_word && e->features->caps_word_idle_ms &&
        after(now_ms, e->caps_word_last, e->features->caps_word_idle_ms)) {
        e->caps_word = false;
    }
    if (e->leader && after(now_ms, e->leader_last, e->features->leader_timeout_ms)) {
        leader_finish(e, true);
    }
    if (e->macro_off >= 0) {
        macro_run(e);
    }
    mouse_tick(e);
    flush(e);
}
