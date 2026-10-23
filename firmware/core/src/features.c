#include "kb/features.h"

#include <string.h>

#include "kb/keymap.h"

void kb_features_default(struct kb_features *f)
{
    memset(f, 0, sizeof(*f));
    for (unsigned i = 0; i < KB_COMBO_MAX; i++) {
        for (unsigned k = 0; k < KB_COMBO_KEYS; k++) {
            f->combos[i].keys[k].row = KB_POS_NONE;
            f->combos[i].keys[k].col = KB_POS_NONE;
        }
    }
    for (unsigned i = 0; i < KB_TAP_HOLD_RULE_MAX; i++) {
        f->tap_hold[i].row = KB_POS_NONE;
        f->tap_hold[i].col = KB_POS_NONE;
    }
    f->leader_timeout_ms = 300;
    f->caps_word_idle_ms = 5000;
    f->auto_shift_ms = 175;
    f->mouse = (struct kb_mouse_config){
        .interval_ms = 16,
        .move_delta = 4,
        .max_speed = 20,
        .time_to_max_ms = 1000,
        .wheel_interval_ms = 80,
    };
    for (unsigned l = 0; l < KB_MAX_LAYERS; l++) {
        for (unsigned i = 0; i < KB_ENCODER_MAX; i++) {
            f->encoders[l][i].cw = l == 0 ? KC_NO : KC_TRNS;
            f->encoders[l][i].ccw = l == 0 ? KC_NO : KC_TRNS;
        }
    }
}

/* An action keycode a feature can produce: anything valid except tap dance inside tables. */
static bool result_ok(kb_keycode_t kc, uint8_t layers)
{
    return kb_keycode_is_valid(kc, layers) && kc != KC_TRNS;
}

static bool pos_ok(struct kb_pos p, uint8_t rows, uint8_t cols)
{
    return p.row < rows && p.col < cols;
}

static bool basic_ok(uint8_t usage)
{
    return kb_keycode_kind(usage, 1) == KB_KIND_BASIC;
}

bool kb_features_valid(const struct kb_features *f, uint8_t layers, uint8_t rows, uint8_t cols)
{
    for (unsigned i = 0; i < KB_COMBO_MAX; i++) {
        const struct kb_combo *c = &f->combos[i];
        if (c->result == KC_NO) {
            continue;
        }
        unsigned n = 0;
        while (n < KB_COMBO_KEYS && c->keys[n].row != KB_POS_NONE) {
            if (!pos_ok(c->keys[n], rows, cols)) {
                return false;
            }
            for (unsigned j = 0; j < n; j++) {
                if (c->keys[j].row == c->keys[n].row && c->keys[j].col == c->keys[n].col) {
                    return false;
                }
            }
            n++;
        }
        if (n < 2 || !result_ok(c->result, layers) ||
            kb_keycode_kind(c->result, layers) == KB_KIND_TAP_DANCE) {
            return false;
        }
        for (unsigned k = n; k < KB_COMBO_KEYS; k++) {
            if (c->keys[k].row != KB_POS_NONE) {
                return false;
            }
        }
    }

    for (unsigned i = 0; i < KB_TAP_DANCE_MAX; i++) {
        const struct kb_tap_dance *t = &f->tap_dances[i];
        const kb_keycode_t acts[] = {t->tap1, t->hold1, t->tap2, t->tap3};
        for (unsigned k = 0; k < 4; k++) {
            enum kb_keycode_kind kind = kb_keycode_kind(acts[k], layers);
            if (acts[k] != KC_NO && (!result_ok(acts[k], layers) || kind == KB_KIND_TAP_DANCE ||
                                     kind == KB_KIND_LT || kind == KB_KIND_MT)) {
                return false;
            }
        }
    }

    for (unsigned i = 0; i < KB_KEY_OVERRIDE_MAX; i++) {
        const struct kb_key_override *o = &f->overrides[i];
        if (o->replacement == KC_NO) {
            continue;
        }
        enum kb_keycode_kind kind = kb_keycode_kind(o->replacement, layers);
        if (!basic_ok(o->trigger_key) || o->trigger_mods == 0 ||
            !(kind == KB_KIND_BASIC || kind == KB_KIND_MODDED || kind == KB_KIND_CONSUMER ||
              kind == KB_KIND_SYSTEM)) {
            return false;
        }
    }

    for (unsigned i = 0; i < KB_LEADER_MAX; i++) {
        const struct kb_leader_seq *s = &f->leader[i];
        if (s->result == KC_NO) {
            continue;
        }
        if (!result_ok(s->result, layers) || s->keys[0] == 0) {
            return false;
        }
        bool ended = false;
        for (unsigned k = 0; k < KB_LEADER_KEYS; k++) {
            if (s->keys[k] == 0) {
                ended = true;
            } else if (ended || !basic_ok(s->keys[k])) {
                return false;
            }
        }
    }

    for (unsigned i = 0; i < KB_TAP_HOLD_RULE_MAX; i++) {
        const struct kb_tap_hold_rule *r = &f->tap_hold[i];
        if (r->row == KB_POS_NONE) {
            continue;
        }
        if (!pos_ok((struct kb_pos){r->row, r->col}, rows, cols) || (r->flags & ~KB_TH_FLAGS_ALL) ||
            r->tapping_term_ms < 50 || r->tapping_term_ms > 2000 || r->quick_tap_ms > 2000) {
            return false;
        }
    }

    if (f->mouse.interval_ms == 0 || f->mouse.wheel_interval_ms == 0 || f->mouse.max_speed == 0 ||
        f->mouse.move_delta == 0 || f->mouse.move_delta > f->mouse.max_speed ||
        f->mouse.max_speed > 127) {
        return false;
    }
    if (f->auto_shift_ms < 50 || f->auto_shift_ms > 1000 || f->leader_timeout_ms < 100 ||
        f->leader_timeout_ms > 5000) {
        return false;
    }

    for (unsigned l = 0; l < KB_MAX_LAYERS; l++) {
        for (unsigned i = 0; i < KB_ENCODER_MAX; i++) {
            const struct kb_encoder_action *a = &f->encoders[l][i];
            const kb_keycode_t acts[] = {a->cw, a->ccw};
            for (unsigned k = 0; k < 2; k++) {
                enum kb_keycode_kind kind = kb_keycode_kind(acts[k], layers);
                if (kind == KB_KIND_INVALID || kind == KB_KIND_LT || kind == KB_KIND_MT ||
                    kind == KB_KIND_TAP_DANCE) {
                    return false;
                }
            }
        }
    }

    return kb_macros_valid(f->macros, KB_MACRO_BUFFER, layers);
}

