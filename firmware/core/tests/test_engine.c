#include "kb/engine.h"
#include "kb_test.h"

#include <string.h>

#define LAYERS 4
#define ROWS 2
#define COLS 6

/* Key positions */
enum {
    K_A = 0, /* (0,0) */
    K_B,
    K_MO1,
    K_TG2,
    K_LT1,
    K_MT,
    K_OSM = COLS, /* (1,0) */
    K_OSL3,
    K_EXCL,
    K_MUTE,
    K_BOOT,
    K_TO3,
};

static const kb_keycode_t keymap_def[LAYERS][ROWS][COLS] = {
    {
        {KC_A, KC_B, MO(1), TG(2), LT(1, KC_SPC), LCTL_T(KC_ESC)},
        {OSM(MOD_LSFT), OSL(3), LSFT(KC_1), KC_MUTE, QK_BOOT, TO(3)},
    },
    {
        {KC_1, KC_2, _______, _______, _______, _______},
        {_______, _______, _______, KC_SLEP, _______, _______},
    },
    {
        {KC_X, _______, _______, _______, _______, _______},
        {_______, _______, _______, _______, _______, _______},
    },
    {
        {KC_F1, KC_NO, _______, _______, _______, _______},
        {_______, _______, _______, _______, _______, TO(0)},
    },
};

static kb_keycode_t storage[LAYERS * ROWS * COLS];
static struct kb_keymap keymap;
static struct kb_engine eng;

static int reports;
static uint8_t boot[8];
static struct kb_hid_state last;
static kb_keycode_t last_action;
static int actions;
static uint32_t last_layers;
static uint8_t last_highest;

static void on_report(const struct kb_hid_state *hid, void *ctx)
{
    (void)ctx;
    reports++;
    last = *hid;
    kb_hid_boot_report(hid, boot);
}

static void on_action(kb_keycode_t kc, bool pressed, void *ctx)
{
    (void)ctx;
    if (pressed) {
        last_action = kc;
        actions++;
    }
}

static void on_layers(uint32_t mask, uint8_t highest, void *ctx)
{
    (void)ctx;
    last_layers = mask;
    last_highest = highest;
}

static const struct kb_engine_ops ops = {
    .report = on_report,
    .action = on_action,
    .layers = on_layers,
};

