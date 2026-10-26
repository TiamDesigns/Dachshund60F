/* The engine's table-driven features: combos, tap dance, key overrides,
 * Caps Word, Auto Shift, Repeat, Leader, macros, dynamic macros, mouse keys
 * and encoders. */
#include "kb/engine.h"
#include "kb/features.h"
#include "kb/protocol_gen.h"
#include "kb_test.h"

#include <string.h>

#define LAYERS 2
#define ROWS 2
#define COLS 12

enum {
    K_A = 0,
    K_B,
    K_C,
    K_D,
    K_TD,
    K_M0,
    K_M1,
    K_REP,
    K_LEAD,
    K_SPC,
    K_MINS,
    K_1,
    K_LSFT = COLS,
    K_RSFT,
    K_BSPC,
    K_CW,
    K_AS,
    K_MSR,
    K_BTN1,
    K_MO1,
    K_REC,
    K_STOP,
    K_PLAY,
};

static const kb_keycode_t keymap_def[LAYERS][ROWS][COLS] = {
    {
        {KC_A, KC_B, KC_C, KC_D, TD(0), M(0), M(1), QK_REP, QK_LEAD, KC_SPC, KC_MINS, KC_1},
        {KC_LSFT, KC_RSFT, KC_BSPC, CW_TOGG, AS_TOGG, MS_RGHT, MS_BTN1, MO(1), DM_REC1, DM_RSTP,
         DM_PLY1, KC_NO},
    },
    {
        {_______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
         _______, _______},
        {_______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
         _______, _______},
    },
};

static kb_keycode_t storage[LAYERS * ROWS * COLS];
static struct kb_keymap keymap;
static struct kb_engine eng;
static struct kb_features feat;

/* What was typed: every key usage as it went down, with the modifiers then. */
struct typed {
    uint8_t usage;
    uint8_t mods;
};
static struct typed typed[64];
static int ntyped;
static struct kb_hid_state last;
static uint16_t consumer_log[16];
static int nconsumer;
static struct kb_mouse_report mouse_log[64];
static int nmouse;

static void on_report(const struct kb_hid_state *hid, void *ctx)
{
    (void)ctx;
    for (unsigned u = 1; u < 0xE0; u++) {
        if (kb_hid_key_held(hid, (uint8_t)u) && !kb_hid_key_held(&last, (uint8_t)u) && ntyped < 64) {
            typed[ntyped++] = (struct typed){(uint8_t)u, hid->mods};
        }
    }
    if (hid->consumer && hid->consumer != last.consumer && nconsumer < 16) {
        consumer_log[nconsumer++] = hid->consumer;
    }
    last = *hid;
}

static void on_mouse(const struct kb_mouse_report *r, void *ctx)
{
    (void)ctx;
    if (nmouse < 64) {
        mouse_log[nmouse++] = *r;
    }
}

static const struct kb_engine_ops ops = {.report = on_report, .mouse = on_mouse};

static void setup(void)
{
    kb_keymap_init(&keymap, storage, LAYERS, ROWS, COLS, &keymap_def[0][0][0]);
    kb_engine_init(&eng, &keymap, &ops, NULL);
    kb_features_default(&feat);
    kb_engine_set_features(&eng, &feat);
    ntyped = 0;
    nconsumer = 0;
    nmouse = 0;
    memset(&last, 0, sizeof(last));
}

/* Call after changing feat's settings that the engine copies. */
static void apply(void)
{
    KB_CHECK(kb_features_valid(&feat, LAYERS, ROWS, COLS));
    kb_engine_set_features(&eng, &feat);
}

static void key(int pos, bool pressed, uint32_t t)
{
    kb_engine_key(&eng, (uint8_t)(pos / COLS), (uint8_t)(pos % COLS), pressed, t);
}

static void tap(int pos, uint32_t t)
{
    key(pos, true, t);
    key(pos, false, t + 1);
}

static struct kb_pos pos(int p)
{
    return (struct kb_pos){(uint8_t)(p / COLS), (uint8_t)(p % COLS)};
}

static void combo(unsigned i, int k0, int k1, int k2, kb_keycode_t result)
{
    feat.combos[i].keys[0] = pos(k0);
    feat.combos[i].keys[1] = pos(k1);
    if (k2 >= 0) {
        feat.combos[i].keys[2] = pos(k2);
    }
    feat.combos[i].result = result;
}

/* typed[] as usages, e.g. "A b" checks KC_A with Shift then KC_B without. */
static void check_typed(const uint8_t *usages, const uint8_t *shift, int n, int line)
{
    if (ntyped != n) {
        fprintf(stderr, "%s:%d: typed %d keys, expected %d\n", __FILE__, line, ntyped, n);
        kb_test_failures++;
        return;
    }
    for (int i = 0; i < n; i++) {
        bool shifted = (typed[i].mods & 0x22u) != 0;
        if (typed[i].usage != usages[i] || shifted != (shift[i] != 0)) {
            fprintf(stderr, "%s:%d: key %d is 0x%02x%s, expected 0x%02x%s\n", __FILE__, line, i,
                    typed[i].usage, shifted ? "+shift" : "", usages[i], shift[i] ? "+shift" : "");
            kb_test_failures++;
        }
    }
}