/* Length of the step at p (with at least `left` bytes), or 0 if it is cut off. */
static unsigned step_len(const uint8_t *p, unsigned left)
{
    unsigned n = (*p >= KB_MACRO_TAP && *p <= KB_MACRO_DELAY) ? 3u : 1u;
    return n <= left ? n : 0u;
}

int kb_macro_offset(const uint8_t *buf, uint16_t size, uint8_t n)
{
    unsigned off = 0;
    for (uint8_t m = 0; m < n; m++) {
        while (off < size && buf[off] != KB_MACRO_END) {
            unsigned len = step_len(&buf[off], size - off);
            if (len == 0) {
                return -1;
            }
            off += len;
        }
        if (off >= size) {
            return -1;
        }
        off++; /* the end marker */
    }
    return off < size ? (int)off : -1;
}

bool kb_macros_valid(const uint8_t *buf, uint16_t size, uint8_t layers)
{
    unsigned off = 0;
    while (off < size) {
        uint8_t op = buf[off];
        if (op == KB_MACRO_END) {
            off++;
            continue;
        }
        if (op >= KB_MACRO_TAP && op <= KB_MACRO_DELAY) {
            if (size - off < 3) {
                return false;
            }
            kb_keycode_t kc = (kb_keycode_t)(buf[off + 1] | (buf[off + 2] << 8));
            if (op != KB_MACRO_DELAY) {
                enum kb_keycode_kind kind = kb_keycode_kind(kc, layers);
                /* Steps send keys; they don't start more macros or tap-hold keys. */
                if (!(kind == KB_KIND_BASIC || kind == KB_KIND_MODDED ||
                      kind == KB_KIND_CONSUMER || kind == KB_KIND_SYSTEM)) {
                    return false;
                }
            }
            off += 3;
        } else if (op >= 0x20 && op <= 0x7E) {
            off++;
        } else {
            return false;
        }
    }
    return true;
}

bool kb_ascii_to_key(char c, uint8_t *usage, bool *shift)
{
    static const char unshifted[] = "\t -=[]\\;'`,./";
    static const uint8_t unshifted_keys[] = {KC_TAB,  KC_SPC,  KC_MINS, KC_EQL,  KC_LBRC,
                                             KC_RBRC, KC_BSLS, KC_SCLN, KC_QUOT, KC_GRV,
                                             KC_COMM, KC_DOT,  KC_SLSH};
    static const char shifted[] = "_+{}|:\"~<>?";
    static const uint8_t shifted_keys[] = {KC_MINS, KC_EQL,  KC_LBRC, KC_RBRC,
                                           KC_BSLS, KC_SCLN, KC_QUOT, KC_GRV,
                                           KC_COMM, KC_DOT,  KC_SLSH};
    static const char shifted_digits[] = ")!@#$%^&*(";

    *shift = false;
    if (c >= 'a' && c <= 'z') {
        *usage = (uint8_t)(KC_A + (c - 'a'));
        return true;
    }
    if (c >= 'A' && c <= 'Z') {
        *usage = (uint8_t)(KC_A + (c - 'A'));
        *shift = true;
        return true;
    }
    if (c >= '1' && c <= '9') {
        *usage = (uint8_t)(KC_1 + (c - '1'));
        return true;
    }
    if (c == '0') {
        *usage = KC_0;
        return true;
    }
    for (unsigned i = 0; i < 10; i++) {
        if (c == shifted_digits[i]) {
            *usage = i == 0 ? (uint8_t)KC_0 : (uint8_t)(KC_1 + i - 1);
            *shift = true;
            return true;
        }
    }
    for (unsigned i = 0; i < sizeof(unshifted) - 1; i++) {
        if (c == unshifted[i]) {
            *usage = unshifted_keys[i];
            return true;
        }
    }
    for (unsigned i = 0; i < sizeof(shifted) - 1; i++) {
        if (c == shifted[i]) {
            *usage = shifted_keys[i];
            *shift = true;
            return true;
        }
    }
    return false;
}
