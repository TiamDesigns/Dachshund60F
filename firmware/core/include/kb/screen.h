/*
 * Status screen content model, independent of the panel (OLED or e-paper)
 * and of the font.
 *
 * The screen is a grid of character cells. A layout is a list of widgets,
 * each placed at a row and column; rendering turns a layout plus the current
 * keyboard state into text items for the firmware to draw. Rows past the
 * bottom of a small screen are skipped, so one layout works on 128x32 and
 * larger panels.
 *
 * Layouts are meant to be user-editable (later over the config protocol), so
 * kb_screen_layout_validate() checks every field before a layout is used.
 */
#ifndef KB_SCREEN_H
#define KB_SCREEN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define KB_SCREEN_MAX_WIDGETS    8
#define KB_SCREEN_TEXT_MAX       24 /* bytes, including the terminating NUL */
#define KB_SCREEN_MAX_LAYERS     16
#define KB_SCREEN_LAYER_NAME_MAX 9  /* bytes, including the terminating NUL */
#define KB_SCREEN_ITEM_MAX       32 /* longest rendered item, including NUL */

enum kb_screen_widget_type {
    KB_WIDGET_TEXT = 1,   /* fixed text from the widget */
    KB_WIDGET_LAYER,      /* active layer name, or "L<n>" */
    KB_WIDGET_LOCKS,      /* "CAPS NUM SCRL", active locks only */
    KB_WIDGET_BATTERY,    /* "BAT 75%", "CHG 75%" or "BAT --" */
    KB_WIDGET_CONNECTION, /* "USB", "BT1".."BT9" or "2.4G" */
};

enum kb_screen_align {
    KB_ALIGN_LEFT = 0,   /* col counts from the left edge */
    KB_ALIGN_RIGHT = 1,  /* col counts from the right edge */
    KB_ALIGN_CENTER = 2, /* col is ignored */
};

/* Lock bits, as in the HID keyboard LED output report. */
#define KB_LOCK_NUM    0x01u
#define KB_LOCK_CAPS   0x02u
#define KB_LOCK_SCROLL 0x04u

enum kb_transport {
    KB_TRANSPORT_NONE = 0,
    KB_TRANSPORT_USB,
    KB_TRANSPORT_BLE,
    KB_TRANSPORT_DONGLE,
};

/* Which widgets the default layout includes. */
#define KB_SCREEN_SHOW_LAYER      0x01u
#define KB_SCREEN_SHOW_BATTERY    0x02u
#define KB_SCREEN_SHOW_LOCKS      0x04u
#define KB_SCREEN_SHOW_CONNECTION 0x08u
#define KB_SCREEN_SHOW_TEXT       0x10u

struct kb_screen_widget {
    uint8_t type;  /* enum kb_screen_widget_type */
    uint8_t row;
    uint8_t col;
    uint8_t align; /* enum kb_screen_align */
    char text[KB_SCREEN_TEXT_MAX]; /* KB_WIDGET_TEXT only */
};

struct kb_screen_layout {
    uint8_t count;
    struct kb_screen_widget widgets[KB_SCREEN_MAX_WIDGETS];
    /* Empty names render as "L<n>". */
    char layer_names[KB_SCREEN_MAX_LAYERS][KB_SCREEN_LAYER_NAME_MAX];
};

struct kb_screen_state {
    uint8_t layer;
    uint8_t locks;       /* KB_LOCK_* bits */
    int8_t battery_pct;  /* 0-100, negative when unknown */
    bool charging;
    uint8_t transport;   /* enum kb_transport */
    uint8_t ble_profile; /* 0-based, shown as BT1.. */
};

struct kb_screen_item {
    uint8_t row;
    uint8_t col;
    char text[KB_SCREEN_ITEM_MAX];
};

/* State before anything is known: layer 0, no locks, battery unknown. */
void kb_screen_state_init(struct kb_screen_state *state);

/*
 * Build the default layout. Status widgets fill rows two per row (left, then
 * right-aligned) in the order layer, battery, locks, connection; the text
 * goes centred on the row after them. Layers 0 and 1 are named "Base" and
 * "Fn". text may be NULL; longer text is truncated.
 */
void kb_screen_default_layout(struct kb_screen_layout *layout, uint32_t show, const char *text);

/* 0 if every field is in range and every string terminated, else -EINVAL. */
int kb_screen_layout_validate(const struct kb_screen_layout *layout);

/*
 * Render a validated layout for a grid of cols x rows cells. Writes at most
 * max_items items, skipping widgets that are empty, off-screen or past
 * max_items, and truncating text at the right edge. Returns the item count.
 */
size_t kb_screen_render(const struct kb_screen_layout *layout, const struct kb_screen_state *state,
                        uint8_t cols, uint8_t rows, struct kb_screen_item *items, size_t max_items);

/*
 * Rotate a 1-bit, vertically tiled frame (each byte holds 8 pixels of one
 * column, as SSD1306 and SSD16xx take them) by 90 or 270 degrees clockwise.
 * src is src_w x src_h pixels; dst is dst_w x dst_h, with dst_w >= src_h and
 * dst_h >= src_w, and both heights multiples of 8. Pixels of dst that src
 * doesn't cover are cleared. msb_first: bit 7 is a byte's top pixel. Returns
 * -EINVAL for any other rotation or size, without touching dst.
 */
int kb_screen_rotate(const uint8_t *src, uint16_t src_w, uint16_t src_h, uint8_t *dst,
                     uint16_t dst_w, uint16_t dst_h, uint16_t rotation, bool msb_first);

#endif /* KB_SCREEN_H */