#define CHECK_TYPED(...)                                                       \
    do {                                                                       \
        const uint8_t u_[] = {__VA_ARGS__};                                    \
        uint8_t s_[sizeof(u_)] = {0};                                          \
        check_typed(u_, s_, (int)sizeof(u_), __LINE__);                        \
    } while (0)

#define CHECK_TYPED_SHIFT(usages, shifts)                                      \
    do {                                                                       \
        const uint8_t u_[] = usages;                                           \
        const uint8_t s_[] = shifts;                                           \
        check_typed(u_, s_, (int)sizeof(u_), __LINE__);                        \
    } while (0)

#define LIST(...) {__VA_ARGS__}

/* Combos */

static void test_combo_fires(void)
{
    setup();
    combo(0, K_A, K_B, -1, KC_ESC);
    apply();
    key(K_A, true, 0);
    KB_CHECK_EQ(ntyped, 0); /* waiting */
    key(K_B, true, 20);
    CHECK_TYPED(KC_ESC);
    KB_CHECK(kb_hid_key_held(&last, KC_ESC));
    key(K_A, false, 100); /* the first key up releases it */
    KB_CHECK(!kb_hid_key_held(&last, KC_ESC));
    key(K_B, false, 110);
    CHECK_TYPED(KC_ESC);
    KB_CHECK_EQ(last.boot_keys[0], 0);
}

static void test_combo_switched_off(void)
{
    setup();
    combo(0, K_A, K_B, -1, KC_ESC);
    feat.off = KB_OFF_COMBOS;
    apply();
    key(K_A, true, 0);
    CHECK_TYPED(KC_A); /* at once, not held back */
    key(K_B, true, 20);
    CHECK_TYPED(KC_A, KC_B);
    key(K_A, false, 30);
    key(K_B, false, 40);
    KB_CHECK_EQ(last.boot_keys[0], 0);

    feat.off = 0; /* the table was kept */
    apply();
    ntyped = 0;
    key(K_A, true, 100);
    key(K_B, true, 110);
    CHECK_TYPED(KC_ESC);
}

static void test_combo_too_slow_types_the_keys(void)
{
    setup();
    combo(0, K_A, K_B, -1, KC_ESC);
    apply();
    key(K_A, true, 0);
    key(K_B, true, 80);
    CHECK_TYPED(KC_A);
    kb_engine_tick(&eng, 200);
    CHECK_TYPED(KC_A, KC_B);
    key(K_A, false, 210);
    key(K_B, false, 220);
    KB_CHECK_EQ(last.boot_keys[0], 0);
}

static void test_combo_key_tapped_alone(void)
{
    setup();
    combo(0, K_A, K_B, -1, KC_ESC);
    apply();
    tap(K_A, 0);
    CHECK_TYPED(KC_A);
    KB_CHECK(!kb_hid_key_held(&last, KC_A));
    /* A key that's in no combo goes straight through. */
    key(K_C, true, 10);
    CHECK_TYPED(KC_A, KC_C);
}

static void test_combo_other_key_breaks_it(void)
{
    setup();
    combo(0, K_A, K_B, -1, KC_ESC);
    apply();
    key(K_A, true, 0);
    key(K_C, true, 5);
    CHECK_TYPED(KC_A, KC_C);
}

static void test_combo_longer_wins_when_completed(void)
{
    setup();
    combo(0, K_A, K_B, -1, KC_ESC);
    combo(1, K_A, K_B, K_C, KC_TAB);
    apply();
    key(K_A, true, 0);
    key(K_B, true, 5);
    KB_CHECK_EQ(ntyped, 0); /* could still become A+B+C */
    key(K_C, true, 10);
    CHECK_TYPED(KC_TAB);
    key(K_A, false, 50);
    key(K_B, false, 50);
    key(K_C, false, 50);

    /* A+B alone fires once the timeout passes. */
    ntyped = 0;
    key(K_A, true, 100);
    key(K_B, true, 105);
    kb_engine_tick(&eng, 160);
    CHECK_TYPED(KC_ESC);
}

static void test_combo_released_early_still_fires(void)
{
    setup();
    combo(0, K_A, K_B, K_C, KC_TAB);
    combo(1, K_A, K_B, -1, KC_ESC);
    apply();
    key(K_A, true, 0);
    key(K_B, true, 5);
    key(K_A, false, 20); /* released before C: it was A+B */
    CHECK_TYPED(KC_ESC);
    KB_CHECK(!kb_hid_key_held(&last, KC_ESC));
    key(K_B, false, 30);
    KB_CHECK_EQ(ntyped, 1);
}

