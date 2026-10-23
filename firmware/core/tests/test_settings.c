#include "kb/debounce.h"
#include "kb/keycodes.h"
#include "kb/protocol_gen.h"
#include "kb/settings.h"
#include "kb_test.h"

#include <string.h>

#define L 2
#define R 2
#define C 3

static const kb_keycode_t defaults[L][R][C] = {
    {{KC_A, KC_B, MO(1)}, {KC_1, KC_2, KC_3}},
    {{KC_F1, _______, _______}, {_______, KC_VOLU, QK_BOOT}},
};

static kb_keycode_t keys[L * R * C];
static struct kb_keymap km;
static struct kb_settings st;

static const struct kb_settings default_settings = {
    .debounce_algo = KB_DEBOUNCE_EAGER_PRESS,
    .debounce_ms = 5,
    .tapping_term_ms = 200,
    .nkro = true,
    .sleep_timeout_min = 10,
    .indicator_brightness = 25,
    .name = "",
};

static void reset(void)
{
    kb_keymap_init(&km, keys, L, R, C, &defaults[0][0][0]);
    st = default_settings;
}

static void test_defaults_valid(void)
{
    KB_CHECK(kb_settings_valid(&default_settings));
    struct kb_settings s = default_settings;
    s.debounce_algo = KB_DEBOUNCE_ALGO_COUNT;
    KB_CHECK(!kb_settings_valid(&s));
    s = default_settings;
    s.tapping_term_ms = 10;
    KB_CHECK(!kb_settings_valid(&s));
    s = default_settings;
    s.indicator_brightness = 101;
    KB_CHECK(!kb_settings_valid(&s));
}

static void test_round_trip(void)
{
    reset();
    kb_keymap_set(&km, 0, 0, 0, KC_Z);
    kb_keymap_set(&km, 1, 1, 1, LSFT(KC_9));
    st.debounce_ms = 12;
    st.tapping_term_ms = 333;
    st.nkro = false;
    memcpy(st.name, "Oskar's board!!!", 16);

    uint8_t buf[256];
    size_t n = kb_settings_encode(&km, &st, buf, sizeof(buf));
    KB_CHECK_EQ(n, kb_settings_encoded_size(&km));
    KB_CHECK_EQ(kb_settings_encode(&km, &st, buf, n - 1), 0);

    reset();
    KB_CHECK_EQ(kb_settings_decode(KB_SETTINGS_SCHEMA, buf, n, &km, &st),
                KB_LOADED_KEYMAP | KB_LOADED_SETTINGS);
    KB_CHECK_EQ(kb_keymap_get(&km, 0, 0, 0), KC_Z);
    KB_CHECK_EQ(kb_keymap_get(&km, 1, 1, 1), LSFT(KC_9));
    KB_CHECK_EQ(kb_keymap_get(&km, 1, 1, 2), QK_BOOT);
    KB_CHECK_EQ(st.debounce_ms, 12);
    KB_CHECK_EQ(st.tapping_term_ms, 333);
    KB_CHECK(!st.nkro);
    KB_CHECK(strcmp(st.name, "Oskar's board!!!") == 0);
}

static void test_other_geometry_keeps_default_keymap(void)
{
    /* Saved by a build with 3 layers: settings load, the keymap doesn't. */
    static kb_keycode_t big_keys[3 * R * C];
    struct kb_keymap big;
    kb_keymap_init(&big, big_keys, 3, R, C, NULL);
    kb_keymap_set(&big, 0, 0, 0, KC_Q);
    struct kb_settings s = default_settings;
    s.debounce_ms = 9;

    uint8_t buf[256];
    size_t n = kb_settings_encode(&big, &s, buf, sizeof(buf));
    reset();
    KB_CHECK_EQ(kb_settings_decode(KB_SETTINGS_SCHEMA, buf, n, &km, &st), KB_LOADED_SETTINGS);
    KB_CHECK_EQ(kb_keymap_get(&km, 0, 0, 0), KC_A);
    KB_CHECK_EQ(st.debounce_ms, 9);
}

