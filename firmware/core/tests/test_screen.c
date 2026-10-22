#include "kb/screen.h"
#include "kb_test.h"

#include <errno.h>
#include <string.h>

#define ALL_WIDGETS                                                                     \
    (KB_SCREEN_SHOW_LAYER | KB_SCREEN_SHOW_BATTERY | KB_SCREEN_SHOW_LOCKS |              \
     KB_SCREEN_SHOW_CONNECTION | KB_SCREEN_SHOW_TEXT)

/* Find the item at (row, col); NULL if none. */
static const struct kb_screen_item *item_at(const struct kb_screen_item *items, size_t n,
                                            uint8_t row, uint8_t col)
{
    for (size_t i = 0; i < n; i++) {
        if (items[i].row == row && items[i].col == col) {
            return &items[i];
        }
    }
    return NULL;
}

static int item_is(const struct kb_screen_item *items, size_t n, uint8_t row, uint8_t col,
                   const char *text)
{
    const struct kb_screen_item *it = item_at(items, n, row, col);
    if (!it) {
        fprintf(stderr, "  no item at (%u,%u), expected \"%s\"\n", row, col, text);
        return 0;
    }
    if (strcmp(it->text, text) != 0) {
        fprintf(stderr, "  item at (%u,%u) is \"%s\", expected \"%s\"\n", row, col, it->text,
                text);
        return 0;
    }
    return 1;
}

static void test_default_layout_is_valid(void)
{
    struct kb_screen_layout layout;

    kb_screen_default_layout(&layout, ALL_WIDGETS, "Dachshund60F");
    KB_CHECK_EQ(kb_screen_layout_validate(&layout), 0);
    KB_CHECK_EQ(layout.count, 5);

    kb_screen_default_layout(&layout, 0, NULL);
    KB_CHECK_EQ(kb_screen_layout_validate(&layout), 0);
    KB_CHECK_EQ(layout.count, 0);
}

static void test_default_layout_on_two_rows(void)
{
    /* 128x32 OLED with the 10x16 font: 12 x 2 cells. */
    struct kb_screen_layout layout;
    struct kb_screen_state state;
    struct kb_screen_item items[KB_SCREEN_MAX_WIDGETS];

    kb_screen_default_layout(&layout, ALL_WIDGETS, "Dachshund60F");
    kb_screen_state_init(&state);
    state.locks = KB_LOCK_CAPS;
    state.transport = KB_TRANSPORT_USB;
    state.battery_pct = 75;

    size_t n = kb_screen_render(&layout, &state, 12, 2, items, KB_SCREEN_MAX_WIDGETS);

    KB_CHECK_EQ(n, 4); /* the text row does not fit */
    KB_CHECK(item_is(items, n, 0, 0, "Base"));
    KB_CHECK(item_is(items, n, 0, 5, "BAT 75%"));
    KB_CHECK(item_is(items, n, 1, 0, "CAPS"));
    KB_CHECK(item_is(items, n, 1, 9, "USB"));
}

static void test_default_layout_text_row_on_taller_screen(void)
{
    struct kb_screen_layout layout;
    struct kb_screen_state state;
    struct kb_screen_item items[KB_SCREEN_MAX_WIDGETS];

    kb_screen_default_layout(&layout, ALL_WIDGETS, "Dachshund60F");
    kb_screen_state_init(&state);

    size_t n = kb_screen_render(&layout, &state, 25, 7, items, KB_SCREEN_MAX_WIDGETS);

    /* No locks and no connection yet: those widgets render nothing. */
    KB_CHECK_EQ(n, 3);
    KB_CHECK(item_is(items, n, 0, 0, "Base"));
    KB_CHECK(item_is(items, n, 0, 19, "BAT --"));
    KB_CHECK(item_is(items, n, 2, 6, "Dachshund60F"));
}