static void test_combo_layer_mask(void)
{
    setup();
    combo(0, K_A, K_B, -1, KC_ESC);
    feat.combos[0].layers = 1u << 1; /* only on layer 1 */
    apply();
    key(K_A, true, 0);
    key(K_B, true, 5);
    CHECK_TYPED(KC_A, KC_B);
    key(K_A, false, 10);
    key(K_B, false, 10);
    key(K_MO1, true, 20);
    key(K_A, true, 30);
    key(K_B, true, 35);
    CHECK_TYPED(KC_A, KC_B, KC_ESC);
}

/* Tap dance */

static void dance_setup(void)
{
    setup();
    feat.tap_dances[0] = (struct kb_tap_dance){
        .tap1 = KC_C, .hold1 = KC_LCTL, .tap2 = KC_D, .tap3 = KC_TAB, .term_ms = 200};
    apply();
}

static void test_tap_dance_single(void)
{
    dance_setup();
    tap(K_TD, 0);
    KB_CHECK_EQ(ntyped, 0);
    kb_engine_tick(&eng, 150);
    KB_CHECK_EQ(ntyped, 0);
    kb_engine_tick(&eng, 202);
    CHECK_TYPED(KC_C);
    KB_CHECK(!kb_hid_key_held(&last, KC_C));
}

static void test_tap_dance_double_and_triple(void)
{
    dance_setup();
    tap(K_TD, 0);
    tap(K_TD, 100);
    kb_engine_tick(&eng, 400);
    CHECK_TYPED(KC_D);

    ntyped = 0;
    tap(K_TD, 1000);
    tap(K_TD, 1100);
    key(K_TD, true, 1200); /* the third tap decides at once */
    CHECK_TYPED(KC_TAB);
    KB_CHECK(kb_hid_key_held(&last, KC_TAB));
    key(K_TD, false, 1210);
    KB_CHECK(!kb_hid_key_held(&last, KC_TAB));
}

static void test_tap_dance_hold(void)
{
    dance_setup();
    key(K_TD, true, 0);
    kb_engine_tick(&eng, 250);
    KB_CHECK_EQ(last.mods, 0x01); /* Left Ctrl */
    key(K_A, true, 260);
    CHECK_TYPED(KC_A);
    KB_CHECK_EQ(typed[0].mods, 0x01);
    key(K_A, false, 270);
    key(K_TD, false, 300);
    KB_CHECK_EQ(last.mods, 0);
}

static void test_tap_dance_switched_off(void)
{
    dance_setup();
    feat.off = KB_OFF_TAP_DANCE;
    apply();
    key(K_TD, true, 0);
    CHECK_TYPED(KC_C); /* the one-tap action, at once */
    kb_engine_tick(&eng, 400);
    KB_CHECK_EQ(last.mods, 0); /* holding doesn't turn it into Ctrl */
    key(K_TD, false, 410);
    tap(K_TD, 420); /* nor does tapping twice make a double tap */
    CHECK_TYPED(KC_C, KC_C);
    KB_CHECK(kb_engine_idle(&eng));
}

static void test_tap_dance_interrupted(void)
{
    dance_setup();
    tap(K_TD, 0);
    key(K_A, true, 50); /* another key ends the dance first */
    CHECK_TYPED(KC_C, KC_A);
}

/* Key overrides */

static void test_key_override(void)
{
    setup();
    feat.overrides[0] = (struct kb_key_override){
        .trigger_mods = 0x02, .trigger_key = KC_BSPC, .replacement = KC_DEL};
    apply();
    key(K_RSFT, true, 0); /* either Shift counts */
    key(K_BSPC, true, 10);
    CHECK_TYPED(KC_DEL);
    KB_CHECK_EQ(last.mods, 0); /* Shift hidden */
    key(K_BSPC, false, 20);
    KB_CHECK_EQ(last.mods, 0x20); /* back */
    key(K_RSFT, false, 30);

    /* Without Shift, plain Backspace. */
    tap(K_BSPC, 40);
    CHECK_TYPED(KC_DEL, KC_BSPC);
}

/* Caps Word */

static void test_caps_word(void)
{
    setup();
    tap(K_CW, 0);
    KB_CHECK(kb_engine_caps_word(&eng));
    tap(K_A, 10);
    tap(K_1, 20);
    tap(K_MINS, 30);
    tap(K_B, 40);
    tap(K_SPC, 50); /* ends it */
    KB_CHECK(!kb_engine_caps_word(&eng));
    tap(K_C, 60);
    CHECK_TYPED_SHIFT(LIST(KC_A, KC_1, KC_MINS, KC_B, KC_SPC, KC_C), LIST(1, 0, 1, 1, 0, 0));
    KB_CHECK_EQ(last.mods, 0);
}

static void test_caps_word_idle_timeout(void)
{
    setup();
    tap(K_CW, 0);
    kb_engine_tick(&eng, 4000);
    KB_CHECK(kb_engine_caps_word(&eng));
    kb_engine_tick(&eng, 5001);
    KB_CHECK(!kb_engine_caps_word(&eng));
}