static void test_bad_values_fall_back(void)
{
    reset();
    uint8_t buf[256];
    size_t n = kb_settings_encode(&km, &st, buf, sizeof(buf));
    /* Keymap section: tag, len (2), layers, rows, cols, then keycodes. */
    buf[3 + 3] = 0xFF;
    buf[3 + 3 + 1] = 0xFF; /* key 0: undefined keycode */
    size_t settings = 3 + 3 + 2 * L * R * C;
    KB_CHECK_EQ(buf[settings], KB_SECTION_SETTINGS);
    buf[settings + 3 + 0] = 77;   /* debounce algorithm: no such */
    buf[settings + 3 + 2] = 1;    /* tapping term 1 ms */
    buf[settings + 3 + 3] = 0;
    buf[settings + 3 + 4] = 1;    /* nkro: fine */
    buf[settings + 3 + 6] = 200;  /* brightness out of range */

    reset();
    st.nkro = false;
    KB_CHECK_EQ(kb_settings_decode(KB_SETTINGS_SCHEMA, buf, n, &km, &st),
                KB_LOADED_KEYMAP | KB_LOADED_SETTINGS);
    KB_CHECK_EQ(kb_keymap_get(&km, 0, 0, 0), KC_NO);
    KB_CHECK_EQ(kb_keymap_get(&km, 0, 0, 1), KC_B);
    KB_CHECK_EQ(st.debounce_algo, default_settings.debounce_algo);
    KB_CHECK_EQ(st.tapping_term_ms, default_settings.tapping_term_ms);
    KB_CHECK_EQ(st.indicator_brightness, default_settings.indicator_brightness);
    KB_CHECK(st.nkro);
}

static void test_forward_compatible_sections(void)
{
    /* An unknown section, then a settings section with only two fields
     * (from older firmware) and one with an extra field (from newer).
     */
    const uint8_t blob[] = {
        9, 2, 0, 0xAA, 0xBB,                  /* unknown tag 9: skipped */
        KB_SECTION_SETTINGS, 2, 0, KB_DEBOUNCE_DEFER_GLOBAL, 15,
    };
    reset();
    KB_CHECK_EQ(kb_settings_decode(KB_SETTINGS_SCHEMA, blob, sizeof(blob), &km, &st),
                KB_LOADED_SETTINGS);
    KB_CHECK_EQ(st.debounce_algo, KB_DEBOUNCE_DEFER_GLOBAL);
    KB_CHECK_EQ(st.debounce_ms, 15);
    KB_CHECK_EQ(st.tapping_term_ms, 200);
    KB_CHECK_EQ(kb_keymap_get(&km, 0, 0, 0), KC_A);
}

static void test_malformed_and_newer(void)
{
    reset();
    uint8_t buf[256];
    size_t n = kb_settings_encode(&km, &st, buf, sizeof(buf));
    kb_keymap_set(&km, 0, 0, 0, KC_X);

    /* Truncated: rejected, nothing changes. */
    KB_CHECK_EQ(kb_settings_decode(KB_SETTINGS_SCHEMA, buf, n - 1, &km, &st), -1);
    KB_CHECK_EQ(kb_keymap_get(&km, 0, 0, 0), KC_X);
    /* A section length running past the end. */
    uint8_t bad[] = {KB_SECTION_SETTINGS, 50, 0, 1, 2};
    KB_CHECK_EQ(kb_settings_decode(KB_SETTINGS_SCHEMA, bad, sizeof(bad), &km, &st), -1);
    /* Saved by newer firmware with a schema this build doesn't know. */
    KB_CHECK_EQ(kb_settings_decode(KB_SETTINGS_SCHEMA + 1, buf, n, &km, &st), -1);
    KB_CHECK_EQ(kb_settings_decode(0, buf, n, &km, &st), -1);
    KB_CHECK_EQ(kb_keymap_get(&km, 0, 0, 0), KC_X);
    /* Empty is valid: nothing loaded. */
    KB_CHECK_EQ(kb_settings_decode(KB_SETTINGS_SCHEMA, buf, 0, &km, &st), 0);
}