static void setup(void)
{
    kb_keymap_init(&keymap, storage, LAYERS, ROWS, COLS, &keymap_def[0][0][0]);
    kb_engine_init(&eng, &keymap, &ops, NULL);
    reports = 0;
    memset(boot, 0, sizeof(boot));
    memset(&last, 0, sizeof(last));
    last_action = KC_NO;
    actions = 0;
    last_layers = 1;
    last_highest = 0;
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

static void test_basic_press_release(void)
{
    setup();
    key(K_A, true, 0);
    KB_CHECK_EQ(reports, 1);
    KB_CHECK_EQ(boot[2], KC_A);
    KB_CHECK(kb_hid_key_held(&last, KC_A));
    key(K_A, false, 10);
    KB_CHECK_EQ(reports, 2);
    KB_CHECK_EQ(boot[2], 0);
    KB_CHECK(!kb_hid_key_held(&last, KC_A));
}

static void test_boot_report_keeps_press_order(void)
{
    setup();
    key(K_B, true, 0);
    key(K_A, true, 1);
    KB_CHECK_EQ(boot[2], KC_B);
    KB_CHECK_EQ(boot[3], KC_A);
    key(K_B, false, 2);
    KB_CHECK_EQ(boot[2], KC_A);
    KB_CHECK_EQ(boot[3], 0);
}

static void test_boot_report_overflow_goes_to_nkro_only(void)
{
    /* Seven distinct keys on a one-layer, one-row keymap. */
    kb_keycode_t def[7] = {KC_A, KC_B, KC_C, KC_D, KC_E, KC_F, KC_G};
    kb_keycode_t store[7];
    struct kb_keymap km;
    kb_keymap_init(&km, store, 1, 1, 7, def);
    setup();
    kb_engine_init(&eng, &km, &ops, NULL);

    for (uint8_t c = 0; c < 7; c++) {
        kb_engine_key(&eng, 0, c, true, c);
    }
    KB_CHECK_EQ(boot[2], KC_A);
    KB_CHECK_EQ(boot[7], KC_F);
    KB_CHECK(kb_hid_key_held(&last, KC_G));
}

static void test_momentary_layer(void)
{
    setup();
    key(K_MO1, true, 0);
    KB_CHECK_EQ(last_layers, 0x3);
    KB_CHECK_EQ(last_highest, 1);
    key(K_A, true, 1);
    KB_CHECK_EQ(boot[2], KC_1);
    key(K_B, true, 2);
    KB_CHECK_EQ(boot[3], KC_2);
    key(K_MO1, false, 3);
    KB_CHECK_EQ(last_layers, 0x1);
    /* Keys pressed on layer 1 release what they pressed. */
    key(K_A, false, 4);
    KB_CHECK(!kb_hid_key_held(&last, KC_1));
    KB_CHECK(!kb_hid_key_held(&last, KC_A));
    key(K_B, false, 5);
    KB_CHECK_EQ(boot[2], 0);
}

static void test_transparent_falls_through(void)
{
    setup();
    key(K_MO1, true, 0);
    key(K_EXCL, true, 1); /* transparent on layer 1 */
    KB_CHECK_EQ(boot[0], 0x02);
    KB_CHECK_EQ(boot[2], KC_1);
}

static void test_toggle_layer(void)
{
    setup();
    tap(K_TG2, 0);
    KB_CHECK_EQ(last_layers, 0x5);
    key(K_A, true, 20);
    KB_CHECK_EQ(boot[2], KC_X);
    key(K_A, false, 21);
    tap(K_TG2, 30);
    KB_CHECK_EQ(last_layers, 0x1);
    key(K_A, true, 40);
    KB_CHECK_EQ(boot[2], KC_A);
}

static void test_to_layer(void)
{
    setup();
    tap(K_TG2, 0);
    tap(K_TO3, 10);
    KB_CHECK_EQ(last_layers, 0x9); /* layer 2 off, 3 on, default 0 */
    KB_CHECK_EQ(last_highest, 3);
    key(K_A, true, 20);
    KB_CHECK_EQ(boot[2], KC_F1);
    key(K_A, false, 21);
    key(K_B, true, 22); /* KC_NO on layer 3 is opaque */
    KB_CHECK_EQ(boot[2], 0);
    key(K_B, false, 23);
    tap(K_TO3, 30); /* TO(0) on layer 3 */
    KB_CHECK_EQ(last_layers, 0x1);
}

static void test_default_layer(void)
{
    kb_keycode_t def[2][1][2] = {{{DF(1), KC_A}}, {{DF(0), KC_B}}};
    kb_keycode_t store[4];
    struct kb_keymap km;
    kb_keymap_init(&km, store, 2, 1, 2, &def[0][0][0]);
    setup();
    kb_engine_init(&eng, &km, &ops, NULL);

    kb_engine_key(&eng, 0, 0, true, 0);
    kb_engine_key(&eng, 0, 0, false, 1);
    KB_CHECK_EQ(kb_engine_layer_mask(&eng), 0x2);
    kb_engine_key(&eng, 0, 1, true, 2);
    KB_CHECK_EQ(boot[2], KC_B);
    kb_engine_key(&eng, 0, 1, false, 3);
    kb_engine_key(&eng, 0, 0, true, 4);
    kb_engine_key(&eng, 0, 0, false, 5);
    KB_CHECK_EQ(kb_engine_layer_mask(&eng), 0x1);
}

static void test_two_momentary_keys_share_a_layer(void)
{
    kb_keycode_t def[2][1][3] = {{{MO(1), MO(1), KC_A}}, {{_______, _______, KC_B}}};
    kb_keycode_t store[6];
    struct kb_keymap km;
    kb_keymap_init(&km, store, 2, 1, 3, &def[0][0][0]);
    setup();
    kb_engine_init(&eng, &km, &ops, NULL);

    kb_engine_key(&eng, 0, 0, true, 0);
    kb_engine_key(&eng, 0, 1, true, 1);
    kb_engine_key(&eng, 0, 0, false, 2);
    KB_CHECK_EQ(kb_engine_layer_mask(&eng), 0x3); /* still held by the other key */
    kb_engine_key(&eng, 0, 1, false, 3);
    KB_CHECK_EQ(kb_engine_layer_mask(&eng), 0x1);
}

static void test_modded_key(void)
{
    setup();
    key(K_EXCL, true, 0);
    KB_CHECK_EQ(boot[0], 0x02);
    KB_CHECK_EQ(boot[2], KC_1);
    key(K_EXCL, false, 1);
    KB_CHECK_EQ(boot[0], 0);
    KB_CHECK_EQ(boot[2], 0);
}

static void test_modifier_shared_by_two_keys(void)
{
    /* Real Shift held, then LSFT(KC_1) pressed and released: Shift stays. */
    kb_keycode_t def[3] = {KC_LSFT, LSFT(KC_1), KC_A};
    kb_keycode_t store[3];
    struct kb_keymap km;
    kb_keymap_init(&km, store, 1, 1, 3, def);
    setup();
    kb_engine_init(&eng, &km, &ops, NULL);

    kb_engine_key(&eng, 0, 0, true, 0);
    kb_engine_key(&eng, 0, 1, true, 1);
    kb_engine_key(&eng, 0, 1, false, 2);
    KB_CHECK_EQ(boot[0], 0x02);
    kb_engine_key(&eng, 0, 0, false, 3);
    KB_CHECK_EQ(boot[0], 0);
}

static void test_consumer_and_system(void)
{
    setup();
    key(K_MUTE, true, 0);
    KB_CHECK_EQ(last.consumer, 0xE2);
    key(K_MUTE, false, 1);
    KB_CHECK_EQ(last.consumer, 0);

    key(K_MO1, true, 2);
    key(K_MUTE, true, 3); /* KC_SLEP on layer 1 */
    KB_CHECK_EQ(last.system, 0x82);
    key(K_MUTE, false, 4);
    KB_CHECK_EQ(last.system, 0);
}

static void test_one_shot_mod(void)
{
    setup();
    tap(K_OSM, 0);
    KB_CHECK_EQ(boot[0], 0); /* nothing sent until the next key */
    key(K_A, true, 10);
    KB_CHECK_EQ(boot[0], 0x02);
    KB_CHECK_EQ(boot[2], KC_A);
    key(K_A, false, 11);
    KB_CHECK_EQ(boot[0], 0);
    key(K_B, true, 12); /* used up */
    KB_CHECK_EQ(boot[0], 0);
}

static void test_one_shot_mod_held_acts_as_mod(void)
{
    setup();
    key(K_OSM, true, 0);
    KB_CHECK_EQ(boot[0], 0x02);
    tap(K_A, 10);
    tap(K_B, 20);
    key(K_OSM, false, 30);
    KB_CHECK_EQ(boot[0], 0);
    key(K_A, true, 40); /* not one-shot, since it was used while held */
    KB_CHECK_EQ(boot[0], 0);
}

static void test_one_shot_layer(void)
{
    setup();
    tap(K_OSL3, 0);
    KB_CHECK_EQ(last_layers, 0x9);
    key(K_A, true, 10);
    KB_CHECK_EQ(boot[2], KC_F1);
    KB_CHECK_EQ(last_layers, 0x1); /* gone after one key */
    key(K_A, false, 11);
    KB_CHECK(!kb_hid_key_held(&last, KC_F1));
    key(K_A, true, 20);
    KB_CHECK_EQ(boot[2], KC_A);
}

static void test_one_shot_layer_and_mod_stack(void)
{
    setup();
    tap(K_OSL3, 0);
    tap(K_OSM, 10); /* transparent on layer 3: still OSM, keeps the layer */
    key(K_A, true, 20);
    KB_CHECK_EQ(boot[0], 0x02);
    KB_CHECK_EQ(boot[2], KC_F1);
}

static void test_layer_tap_tap(void)
{
    setup();
    key(K_LT1, true, 0);
    KB_CHECK_EQ(reports, 0);
    key(K_LT1, false, 100);
    KB_CHECK_EQ(reports, 2); /* press then release of Space */
    KB_CHECK_EQ(boot[2], 0);
    KB_CHECK_EQ(last_layers, 0x1);
}

static void test_layer_tap_hold_by_timeout(void)
{
    setup();
    key(K_LT1, true, 0);
    kb_engine_tick(&eng, 199);
    KB_CHECK_EQ(last_layers, 0x1);
    kb_engine_tick(&eng, 200);
    KB_CHECK_EQ(last_layers, 0x3);
    key(K_A, true, 250);
    KB_CHECK_EQ(boot[2], KC_1);
    key(K_A, false, 260);
    key(K_LT1, false, 300);
    KB_CHECK_EQ(last_layers, 0x1);
    KB_CHECK_EQ(reports, 2); /* no Space */
}

static void test_idle_only_without_timers(void)
{
    setup();
    KB_CHECK(kb_engine_idle(&eng));
    /* A plain key held waits on nothing. */
    key(K_A, true, 0);
    KB_CHECK(kb_engine_idle(&eng));
    key(K_A, false, 10);
    /* An undecided layer-tap waits on its tapping term. */
    key(K_LT1, true, 20);
    KB_CHECK(!kb_engine_idle(&eng));
    kb_engine_tick(&eng, 220);
    KB_CHECK(kb_engine_idle(&eng)); /* decided: a hold */
    key(K_LT1, false, 300);
    KB_CHECK(kb_engine_idle(&eng));
}

static void test_layer_tap_waits_for_decision(void)
{
    /* Hold LT, press A within the term, keep holding: A waits, then lands on layer 1. */
    setup();
    key(K_LT1, true, 0);
    key(K_A, true, 50);
    KB_CHECK_EQ(reports, 0);
    kb_engine_tick(&eng, 200);
    KB_CHECK_EQ(boot[2], KC_1);
    KB_CHECK(!kb_hid_key_held(&last, KC_SPC));
}

static void test_layer_tap_rolled_over_is_a_tap(void)
{
    /* Fast typing: LT down, A down, LT up, A up, all within the term. */
    setup();
    key(K_LT1, true, 0);
    key(K_A, true, 30);
    key(K_LT1, false, 60);
    KB_CHECK(kb_hid_key_held(&last, KC_A));
    KB_CHECK(!kb_hid_key_held(&last, KC_1));
    KB_CHECK(!kb_hid_key_held(&last, KC_SPC));
    key(K_A, false, 90);
    KB_CHECK_EQ(boot[2], 0);
    /* Space pressed, A pressed, Space released, A released */
    KB_CHECK_EQ(reports, 4);
}

static void test_mod_tap(void)
{
    setup();
    tap(K_MT, 0);
    KB_CHECK_EQ(reports, 2);
    KB_CHECK_EQ(boot[0], 0);

    key(K_MT, true, 100);
    kb_engine_tick(&eng, 300);
    KB_CHECK_EQ(boot[0], 0x01);
    KB_CHECK_EQ(boot[2], 0);
    tap(K_A, 310);
    key(K_MT, false, 400);
    KB_CHECK_EQ(boot[0], 0);
}

static void test_mod_tap_decided_by_a_late_event(void)
{
    /* No tick: an event after the term decides the hold first. */
    setup();
    key(K_MT, true, 0);
    key(K_A, true, 250);
    KB_CHECK_EQ(boot[0], 0x01);
    KB_CHECK_EQ(boot[2], KC_A);
}

static void test_tap_hold_queue_overflow_forces_hold(void)
{
    setup();
    key(K_MT, true, 0);
    for (int i = 0; i < 20; i++) {
        key(K_A, (i & 1) == 0, (uint32_t)(1 + i));
    }
    KB_CHECK_EQ(boot[0], 0x01);
}

static void test_tapping_term_setting(void)
{
    setup();
    kb_engine_set_tapping_term(&eng, 50);
    key(K_LT1, true, 0);
    kb_engine_tick(&eng, 50);
    KB_CHECK_EQ(last_layers, 0x3);
}

static void rule(uint8_t flags, uint16_t quick_tap_ms)
{
    struct kb_tap_hold_rule r = {.flags = flags, .tapping_term_ms = 200, .quick_tap_ms = quick_tap_ms};
    kb_engine_set_tap_hold(&eng, &r, NULL, 0);
}

static void test_permissive_hold(void)
{
    /* LT down, A tapped inside it, all within the term: A on layer 1. */
    setup();
    rule(KB_TH_PERMISSIVE_HOLD, 0);
    key(K_LT1, true, 0);
    key(K_A, true, 30);
    KB_CHECK_EQ(reports, 0); /* a press alone doesn't decide */
    key(K_A, false, 60);
    KB_CHECK_EQ(last_layers, 0x3);
    KB_CHECK(!kb_hid_key_held(&last, KC_SPC));
    KB_CHECK_EQ(reports, 2); /* 1 pressed and released */
    key(K_LT1, false, 90);
    KB_CHECK_EQ(last_layers, 0x1);

    /* Rolled over (LT released first) is still a tap. */
    setup();
    rule(KB_TH_PERMISSIVE_HOLD, 0);
    key(K_LT1, true, 0);
    key(K_A, true, 30);
    key(K_LT1, false, 60);
    KB_CHECK(kb_hid_key_held(&last, KC_A));
    KB_CHECK(!kb_hid_key_held(&last, KC_1));
}

static void test_hold_on_other_key_press(void)
{
    setup();
    rule(KB_TH_HOLD_ON_OTHER_PRESS, 0);
    key(K_MT, true, 0);
    key(K_A, true, 20);
    KB_CHECK_EQ(boot[0], 0x01);
    KB_CHECK_EQ(boot[2], KC_A);
}

static void test_quick_tap_repeats_the_tap(void)
{
    setup();
    rule(0, 150);
    tap(K_LT1, 0);
    key(K_LT1, true, 100); /* again within 150 ms: Space at once, held */
    KB_CHECK(kb_hid_key_held(&last, KC_SPC));
    kb_engine_tick(&eng, 400);
    KB_CHECK_EQ(last_layers, 0x1);
    key(K_LT1, false, 500);
    KB_CHECK(!kb_hid_key_held(&last, KC_SPC));

    /* Too late: an ordinary tap-hold again. */
    key(K_LT1, true, 800);
    KB_CHECK_EQ(reports, 4);
    kb_engine_tick(&eng, 1000);
    KB_CHECK_EQ(last_layers, 0x3);
}

static void test_retro_tapping(void)
{
    setup();
    rule(KB_TH_RETRO_TAP, 0);
    key(K_MT, true, 0);
    kb_engine_tick(&eng, 300);
    KB_CHECK_EQ(boot[0], 0x01);
    int before = reports;
    key(K_MT, false, 400);
    /* Ctrl released with Esc pressed, then Esc released */
    KB_CHECK_EQ(reports, before + 2);
    KB_CHECK_EQ(boot[0], 0);
    KB_CHECK_EQ(boot[2], 0);

    /* Used as a modifier: no tap afterwards. */
    key(K_MT, true, 1000);
    kb_engine_tick(&eng, 1300);
    tap(K_A, 1310);
    before = reports;
    key(K_MT, false, 1400);
    KB_CHECK_EQ(reports, before + 1);
}

static void test_per_key_rule(void)
{
    static const struct kb_tap_hold_rule keys[] = {
        {.row = K_MT / COLS, .col = K_MT % COLS, .flags = KB_TH_HOLD_ON_OTHER_PRESS, .tapping_term_ms = 100},
    };
    setup();
    kb_engine_set_tap_hold(&eng, NULL, keys, 1);
    key(K_MT, true, 0);
    kb_engine_tick(&eng, 100);
    KB_CHECK_EQ(boot[0], 0x01);
    key(K_MT, false, 110);
    /* Other keys keep the default: LT with A pressed waits. */
    key(K_LT1, true, 200);
    key(K_A, true, 220);
    KB_CHECK_EQ(last_layers, 0x1);
    kb_engine_tick(&eng, 399);
    KB_CHECK_EQ(last_layers, 0x1);
    kb_engine_tick(&eng, 400);
    KB_CHECK_EQ(last_layers, 0x3);
}

static void test_action_callback(void)
{
    setup();
    key(K_BOOT, true, 0);
    KB_CHECK_EQ(actions, 1);
    KB_CHECK_EQ(last_action, QK_BOOT);
    KB_CHECK_EQ(reports, 0);
}

static void test_out_of_range_and_duplicates_ignored(void)
{
    setup();
    kb_engine_key(&eng, ROWS, 0, true, 0);
    kb_engine_key(&eng, 0, COLS, true, 0);
    kb_engine_key(&eng, 255, 255, true, 0);
    KB_CHECK_EQ(reports, 0);
    key(K_A, true, 1);
    key(K_A, true, 2);
    key(K_A, false, 3);
    KB_CHECK_EQ(boot[2], 0);
    key(K_A, false, 4); /* release of a key that is up */
    KB_CHECK_EQ(reports, 2);
}

static void test_matrix_diff(void)
{
    setup();
    kb_matrix_row_t prev[ROWS] = {0, 0};
    kb_matrix_row_t now[ROWS] = {0x3, 0x0}; /* A and B */
    kb_engine_matrix(&eng, prev, now, 0);
    KB_CHECK_EQ(boot[2], KC_A);
    KB_CHECK_EQ(boot[3], KC_B);
    kb_matrix_row_t up[ROWS] = {0x2, 0x0};
    kb_engine_matrix(&eng, now, up, 1);
    KB_CHECK_EQ(boot[2], KC_B);
}

static void test_timestamps_wrap(void)
{
    setup();
    key(K_LT1, true, UINT32_MAX - 99);
    kb_engine_tick(&eng, 50);
    KB_CHECK_EQ(last_layers, 0x1);
    kb_engine_tick(&eng, 100);
    KB_CHECK_EQ(last_layers, 0x3);
}

int main(void)
{
    printf("test_engine\n");
    KB_RUN(test_basic_press_release);
    KB_RUN(test_boot_report_keeps_press_order);
    KB_RUN(test_boot_report_overflow_goes_to_nkro_only);
    KB_RUN(test_momentary_layer);
    KB_RUN(test_transparent_falls_through);
    KB_RUN(test_toggle_layer);
    KB_RUN(test_to_layer);
    KB_RUN(test_default_layer);
    KB_RUN(test_two_momentary_keys_share_a_layer);
    KB_RUN(test_modded_key);
    KB_RUN(test_modifier_shared_by_two_keys);
    KB_RUN(test_consumer_and_system);
    KB_RUN(test_one_shot_mod);
    KB_RUN(test_one_shot_mod_held_acts_as_mod);
    KB_RUN(test_one_shot_layer);
    KB_RUN(test_one_shot_layer_and_mod_stack);
    KB_RUN(test_layer_tap_tap);
    KB_RUN(test_layer_tap_hold_by_timeout);
    KB_RUN(test_idle_only_without_timers);
    KB_RUN(test_layer_tap_waits_for_decision);
    KB_RUN(test_layer_tap_rolled_over_is_a_tap);
    KB_RUN(test_mod_tap);
    KB_RUN(test_mod_tap_decided_by_a_late_event);
    KB_RUN(test_tap_hold_queue_overflow_forces_hold);
    KB_RUN(test_tapping_term_setting);
    KB_RUN(test_permissive_hold);
    KB_RUN(test_hold_on_other_key_press);
    KB_RUN(test_quick_tap_repeats_the_tap);
    KB_RUN(test_retro_tapping);
    KB_RUN(test_per_key_rule);
    KB_RUN(test_action_callback);
    KB_RUN(test_out_of_range_and_duplicates_ignored);
    KB_RUN(test_matrix_diff);
    KB_RUN(test_timestamps_wrap);
    return KB_TEST_RESULT();
}