static void test_caps_word_both_shifts(void)
{
    setup();
    key(K_LSFT, true, 0);
    key(K_RSFT, true, 10);
    KB_CHECK(!kb_engine_caps_word(&eng)); /* off unless set */
    key(K_LSFT, false, 20);
    key(K_RSFT, false, 20);

    feat.caps_word_on_both_shifts = true;
    apply();
    key(K_LSFT, true, 30);
    key(K_RSFT, true, 40);
    KB_CHECK(kb_engine_caps_word(&eng));
    key(K_LSFT, false, 50);
    key(K_RSFT, false, 50);
    tap(K_A, 60);
    CHECK_TYPED_SHIFT(LIST(KC_A), LIST(1));
}

/* Auto Shift */

static void test_auto_shift(void)
{
    setup();
    tap(K_AS, 0);
    tap(K_A, 10); /* quick: plain */
    key(K_B, true, 100);
    kb_engine_tick(&eng, 300); /* held past 175 ms: shifted */
    key(K_B, false, 310);
    CHECK_TYPED_SHIFT(LIST(KC_A, KC_B), LIST(0, 1));
    KB_CHECK_EQ(last.mods, 0);

    /* Rolling from one key to the next types both plain. */
    ntyped = 0;
    key(K_A, true, 400);
    key(K_C, true, 430);
    key(K_A, false, 440);
    key(K_C, false, 450);
    CHECK_TYPED(KC_A, KC_C);

    /* Not for keys like Space, and off again with AS_TOGG. */
    ntyped = 0;
    key(K_SPC, true, 500);
    CHECK_TYPED(KC_SPC);
    key(K_SPC, false, 510);
    tap(K_AS, 520);
    key(K_A, true, 530);
    CHECK_TYPED(KC_SPC, KC_A);
}

static void test_auto_shift_on_from_settings(void)
{
    setup();
    feat.auto_shift = true;
    feat.auto_shift_ms = 100;
    apply();
    key(K_A, true, 0);
    kb_engine_tick(&eng, 120);
    CHECK_TYPED_SHIFT(LIST(KC_A), LIST(1));
}

/* Repeat */

static void test_repeat_key(void)
{
    setup();
    tap(K_REP, 0); /* nothing yet */
    KB_CHECK_EQ(ntyped, 0);
    key(K_LSFT, true, 10);
    tap(K_A, 20);
    key(K_LSFT, false, 30);
    key(K_REP, true, 40);
    KB_CHECK(kb_hid_key_held(&last, KC_A));
    key(K_REP, false, 50);
    tap(K_REP, 60);
    CHECK_TYPED_SHIFT(LIST(KC_A, KC_A, KC_A), LIST(1, 1, 1));
    KB_CHECK_EQ(last.mods, 0);
}

/* Leader */

static void leader_setup(void)
{
    setup();
    feat.leader[0] = (struct kb_leader_seq){.keys = {KC_A, KC_B}, .result = KC_ESC};
    feat.leader[1] = (struct kb_leader_seq){.keys = {KC_C}, .result = KC_TAB};
    feat.leader[2] = (struct kb_leader_seq){.keys = {KC_C, KC_D}, .result = LSFT(KC_1)};
    apply();
}

static void test_leader(void)
{
    leader_setup();
    tap(K_LEAD, 0);
    tap(K_A, 10);
    tap(K_B, 20);
    CHECK_TYPED(KC_ESC);
    KB_CHECK(!kb_hid_key_held(&last, KC_ESC));
    tap(K_A, 30); /* over: typed as usual */
    CHECK_TYPED(KC_ESC, KC_A);
}

static void test_leader_prefix_waits(void)
{
    leader_setup();
    tap(K_LEAD, 0);
    tap(K_C, 10);
    KB_CHECK_EQ(ntyped, 0); /* C, or C D? */
    tap(K_D, 20);
    CHECK_TYPED_SHIFT(LIST(KC_1), LIST(1));

    ntyped = 0;
    tap(K_LEAD, 100);
    tap(K_C, 110);
    kb_engine_tick(&eng, 500); /* timed out on an exact match */
    CHECK_TYPED(KC_TAB);
}

static void test_leader_no_match(void)
{
    leader_setup();
    tap(K_LEAD, 0);
    tap(K_D, 10); /* no sequence starts with D: dropped, leader over */
    tap(K_D, 20);
    CHECK_TYPED(KC_D);
}

/* Macros */