static struct kb_features feat, loaded;
static uint8_t blob[4096];

/* Settings and features in one blob, as the firmware saves them. */
static size_t encode_all(void)
{
    size_t n = kb_settings_encode(&km, &st, blob, sizeof(blob));
    KB_CHECK(n > 0);
    size_t m = kb_features_encode(&feat, km.layers, blob + n, sizeof(blob) - n);
    KB_CHECK_EQ(m, kb_features_encoded_size(&feat, km.layers));
    return n + m;
}

static void test_features_round_trip(void)
{
    reset();
    kb_features_default(&feat);
    feat.combos[3] = (struct kb_combo){.keys = {{0, 0}, {1, 2}, {KB_POS_NONE, KB_POS_NONE},
                                                {KB_POS_NONE, KB_POS_NONE}},
                                       .result = KC_ESC, .timeout_ms = 40, .layers = 1};
    feat.tap_dances[15] = (struct kb_tap_dance){.tap1 = KC_A, .hold1 = KC_LCTL, .tap2 = MO(1)};
    feat.overrides[0] = (struct kb_key_override){.trigger_mods = 0x02, .trigger_key = KC_BSPC,
                                                 .replacement = KC_DEL};
    feat.leader[1] = (struct kb_leader_seq){.keys = {KC_A, KC_B}, .result = KC_VOLU};
    feat.tap_hold[0] = (struct kb_tap_hold_rule){.row = 1, .col = 2, .flags = KB_TH_RETRO_TAP,
                                                 .tapping_term_ms = 250, .quick_tap_ms = 120};
    feat.encoders[1][1] = (struct kb_encoder_action){.cw = KC_MNXT, .ccw = KC_MPRV};
    feat.auto_shift = true;
    feat.caps_word_idle_ms = 0;
    feat.mouse.max_speed = 30;
    const uint8_t m[] = {'h', 'i', KB_MACRO_END, KB_MACRO_TAP, KC_ENT, 0};
    memcpy(feat.macros, m, sizeof(m));
    KB_CHECK(kb_features_valid(&feat, L, R, C));

    size_t n = encode_all();
    KB_CHECK_EQ(kb_features_encode(&feat, km.layers, blob, 10), 0);

    kb_features_default(&loaded);
    KB_CHECK_EQ(kb_features_decode(KB_SETTINGS_SCHEMA, blob, n, &loaded, L, R, C), 1);
    KB_CHECK(memcmp(&loaded.combos[3], &feat.combos[3], sizeof(feat.combos[3])) == 0);
    KB_CHECK_EQ(loaded.tap_dances[15].tap2, MO(1));
    KB_CHECK_EQ(loaded.overrides[0].replacement, KC_DEL);
    KB_CHECK_EQ(loaded.leader[1].result, KC_VOLU);
    KB_CHECK_EQ(loaded.tap_hold[0].quick_tap_ms, 120);
    KB_CHECK_EQ(loaded.encoders[1][1].ccw, KC_MPRV);
    KB_CHECK(loaded.auto_shift);
    KB_CHECK_EQ(loaded.caps_word_idle_ms, 0);
    KB_CHECK_EQ(loaded.mouse.max_speed, 30);
    KB_CHECK(memcmp(loaded.macros, feat.macros, KB_MACRO_BUFFER) == 0);

    /* The settings decoder skips the feature sections, and vice versa. */
    reset();
    KB_CHECK_EQ(kb_settings_decode(KB_SETTINGS_SCHEMA, blob, n, &km, &st),
                KB_LOADED_KEYMAP | KB_LOADED_SETTINGS);
    size_t only = kb_settings_encode(&km, &st, blob, sizeof(blob));
    kb_features_default(&loaded);
    KB_CHECK_EQ(kb_features_decode(KB_SETTINGS_SCHEMA, blob, only, &loaded, L, R, C), 0);
}

