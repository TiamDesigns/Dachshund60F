#include "kb/debounce.h"
#include "kb/keycodes.h"
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

int main(void)
{
    KB_RUN(test_defaults_valid);
    KB_RUN(test_round_trip);
    KB_RUN(test_other_geometry_keeps_default_keymap);
    KB_RUN(test_bad_values_fall_back);
    KB_RUN(test_forward_compatible_sections);
    KB_RUN(test_malformed_and_newer);
    return KB_TEST_RESULT();
}