static void test_macro_text_and_steps(void)
{
    setup();
    const uint8_t m[] = {
        'H', 'i', '!', KB_MACRO_TAP, KC_ENT, 0, KB_MACRO_END, /* M(0) */
        KB_MACRO_DOWN, KC_LCTL, 0, KB_MACRO_TAP, KC_C, 0, KB_MACRO_UP, KC_LCTL, 0,
        KB_MACRO_DELAY, 100, 0, KB_MACRO_TAP, KC_D, 0, KB_MACRO_END, /* M(1) */
    };
    memcpy(feat.macros, m, sizeof(m));
    apply();
    KB_CHECK_EQ(kb_macro_offset(feat.macros, KB_MACRO_BUFFER, 1), 7);

    tap(K_M0, 0);
    CHECK_TYPED_SHIFT(LIST(KC_H, KC_I, KC_1, KC_ENT), LIST(1, 0, 1, 0));
    KB_CHECK_EQ(last.mods, 0);

    ntyped = 0;
    tap(K_M1, 10);
    KB_CHECK_EQ(ntyped, 1);
    KB_CHECK_EQ(typed[0].usage, KC_C);
    KB_CHECK_EQ(typed[0].mods, 0x01);
    KB_CHECK_EQ(last.mods, 0);
    kb_engine_tick(&eng, 50);
    KB_CHECK_EQ(ntyped, 1); /* still waiting */
    kb_engine_tick(&eng, 110);
    KB_CHECK_EQ(ntyped, 2);
    KB_CHECK_EQ(typed[1].usage, KC_D);
}

static void test_macro_missing_does_nothing(void)
{
    setup();
    tap(K_M1, 0); /* empty buffer: no macro 1 */
    kb_engine_tick(&eng, 10);
    KB_CHECK_EQ(ntyped, 0);
}

static void test_dynamic_macro(void)
{
    setup();
    tap(K_REC, 0);
    tap(K_A, 10);
    key(K_LSFT, true, 20);
    tap(K_B, 30);
    key(K_LSFT, false, 40);
    tap(K_STOP, 50);
    CHECK_TYPED_SHIFT(LIST(KC_A, KC_B), LIST(0, 1));
    tap(K_PLAY, 60);
    CHECK_TYPED_SHIFT(LIST(KC_A, KC_B, KC_A, KC_B), LIST(0, 1, 0, 1));
    KB_CHECK_EQ(last.mods, 0);
    KB_CHECK_EQ(last.boot_keys[0], 0);
}

/* Mouse keys */

static void test_mouse_keys(void)
{
    setup();
    key(K_MSR, true, 0);
    KB_CHECK_EQ(nmouse, 1);
    KB_CHECK_EQ(mouse_log[0].x, 4);
    kb_engine_tick(&eng, 10);
    KB_CHECK_EQ(nmouse, 1);
    kb_engine_tick(&eng, 16);
    KB_CHECK_EQ(nmouse, 2);
    kb_engine_tick(&eng, 2000); /* accelerated to the top speed */
    KB_CHECK_EQ(mouse_log[nmouse - 1].x, 20);
    key(K_MSR, false, 2001);
    int n = nmouse;
    kb_engine_tick(&eng, 2100);
    KB_CHECK_EQ(nmouse, n);

    key(K_BTN1, true, 2200);
    KB_CHECK_EQ(mouse_log[nmouse - 1].buttons, 1);
    key(K_BTN1, false, 2210);
    KB_CHECK_EQ(mouse_log[nmouse - 1].buttons, 0);
}

/* Encoders */

static void test_encoder_layers(void)
{
    setup();
    feat.encoders[0][0] = (struct kb_encoder_action){.cw = KC_VOLU, .ccw = KC_VOLD};
    feat.encoders[1][0] = (struct kb_encoder_action){.cw = KC_TRNS, .ccw = KC_MNXT};
    apply();
    kb_engine_encoder(&eng, 0, true, 0);
    kb_engine_encoder(&eng, 0, false, 10);
    KB_CHECK_EQ(nconsumer, 2);
    KB_CHECK_EQ(consumer_log[0], KC_VOLU & 0x3FF);
    KB_CHECK_EQ(consumer_log[1], KC_VOLD & 0x3FF);
    KB_CHECK_EQ(last.consumer, 0);

    key(K_MO1, true, 20);
    kb_engine_encoder(&eng, 0, true, 30);  /* transparent: layer 0's */
    kb_engine_encoder(&eng, 0, false, 40); /* layer 1's own */
    KB_CHECK_EQ(nconsumer, 4);
    KB_CHECK_EQ(consumer_log[2], KC_VOLU & 0x3FF);
    KB_CHECK_EQ(consumer_log[3], KC_MNXT & 0x3FF);

    kb_engine_encoder(&eng, 7, true, 50); /* no such encoder */
    KB_CHECK_EQ(nconsumer, 4);
}

static void test_encoder_basic_key(void)
{
    setup();
    feat.encoders[0][1] = (struct kb_encoder_action){.cw = KC_A, .ccw = KC_B};
    apply();
    kb_engine_encoder(&eng, 1, true, 0);
    kb_engine_encoder(&eng, 1, true, 1);
    kb_engine_encoder(&eng, 1, false, 2);
    CHECK_TYPED(KC_A, KC_A, KC_B);
    KB_CHECK_EQ(last.boot_keys[0], 0);
}

/* Random presses, releases, detents and ticks with every feature set up:
 * nothing crashes, and once every key is up and time has passed, nothing is
 * left held. */