static void test_selected_widgets_pack_rows(void)
{
    struct kb_screen_layout layout;
    struct kb_screen_state state;
    struct kb_screen_item items[KB_SCREEN_MAX_WIDGETS];

    kb_screen_default_layout(&layout, KB_SCREEN_SHOW_LOCKS | KB_SCREEN_SHOW_TEXT, "Hi");
    kb_screen_state_init(&state);
    state.locks = KB_LOCK_NUM;

    size_t n = kb_screen_render(&layout, &state, 12, 2, items, KB_SCREEN_MAX_WIDGETS);

    KB_CHECK_EQ(n, 2);
    KB_CHECK(item_is(items, n, 0, 0, "NUM"));
    KB_CHECK(item_is(items, n, 1, 5, "Hi"));
}

static void test_widget_texts(void)
{
    struct kb_screen_layout layout;
    struct kb_screen_state state;
    struct kb_screen_item items[KB_SCREEN_MAX_WIDGETS];
    size_t n;

    memset(&layout, 0, sizeof(layout));
    layout.count = 4;
    layout.widgets[0] = (struct kb_screen_widget){.type = KB_WIDGET_LAYER, .row = 0};
    layout.widgets[1] = (struct kb_screen_widget){.type = KB_WIDGET_LOCKS, .row = 1};
    layout.widgets[2] = (struct kb_screen_widget){.type = KB_WIDGET_BATTERY, .row = 2};
    layout.widgets[3] = (struct kb_screen_widget){.type = KB_WIDGET_CONNECTION, .row = 3};
    strcpy(layout.layer_names[2], "Nav");

    kb_screen_state_init(&state);
    state.layer = 2;
    state.locks = KB_LOCK_CAPS | KB_LOCK_NUM | KB_LOCK_SCROLL;
    state.battery_pct = 40;
    state.charging = true;
    state.transport = KB_TRANSPORT_BLE;
    state.ble_profile = 2;
    n = kb_screen_render(&layout, &state, 20, 4, items, KB_SCREEN_MAX_WIDGETS);
    KB_CHECK(item_is(items, n, 0, 0, "Nav"));
    KB_CHECK(item_is(items, n, 1, 0, "CAPS NUM SCRL"));
    KB_CHECK(item_is(items, n, 2, 0, "CHG 40%"));
    KB_CHECK(item_is(items, n, 3, 0, "BT3"));

    state.layer = 5; /* unnamed */
    state.battery_pct = 120;
    state.charging = false;
    state.transport = KB_TRANSPORT_DONGLE;
    n = kb_screen_render(&layout, &state, 20, 4, items, KB_SCREEN_MAX_WIDGETS);
    KB_CHECK(item_is(items, n, 0, 0, "L5"));
    KB_CHECK(item_is(items, n, 2, 0, "BAT 100%"));
    KB_CHECK(item_is(items, n, 3, 0, "2.4G"));

    state.layer = 200; /* out of the name table */
    n = kb_screen_render(&layout, &state, 20, 4, items, KB_SCREEN_MAX_WIDGETS);
    KB_CHECK(item_is(items, n, 0, 0, "L200"));
}

static void test_clipping_and_bounds(void)
{
    struct kb_screen_layout layout;
    struct kb_screen_state state;
    struct kb_screen_item items[KB_SCREEN_MAX_WIDGETS];
    size_t n;

    memset(&layout, 0, sizeof(layout));
    kb_screen_state_init(&state);
    layout.count = 4;
    layout.widgets[0] = (struct kb_screen_widget){.type = KB_WIDGET_TEXT, .row = 0, .col = 3};
    strcpy(layout.widgets[0].text, "ABCDEFGHIJ");
    layout.widgets[1] = (struct kb_screen_widget){.type = KB_WIDGET_TEXT, .row = 1, .col = 8};
    strcpy(layout.widgets[1].text, "off");
    layout.widgets[2] = (struct kb_screen_widget){
        .type = KB_WIDGET_TEXT, .row = 1, .col = 7, .align = KB_ALIGN_RIGHT};
    strcpy(layout.widgets[2].text, "R");
    layout.widgets[3] = (struct kb_screen_widget){.type = KB_WIDGET_TEXT, .row = 9};
    strcpy(layout.widgets[3].text, "below");

    n = kb_screen_render(&layout, &state, 8, 2, items, KB_SCREEN_MAX_WIDGETS);
    KB_CHECK_EQ(n, 2);
    KB_CHECK(item_is(items, n, 0, 3, "ABCDE")); /* truncated at the right edge */
    KB_CHECK(item_is(items, n, 1, 0, "R"));     /* right-aligned, 7 cells from the edge */

    /* Right margin wider than the screen: skipped, not wrapped. */
    layout.widgets[2].col = 8;
    n = kb_screen_render(&layout, &state, 8, 2, items, KB_SCREEN_MAX_WIDGETS);
    KB_CHECK_EQ(n, 1);

    /* Output capacity is respected. */
    n = kb_screen_render(&layout, &state, 8, 2, items, 0);
    KB_CHECK_EQ(n, 0);

    /* Zero-size grid renders nothing. */
    n = kb_screen_render(&layout, &state, 0, 0, items, KB_SCREEN_MAX_WIDGETS);
    KB_CHECK_EQ(n, 0);
}