static void test_features_invalid_keep_defaults(void)
{
    reset();
    kb_features_default(&feat);
    feat.combos[0] = (struct kb_combo){.keys = {{0, 0}, {1, 2}, {KB_POS_NONE, KB_POS_NONE},
                                                {KB_POS_NONE, KB_POS_NONE}},
                                       .result = KC_ESC};
    size_t n = encode_all();

    /* Saved for a board with more columns: (1,2) is off a 2-column board. */
    kb_features_default(&loaded);
    KB_CHECK_EQ(kb_features_decode(KB_SETTINGS_SCHEMA, blob, n, &loaded, L, R, 2), 0);
    KB_CHECK_EQ(loaded.combos[0].result, KC_NO);

    /* A bad macro step. */
    feat.macros[0] = 0x05;
    n = encode_all();
    KB_CHECK_EQ(kb_features_decode(KB_SETTINGS_SCHEMA, blob, n, &loaded, L, R, C), 0);
    KB_CHECK_EQ(loaded.combos[0].result, KC_NO);

    /* Malformed framing and other schemas. */
    KB_CHECK_EQ(kb_features_decode(KB_SETTINGS_SCHEMA, blob, n - 1, &loaded, L, R, C), -1);
    KB_CHECK_EQ(kb_features_decode(KB_SETTINGS_SCHEMA + 1, blob, n, &loaded, L, R, C), -1);
}

static void test_features_forward_compatible(void)
{
    /* A newer firmware's section: a longer combo entry, then an unknown table. */
    uint8_t b[] = {
        KB_SECTION_FEATURES, 0, 0, /* length below */
        KB_PROTO_TABLE_COMBO, 16, 1, 0, /* one 16-byte entry */
        0, 0, 0, 1, 0xFF, 0xFF, 0xFF, 0xFF, KC_TAB, 0, 0, 0, 0, 0, 0xAB, 0xCD,
        99, 2, 2, 0, 1, 2, 3, 4, /* table 99 */
        /* An older firmware's tap dance: only tap1 */
        KB_PROTO_TABLE_TAP_DANCE, 2, 1, 0, KC_X, 0,
    };
    b[1] = (uint8_t)(sizeof(b) - 3);
    kb_features_default(&loaded);
    loaded.tap_dances[0].term_ms = 300;
    KB_CHECK_EQ(kb_features_decode(KB_SETTINGS_SCHEMA, b, sizeof(b), &loaded, L, R, C), 1);
    KB_CHECK_EQ(loaded.combos[0].result, KC_TAB);
    KB_CHECK_EQ(loaded.combos[0].keys[1].col, 1);
    KB_CHECK_EQ(loaded.tap_dances[0].tap1, KC_X);
    KB_CHECK_EQ(loaded.tap_dances[0].term_ms, 300);

    /* A block claiming more entries than it holds. */
    b[5] = 5;
    kb_features_default(&loaded);
    KB_CHECK_EQ(kb_features_decode(KB_SETTINGS_SCHEMA, b, sizeof(b), &loaded, L, R, C), 0);
    KB_CHECK_EQ(loaded.combos[0].result, KC_NO);
}

static void test_everything_fits_a_sector(void)
{
    /* The firmware's worst case: 4 layers of 6x15 and a full macro buffer, in a 4 KB sector. */
    static kb_keycode_t big_keys[4 * 6 * 15];
    struct kb_keymap big;
    kb_keymap_init(&big, big_keys, 4, 6, 15, NULL);
    kb_features_default(&feat);
    memset(feat.macros, 'x', KB_MACRO_BUFFER);
    size_t n = kb_settings_encoded_size(&big) + kb_features_encoded_size(&feat, 4);
    KB_CHECK(n + 16 <= 4096);
}

int main(void)
{
    KB_RUN(test_defaults_valid);
    KB_RUN(test_round_trip);
    KB_RUN(test_other_geometry_keeps_default_keymap);
    KB_RUN(test_bad_values_fall_back);
    KB_RUN(test_forward_compatible_sections);
    KB_RUN(test_malformed_and_newer);
    KB_RUN(test_features_round_trip);
    KB_RUN(test_features_invalid_keep_defaults);
    KB_RUN(test_features_forward_compatible);
    KB_RUN(test_everything_fits_a_sector);
    return KB_TEST_RESULT();
}