static void test_random_events_leave_nothing_held(void)
{
    for (int round = 0; round < 200; round++) {
        uint32_t seed = 12345u + (uint32_t)round * 7919u;
        setup();
        combo(0, K_A, K_B, -1, KC_ESC);
        combo(1, K_A, K_B, K_C, MO(1));
        combo(2, K_C, K_D, -1, KC_LSFT);
        feat.tap_dances[0] = (struct kb_tap_dance){
            .tap1 = KC_C, .hold1 = KC_LCTL, .tap2 = MO(1), .tap3 = M(0)};
        feat.overrides[0] = (struct kb_key_override){
            .trigger_mods = 0x02, .trigger_key = KC_BSPC, .replacement = KC_DEL};
        feat.leader[0] = (struct kb_leader_seq){.keys = {KC_A, KC_B}, .result = KC_ESC};
        feat.encoders[0][0] = (struct kb_encoder_action){.cw = KC_VOLU, .ccw = KC_A};
        const uint8_t m[] = {'x', KB_MACRO_DOWN, KC_LALT, 0, KB_MACRO_DELAY, 30, 0,
                             KB_MACRO_UP, KC_LALT, 0, KB_MACRO_END,
                             KB_MACRO_DOWN, KC_B, 0, KB_MACRO_END};
        memcpy(feat.macros, m, sizeof(m));
        feat.caps_word_on_both_shifts = true;
        feat.auto_shift = (round & 1) != 0;
        apply();

        bool down[ROWS * COLS] = {0};
        uint32_t t = (uint32_t)round * 0x40000000u; /* wraps for some rounds */
        for (int step = 0; step < 300; step++) {
            seed = seed * 1103515245u + 12345u;
            uint32_t r = seed >> 8;
            t += r % 60;
            if (r % 11 == 0) {
                kb_engine_encoder(&eng, (uint8_t)(r % 3), (r & 0x100) != 0, t);
            } else if (r % 7 == 0) {
                kb_engine_tick(&eng, t);
            } else {
                int p = (int)((r >> 4) % (ROWS * COLS));
                down[p] = !down[p];
                key(p, down[p], t);
            }
        }
        for (int p = 0; p < ROWS * COLS; p++) {
            if (down[p]) {
                key(p, false, t);
            }
        }
        for (int i = 1; i <= 20; i++) {
            kb_engine_tick(&eng, t + (uint32_t)i * 500);
        }
        KB_CHECK_EQ(last.mods, 0);
        KB_CHECK_EQ(last.boot_keys[0], 0);
        KB_CHECK_EQ(last.consumer, 0);
        KB_CHECK_EQ(kb_engine_layer_mask(&eng) & 0x2u, 0);
        if (kb_test_failures) {
            fprintf(stderr, "round %d\n", round);
            return;
        }
    }
}

static void test_tables_change_under_the_engine(void)
{
    setup();
    combo(0, K_A, K_B, -1, KC_ESC);
    apply();
    key(K_A, true, 0);
    key(K_B, true, 5);
    KB_CHECK(kb_hid_key_held(&last, KC_ESC));
    /* The host edits a table while the combo is held: it's released. */
    feat.combos[0].result = KC_TAB;
    kb_engine_set_features(&eng, &feat);
    KB_CHECK(!kb_hid_key_held(&last, KC_ESC));
    key(K_A, false, 20);
    key(K_B, false, 25);
    KB_CHECK_EQ(last.boot_keys[0], 0);

    /* A key waiting to become a combo goes on as itself. */
    key(K_A, true, 100);
    KB_CHECK_EQ(ntyped, 1);
    kb_engine_set_features(&eng, &feat);
    KB_CHECK_EQ(typed[ntyped - 1].usage, KC_A);
    key(K_A, false, 110);

    /* A macro waiting on a delay stops, and its held keys come up. */
    const uint8_t m[] = {KB_MACRO_DOWN, KC_LALT, 0, KB_MACRO_DELAY, 100, 0, KB_MACRO_TAP, KC_D, 0,
                         KB_MACRO_END};
    memcpy(feat.macros, m, sizeof(m));
    apply();
    tap(K_M0, 200);
    KB_CHECK_EQ(last.mods, 0x04);
    kb_engine_set_features(&eng, &feat);
    KB_CHECK_EQ(last.mods, 0);
    kb_engine_tick(&eng, 400);
    KB_CHECK(typed[ntyped - 1].usage != KC_D);
}