static void test_validate_rejects_bad_layouts(void)
{
    struct kb_screen_layout layout;

    kb_screen_default_layout(&layout, ALL_WIDGETS, "x");
    layout.count = KB_SCREEN_MAX_WIDGETS + 1;
    KB_CHECK_EQ(kb_screen_layout_validate(&layout), -EINVAL);

    kb_screen_default_layout(&layout, ALL_WIDGETS, "x");
    layout.widgets[0].type = 0;
    KB_CHECK_EQ(kb_screen_layout_validate(&layout), -EINVAL);

    kb_screen_default_layout(&layout, ALL_WIDGETS, "x");
    layout.widgets[0].type = KB_WIDGET_CONNECTION + 1;
    KB_CHECK_EQ(kb_screen_layout_validate(&layout), -EINVAL);

    kb_screen_default_layout(&layout, ALL_WIDGETS, "x");
    layout.widgets[1].align = KB_ALIGN_CENTER + 1;
    KB_CHECK_EQ(kb_screen_layout_validate(&layout), -EINVAL);

    kb_screen_default_layout(&layout, ALL_WIDGETS, "x");
    memset(layout.widgets[4].text, 'A', sizeof(layout.widgets[4].text));
    KB_CHECK_EQ(kb_screen_layout_validate(&layout), -EINVAL);

    kb_screen_default_layout(&layout, ALL_WIDGETS, "x");
    memset(layout.layer_names[15], 'A', sizeof(layout.layer_names[15]));
    KB_CHECK_EQ(kb_screen_layout_validate(&layout), -EINVAL);

    /* Widgets past count are not checked. */
    kb_screen_default_layout(&layout, ALL_WIDGETS, "x");
    layout.widgets[7].type = 99;
    KB_CHECK_EQ(kb_screen_layout_validate(&layout), 0);
}

static void test_long_text_is_truncated_in_layout(void)
{
    struct kb_screen_layout layout;
    char long_text[64];

    memset(long_text, 'Z', sizeof(long_text) - 1);
    long_text[sizeof(long_text) - 1] = '\0';
    kb_screen_default_layout(&layout, KB_SCREEN_SHOW_TEXT, long_text);
    KB_CHECK_EQ(kb_screen_layout_validate(&layout), 0);
    KB_CHECK_EQ(strlen(layout.widgets[0].text), KB_SCREEN_TEXT_MAX - 1);
}

int main(void)
{
    printf("test_screen\n");
    KB_RUN(test_default_layout_is_valid);
    KB_RUN(test_default_layout_on_two_rows);
    KB_RUN(test_default_layout_text_row_on_taller_screen);
    KB_RUN(test_selected_widgets_pack_rows);
    KB_RUN(test_widget_texts);
    KB_RUN(test_clipping_and_bounds);
    KB_RUN(test_validate_rejects_bad_layouts);
    KB_RUN(test_long_text_is_truncated_in_layout);
    return KB_TEST_RESULT();
}
