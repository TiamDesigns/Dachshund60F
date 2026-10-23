#include "kb/keycodes.h"
#include "kb_test.h"

static void test_ranges_do_not_overlap(void)
{
    KB_CHECK_EQ(kb_keycode_kind(KC_NO, 4), KB_KIND_NO);
    KB_CHECK_EQ(kb_keycode_kind(KC_TRNS, 4), KB_KIND_TRNS);
    KB_CHECK_EQ(kb_keycode_kind(KC_A, 4), KB_KIND_BASIC);
    KB_CHECK_EQ(kb_keycode_kind(KC_RGUI, 4), KB_KIND_BASIC);
    KB_CHECK_EQ(kb_keycode_kind(KC_MUTE, 4), KB_KIND_CONSUMER);
    KB_CHECK_EQ(kb_keycode_kind(KC_CALC, 4), KB_KIND_CONSUMER);
    KB_CHECK_EQ(kb_keycode_kind(KC_SLEP, 4), KB_KIND_SYSTEM);
    KB_CHECK_EQ(kb_keycode_kind(LSFT(KC_1), 4), KB_KIND_MODDED);
    KB_CHECK_EQ(kb_keycode_kind(MO(3), 4), KB_KIND_LAYER);
    KB_CHECK_EQ(kb_keycode_kind(OSM(MOD_LSFT), 4), KB_KIND_OSM);
    KB_CHECK_EQ(kb_keycode_kind(LT(1, KC_SPC), 4), KB_KIND_LT);
    KB_CHECK_EQ(kb_keycode_kind(QK_BOOT, 4), KB_KIND_ACTION);
    KB_CHECK_EQ(kb_keycode_kind(BT_SEL(2), 4), KB_KIND_ACTION);
    KB_CHECK_EQ(kb_keycode_kind(MS_WHLD, 4), KB_KIND_ACTION);
    KB_CHECK_EQ(kb_keycode_kind(LCTL_T(KC_A), 4), KB_KIND_MT);
}

static void test_invalid_keycodes(void)
{
    /* HID error usages and gaps */
    KB_CHECK(!kb_keycode_is_valid(0x0002, 4));
    KB_CHECK(!kb_keycode_is_valid(0x0003, 4));
    KB_CHECK(!kb_keycode_is_valid(0x00A5, 4));
    KB_CHECK(!kb_keycode_is_valid(0x00E8, 4));
    KB_CHECK(!kb_keycode_is_valid(0x0100, 4));
    KB_CHECK(!kb_keycode_is_valid(KB_KC_CONSUMER, 4));
    KB_CHECK(!kb_keycode_is_valid(KB_KC_SYSTEM | 0x80u, 4));
    /* Modded and mod-tap keys need a modifier and a real key. */
    KB_CHECK(!kb_keycode_is_valid(KB_KC_MODS(0, KC_A), 4));
    KB_CHECK(!kb_keycode_is_valid(KB_KC_MODS(MOD_RIGHT, KC_A), 4));
    KB_CHECK(!kb_keycode_is_valid(LSFT(0x00A5), 4));
    KB_CHECK(!kb_keycode_is_valid(MT(0, KC_A), 4));
    KB_CHECK(!kb_keycode_is_valid(LCTL_T(KC_NO), 4));
    KB_CHECK(!kb_keycode_is_valid(OSM(0), 4));
    KB_CHECK(!kb_keycode_is_valid(OSM(MOD_RIGHT), 4));
    /* Layers must exist. */
    KB_CHECK(kb_keycode_is_valid(MO(3), 4));
    KB_CHECK(!kb_keycode_is_valid(MO(4), 4));
    KB_CHECK(!kb_keycode_is_valid(LT(4, KC_A), 4));
    KB_CHECK(!kb_keycode_is_valid(KB_KC_LAYER_OP(KB_LAYER_OP_COUNT, 0), 4));
    /* Unassigned actions and reserved space */
    KB_CHECK(!kb_keycode_is_valid(0x7005, 4));
    KB_CHECK(!kb_keycode_is_valid(BT_SEL(0) + KB_BT_PROFILE_MAX, 4));
    KB_CHECK(!kb_keycode_is_valid(0xA000, 4));
    KB_CHECK(!kb_keycode_is_valid(0xFFFF, 4));
}

static void test_every_keycode_is_classified(void)
{
    /* No crash and a sane answer for all 65536 values. */
    unsigned valid = 0;
    for (unsigned kc = 0; kc <= 0xFFFFu; kc++) {
        enum kb_keycode_kind kind = kb_keycode_kind((kb_keycode_t)kc, 16);
        KB_CHECK(kind >= KB_KIND_INVALID && kind <= KB_KIND_MT);
        if (kind != KB_KIND_INVALID) {
            valid++;
        }
    }
    KB_CHECK(valid > 1000);
}

static void test_fields(void)
{
    KB_CHECK_EQ(kb_keycode_key(LSFT(KC_1)), KC_1);
    KB_CHECK_EQ(kb_keycode_mods(LSFT(KC_1)), MOD_LSFT);
    KB_CHECK_EQ(kb_keycode_key(LT(2, KC_SPC)), KC_SPC);
    KB_CHECK_EQ(kb_keycode_mods(RALT_T(KC_ENT)), MOD_RALT);
    KB_CHECK_EQ(kb_mods_to_hid(MOD_LCTL | MOD_LSFT), 0x03);
    KB_CHECK_EQ(kb_mods_to_hid(MOD_RSFT), 0x20);
    KB_CHECK_EQ(kb_mods_to_hid(MOD_RGUI), 0x80);
    KB_CHECK_EQ(kb_mods_to_hid(MOD_RIGHT | MOD_LCTL | MOD_LALT), 0x50);
}

static void test_hid_usages(void)
{
    /* Spot checks against the HID usage tables. */
    KB_CHECK_EQ(KC_A, 0x04);
    KB_CHECK_EQ(KC_0, 0x27);
    KB_CHECK_EQ(KC_ESC, 0x29);
    KB_CHECK_EQ(KC_CAPS, 0x39);
    KB_CHECK_EQ(KC_UP, 0x52);
    KB_CHECK_EQ(KC_F24, 0x73);
    KB_CHECK_EQ(KC_LCTL, 0xE0);
    KB_CHECK_EQ(KC_VOLU & 0x3FF, 0xE9);
    KB_CHECK_EQ(KC_SLEP & 0xFF, 0x82);
}

int main(void)
{
    printf("test_keycodes\n");
    KB_RUN(test_ranges_do_not_overlap);
    KB_RUN(test_invalid_keycodes);
    KB_RUN(test_every_keycode_is_classified);
    KB_RUN(test_fields);
    KB_RUN(test_hid_usages);
    return KB_TEST_RESULT();
}