static void test_entry_codec(void)
{
    kb_features_default(&feat);
    uint8_t buf[16];
    KB_CHECK_EQ(kb_feature_entry_size(KB_PROTO_TABLE_COMBO), 14);
    KB_CHECK_EQ(kb_feature_entry_size(KB_PROTO_TABLE_COUNT), 0);
    KB_CHECK_EQ(kb_feature_entries(KB_PROTO_TABLE_ENCODER, 3), 6);
    KB_CHECK_EQ(kb_feature_entries(KB_PROTO_TABLE_COMBO, 3), KB_COMBO_MAX);
    KB_CHECK(!kb_feature_get(&feat, KB_PROTO_TABLE_COMBO, KB_COMBO_MAX, buf));
    KB_CHECK(!kb_feature_get(&feat, 200, 0, buf));

    const uint8_t dance[10] = {KC_A, 0, KC_LCTL, 0, KC_B, 0, KC_C, 0, 0x2C, 0x01};
    KB_CHECK(kb_feature_put(&feat, KB_PROTO_TABLE_TAP_DANCE, 3, dance, sizeof(dance)));
    KB_CHECK_EQ(feat.tap_dances[3].hold1, KC_LCTL);
    KB_CHECK_EQ(feat.tap_dances[3].term_ms, 300);
    KB_CHECK(kb_feature_get(&feat, KB_PROTO_TABLE_TAP_DANCE, 3, buf));
    KB_CHECK(memcmp(buf, dance, sizeof(dance)) == 0);
    KB_CHECK(!kb_feature_put(&feat, KB_PROTO_TABLE_TAP_DANCE, 3, buf, 11));

    /* Options in the documented order. */
    KB_CHECK(kb_feature_get(&feat, KB_PROTO_TABLE_OPTIONS, 0, buf));
    KB_CHECK_EQ(buf[0] | (buf[1] << 8), 300);  /* leader */
    KB_CHECK_EQ(buf[3] | (buf[4] << 8), 5000); /* caps word idle */
    KB_CHECK_EQ(buf[6] | (buf[7] << 8), 175);  /* auto shift */
    KB_CHECK_EQ(buf[8], 16);                   /* mouse interval */
    KB_CHECK_EQ(buf[13], 80);                  /* wheel interval */
    KB_CHECK_EQ(buf[14], 0);                   /* nothing switched off */

    /* An entry from an older app (no off field) keeps the switches as they are. */
    feat.off = KB_OFF_COMBOS;
    KB_CHECK(kb_feature_put(&feat, KB_PROTO_TABLE_OPTIONS, 0, buf, 14));
    KB_CHECK_EQ(feat.off, KB_OFF_COMBOS);
    buf[14] = KB_OFF_TAP_HOLD | KB_OFF_TAP_DANCE;
    KB_CHECK(kb_feature_put(&feat, KB_PROTO_TABLE_OPTIONS, 0, buf, 15));
    KB_CHECK_EQ(feat.off, KB_OFF_TAP_HOLD | KB_OFF_TAP_DANCE);
}

/* Validation */

static void test_features_valid(void)
{
    kb_features_default(&feat);
    KB_CHECK(kb_features_valid(&feat, LAYERS, ROWS, COLS));
    feat.off = KB_OFF_ALL;
    KB_CHECK(kb_features_valid(&feat, LAYERS, ROWS, COLS));
    feat.off = 0x08; /* no such feature */
    KB_CHECK(!kb_features_valid(&feat, LAYERS, ROWS, COLS));
    feat.off = 0;

    combo(0, K_A, K_B, -1, KC_ESC);
    KB_CHECK(kb_features_valid(&feat, LAYERS, ROWS, COLS));
    feat.combos[0].keys[1] = (struct kb_pos){0, COLS}; /* off the board */
    KB_CHECK(!kb_features_valid(&feat, LAYERS, ROWS, COLS));
    feat.combos[0].keys[1] = pos(K_A); /* twice */
    KB_CHECK(!kb_features_valid(&feat, LAYERS, ROWS, COLS));
    feat.combos[0].keys[1] = (struct kb_pos){KB_POS_NONE, KB_POS_NONE}; /* one key */
    KB_CHECK(!kb_features_valid(&feat, LAYERS, ROWS, COLS));
    combo(0, K_A, K_B, -1, TD(1)); /* no dances from combos */
    KB_CHECK(!kb_features_valid(&feat, LAYERS, ROWS, COLS));
    combo(0, K_A, K_B, -1, MO(5)); /* no such layer */
    KB_CHECK(!kb_features_valid(&feat, LAYERS, ROWS, COLS));

    kb_features_default(&feat);
    feat.tap_dances[0].hold1 = LT(1, KC_A);
    KB_CHECK(!kb_features_valid(&feat, LAYERS, ROWS, COLS));

    kb_features_default(&feat);
    feat.overrides[0] = (struct kb_key_override){.trigger_mods = 0, .trigger_key = KC_A,
                                                 .replacement = KC_B};
    KB_CHECK(!kb_features_valid(&feat, LAYERS, ROWS, COLS));

    kb_features_default(&feat);
    feat.leader[0] = (struct kb_leader_seq){.keys = {KC_A, 0, KC_B}, .result = KC_C};
    KB_CHECK(!kb_features_valid(&feat, LAYERS, ROWS, COLS));

    kb_features_default(&feat);
    feat.tap_hold[0] = (struct kb_tap_hold_rule){.row = 0, .col = 0, .flags = 0x80,
                                                 .tapping_term_ms = 200};
    KB_CHECK(!kb_features_valid(&feat, LAYERS, ROWS, COLS));
    feat.tap_hold[0].flags = KB_TH_PERMISSIVE_HOLD;
    KB_CHECK(kb_features_valid(&feat, LAYERS, ROWS, COLS));
    feat.tap_hold[0].tapping_term_ms = 10;
    KB_CHECK(!kb_features_valid(&feat, LAYERS, ROWS, COLS));

    kb_features_default(&feat);
    feat.mouse.max_speed = 0;
    KB_CHECK(!kb_features_valid(&feat, LAYERS, ROWS, COLS));

    kb_features_default(&feat);
    feat.encoders[0][0].cw = TD(0);
    KB_CHECK(!kb_features_valid(&feat, LAYERS, ROWS, COLS));
    feat.encoders[0][0].cw = MS_WHLU;
    KB_CHECK(kb_features_valid(&feat, LAYERS, ROWS, COLS));
}

