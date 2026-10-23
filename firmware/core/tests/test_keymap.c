#include "kb/keymap.h"
#include "kb_test.h"

#define LAYERS 3
#define ROWS 2
#define COLS 3

static const kb_keycode_t def[LAYERS][ROWS][COLS] = {
    {{KC_A, KC_B, KC_C}, {KC_D, MO(1), KC_NO}},
    {{KC_1, _______, KC_3}, {_______, _______, _______}},
    {{_______, _______, KC_F3}, {_______, _______, _______}},
};

static kb_keycode_t storage[LAYERS * ROWS * COLS];

static void test_init_copies_default(void)
{
    struct kb_keymap km;
    KB_CHECK_EQ(kb_keymap_init(&km, storage, LAYERS, ROWS, COLS, &def[0][0][0]), 0);
    KB_CHECK_EQ(kb_keymap_get(&km, 0, 0, 0), KC_A);
    KB_CHECK_EQ(kb_keymap_get(&km, 0, 1, 1), MO(1));
    KB_CHECK_EQ(kb_keymap_get(&km, 1, 0, 2), KC_3);
    KB_CHECK_EQ(kb_keymap_get(&km, 2, 0, 2), KC_F3);
}

static void test_init_rejects_bad_sizes(void)
{
    struct kb_keymap km;
    KB_CHECK_EQ(kb_keymap_init(&km, storage, 0, ROWS, COLS, NULL), -1);
    KB_CHECK_EQ(kb_keymap_init(&km, storage, KB_MAX_LAYERS + 1, 1, 1, NULL), -1);
    KB_CHECK_EQ(kb_keymap_init(&km, storage, 1, KB_MATRIX_MAX_ROWS + 1, 1, NULL), -1);
    KB_CHECK_EQ(kb_keymap_init(&km, storage, 1, 1, KB_MATRIX_MAX_COLS + 1, NULL), -1);
    KB_CHECK_EQ(kb_keymap_init(&km, NULL, 1, 1, 1, NULL), -1);
}

static void test_init_replaces_invalid_keycodes(void)
{
    kb_keycode_t bad[1][1][3] = {{{KC_A, MO(1), 0xFFFF}}};
    kb_keycode_t store[3];
    struct kb_keymap km;
    /* MO(1) is invalid on a one-layer keymap. */
    KB_CHECK_EQ(kb_keymap_init(&km, store, 1, 1, 3, &bad[0][0][0]), 2);
    KB_CHECK_EQ(kb_keymap_get(&km, 0, 0, 0), KC_A);
    KB_CHECK_EQ(kb_keymap_get(&km, 0, 0, 1), KC_NO);
    KB_CHECK_EQ(kb_keymap_get(&km, 0, 0, 2), KC_NO);
}

static void test_get_out_of_range_is_no(void)
{
    struct kb_keymap km;
    kb_keymap_init(&km, storage, LAYERS, ROWS, COLS, &def[0][0][0]);
    KB_CHECK_EQ(kb_keymap_get(&km, LAYERS, 0, 0), KC_NO);
    KB_CHECK_EQ(kb_keymap_get(&km, 0, ROWS, 0), KC_NO);
    KB_CHECK_EQ(kb_keymap_get(&km, 0, 0, COLS), KC_NO);
    KB_CHECK_EQ(kb_keymap_get(&km, 255, 255, 255), KC_NO);
}

static void test_set_validates(void)
{
    struct kb_keymap km;
    kb_keymap_init(&km, storage, LAYERS, ROWS, COLS, &def[0][0][0]);
    KB_CHECK_EQ(kb_keymap_set(&km, 2, 1, 2, KC_Z), 0);
    KB_CHECK_EQ(kb_keymap_get(&km, 2, 1, 2), KC_Z);
    KB_CHECK_EQ(kb_keymap_set(&km, 0, 0, 0, MO(2)), 0);
    KB_CHECK_EQ(kb_keymap_set(&km, 0, 0, 0, MO(3)), -1);
    KB_CHECK_EQ(kb_keymap_set(&km, 0, 0, 0, 0xA123), -1);
    KB_CHECK_EQ(kb_keymap_set(&km, 3, 0, 0, KC_A), -1);
    KB_CHECK_EQ(kb_keymap_set(&km, 0, 2, 0, KC_A), -1);
    KB_CHECK_EQ(kb_keymap_set(&km, 0, 0, 3, KC_A), -1);
    KB_CHECK_EQ(kb_keymap_get(&km, 0, 0, 0), MO(2));
}

static void test_resolve_transparent_falls_through(void)
{
    struct kb_keymap km;
    kb_keymap_init(&km, storage, LAYERS, ROWS, COLS, &def[0][0][0]);

    KB_CHECK_EQ(kb_keymap_resolve(&km, 0x1, 0, 0), KC_A);
    KB_CHECK_EQ(kb_keymap_resolve(&km, 0x3, 0, 0), KC_1);
    KB_CHECK_EQ(kb_keymap_resolve(&km, 0x3, 0, 1), KC_B);
    /* Layer 2 over layer 1 over layer 0 */
    KB_CHECK_EQ(kb_keymap_resolve(&km, 0x7, 0, 0), KC_1);
    KB_CHECK_EQ(kb_keymap_resolve(&km, 0x7, 0, 2), KC_F3);
    /* Layer 2 without layer 1 skips it. */
    KB_CHECK_EQ(kb_keymap_resolve(&km, 0x5, 0, 0), KC_A);
    /* KC_NO is opaque. */
    KB_CHECK_EQ(kb_keymap_resolve(&km, 0x7, 1, 2), KC_NO);
}

static void test_resolve_all_transparent_is_no(void)
{
    kb_keycode_t all_trns[2] = {KC_TRNS, KC_TRNS};
    kb_keycode_t store[2];
    struct kb_keymap km;
    kb_keymap_init(&km, store, 2, 1, 1, all_trns);
    KB_CHECK_EQ(kb_keymap_resolve(&km, 0x3, 0, 0), KC_NO);
    KB_CHECK_EQ(kb_keymap_resolve(&km, 0x0, 0, 0), KC_NO);
    KB_CHECK_EQ(kb_keymap_resolve(&km, 0xFFFFFFFFu, 5, 5), KC_NO);
}

int main(void)
{
    printf("test_keymap\n");
    KB_RUN(test_init_copies_default);
    KB_RUN(test_init_rejects_bad_sizes);
    KB_RUN(test_init_replaces_invalid_keycodes);
    KB_RUN(test_get_out_of_range_is_no);
    KB_RUN(test_set_validates);
    KB_RUN(test_resolve_transparent_falls_through);
    KB_RUN(test_resolve_all_transparent_is_no);
    return KB_TEST_RESULT();
}