static void test_macros_valid(void)
{
    uint8_t buf[8] = {'a', KB_MACRO_TAP, KC_A, 0, KB_MACRO_END, 0, 0, 0};
    KB_CHECK(kb_macros_valid(buf, sizeof(buf), LAYERS));
    KB_CHECK_EQ(kb_macro_offset(buf, sizeof(buf), 0), 0);
    KB_CHECK_EQ(kb_macro_offset(buf, sizeof(buf), 1), 5);
    KB_CHECK_EQ(kb_macro_offset(buf, sizeof(buf), 3), 7);
    KB_CHECK_EQ(kb_macro_offset(buf, sizeof(buf), 4), -1);

    buf[0] = 0x05; /* unknown op */
    KB_CHECK(!kb_macros_valid(buf, sizeof(buf), LAYERS));
    buf[0] = 0x80;
    KB_CHECK(!kb_macros_valid(buf, sizeof(buf), LAYERS));
    buf[0] = 'a';
    buf[2] = (uint8_t)(M(0) & 0xFF); /* steps can't start macros */
    buf[3] = (uint8_t)(M(0) >> 8);
    KB_CHECK(!kb_macros_valid(buf, sizeof(buf), LAYERS));

    uint8_t cut[2] = {KB_MACRO_DELAY, 10}; /* step cut off at the end */
    KB_CHECK(!kb_macros_valid(cut, sizeof(cut), LAYERS));
    KB_CHECK_EQ(kb_macro_offset(cut, sizeof(cut), 1), -1);
}

static void test_ascii_to_key(void)
{
    uint8_t u;
    bool shift;
    KB_CHECK(kb_ascii_to_key('a', &u, &shift) && u == KC_A && !shift);
    KB_CHECK(kb_ascii_to_key('Z', &u, &shift) && u == KC_Z && shift);
    KB_CHECK(kb_ascii_to_key('0', &u, &shift) && u == KC_0 && !shift);
    KB_CHECK(kb_ascii_to_key(')', &u, &shift) && u == KC_0 && shift);
    KB_CHECK(kb_ascii_to_key('@', &u, &shift) && u == KC_2 && shift);
    KB_CHECK(kb_ascii_to_key('?', &u, &shift) && u == KC_SLSH && shift);
    KB_CHECK(kb_ascii_to_key(' ', &u, &shift) && u == KC_SPC && !shift);
    KB_CHECK(!kb_ascii_to_key('\x7f', &u, &shift));
}

int main(void)
{
    printf("test_features\n");
    KB_RUN(test_combo_fires);
    KB_RUN(test_combo_switched_off);
    KB_RUN(test_combo_too_slow_types_the_keys);
    KB_RUN(test_combo_key_tapped_alone);
    KB_RUN(test_combo_other_key_breaks_it);
    KB_RUN(test_combo_longer_wins_when_completed);
    KB_RUN(test_combo_released_early_still_fires);
    KB_RUN(test_combo_layer_mask);
    KB_RUN(test_tap_dance_single);
    KB_RUN(test_tap_dance_double_and_triple);
    KB_RUN(test_tap_dance_hold);
    KB_RUN(test_tap_dance_switched_off);
    KB_RUN(test_tap_dance_interrupted);
    KB_RUN(test_key_override);
    KB_RUN(test_caps_word);
    KB_RUN(test_caps_word_idle_timeout);
    KB_RUN(test_caps_word_both_shifts);
    KB_RUN(test_auto_shift);
    KB_RUN(test_auto_shift_on_from_settings);
    KB_RUN(test_repeat_key);
    KB_RUN(test_leader);
    KB_RUN(test_leader_prefix_waits);
    KB_RUN(test_leader_no_match);
    KB_RUN(test_macro_text_and_steps);
    KB_RUN(test_macro_missing_does_nothing);
    KB_RUN(test_dynamic_macro);
    KB_RUN(test_mouse_keys);
    KB_RUN(test_encoder_layers);
    KB_RUN(test_encoder_basic_key);
    KB_RUN(test_tables_change_under_the_engine);
    KB_RUN(test_entry_codec);
    KB_RUN(test_random_events_leave_nothing_held);
    KB_RUN(test_features_valid);
    KB_RUN(test_macros_valid);
    KB_RUN(test_ascii_to_key);
    return KB_TEST_RESULT();
}
