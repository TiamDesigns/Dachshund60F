#include "kb/screen.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

static bool is_terminated(const char *s, size_t size)
{
    return memchr(s, '\0', size) != NULL;
}

static void copy_text(char *dst, size_t size, const char *src)
{
    size_t len = src ? strlen(src) : 0;

    if (len >= size) {
        len = size - 1;
    }
    memcpy(dst, src ? src : "", len);
    dst[len] = '\0';
}

void kb_screen_state_init(struct kb_screen_state *state)
{
    memset(state, 0, sizeof(*state));
    state->battery_pct = -1;
    state->transport = KB_TRANSPORT_NONE;
}

void kb_screen_default_layout(struct kb_screen_layout *layout, uint32_t show, const char *text)
{
    static const struct {
        uint32_t flag;
        uint8_t type;
    } status_widgets[] = {
        {KB_SCREEN_SHOW_LAYER, KB_WIDGET_LAYER},
        {KB_SCREEN_SHOW_BATTERY, KB_WIDGET_BATTERY},
        {KB_SCREEN_SHOW_LOCKS, KB_WIDGET_LOCKS},
        {KB_SCREEN_SHOW_CONNECTION, KB_WIDGET_CONNECTION},
    };

    memset(layout, 0, sizeof(*layout));
    copy_text(layout->layer_names[0], KB_SCREEN_LAYER_NAME_MAX, "Base");
    copy_text(layout->layer_names[1], KB_SCREEN_LAYER_NAME_MAX, "Fn");

    uint8_t placed = 0;
    for (size_t i = 0; i < sizeof(status_widgets) / sizeof(status_widgets[0]); i++) {
        if (!(show & status_widgets[i].flag)) {
            continue;
        }
        struct kb_screen_widget *w = &layout->widgets[layout->count++];
        w->type = status_widgets[i].type;
        w->row = placed / 2;
        w->align = (placed % 2) ? KB_ALIGN_RIGHT : KB_ALIGN_LEFT;
        placed++;
    }

    if ((show & KB_SCREEN_SHOW_TEXT) && text && text[0] != '\0') {
        struct kb_screen_widget *w = &layout->widgets[layout->count++];
        w->type = KB_WIDGET_TEXT;
        w->row = (placed + 1) / 2;
        w->align = KB_ALIGN_CENTER;
        copy_text(w->text, KB_SCREEN_TEXT_MAX, text);
    }
}

int kb_screen_layout_validate(const struct kb_screen_layout *layout)
{
    if (layout->count > KB_SCREEN_MAX_WIDGETS) {
        return -EINVAL;
    }

    for (size_t i = 0; i < layout->count; i++) {
        const struct kb_screen_widget *w = &layout->widgets[i];

        if (w->type < KB_WIDGET_TEXT || w->type > KB_WIDGET_CONNECTION) {
            return -EINVAL;
        }
        if (w->align > KB_ALIGN_CENTER) {
            return -EINVAL;
        }
        if (!is_terminated(w->text, sizeof(w->text))) {
            return -EINVAL;
        }
    }

    for (size_t i = 0; i < KB_SCREEN_MAX_LAYERS; i++) {
        if (!is_terminated(layout->layer_names[i], sizeof(layout->layer_names[i]))) {
            return -EINVAL;
        }
    }

    return 0;
}

static void widget_text(const struct kb_screen_layout *layout, const struct kb_screen_widget *w,
                        const struct kb_screen_state *state, char *out, size_t size)
{
    out[0] = '\0';

    switch (w->type) {
    case KB_WIDGET_TEXT:
        copy_text(out, size, w->text);
        break;

    case KB_WIDGET_LAYER:
        if (state->layer < KB_SCREEN_MAX_LAYERS && layout->layer_names[state->layer][0] != '\0') {
            copy_text(out, size, layout->layer_names[state->layer]);
        } else {
            snprintf(out, size, "L%u", (unsigned)state->layer);
        }
        break;

    case KB_WIDGET_LOCKS: {
        static const struct {
            uint8_t bit;
            const char *name;
        } locks[] = {
            {KB_LOCK_CAPS, "CAPS"},
            {KB_LOCK_NUM, "NUM"},
            {KB_LOCK_SCROLL, "SCRL"},
        };
        for (size_t i = 0; i < sizeof(locks) / sizeof(locks[0]); i++) {
            if (state->locks & locks[i].bit) {
                size_t len = strlen(out);
                snprintf(out + len, size - len, "%s%s", len ? " " : "", locks[i].name);
            }
        }
        break;
    }

    case KB_WIDGET_BATTERY:
        if (state->battery_pct < 0) {
            copy_text(out, size, "BAT --");
        } else {
            unsigned pct = state->battery_pct > 100 ? 100u : (unsigned)state->battery_pct;
            snprintf(out, size, "%s %u%%", state->charging ? "CHG" : "BAT", pct);
        }
        break;

    case KB_WIDGET_CONNECTION:
        switch (state->transport) {
        case KB_TRANSPORT_USB:
            copy_text(out, size, "USB");
            break;
        case KB_TRANSPORT_BLE:
            snprintf(out, size, "BT%u", (unsigned)state->ble_profile + 1u);
            break;
        case KB_TRANSPORT_DONGLE:
            copy_text(out, size, "2.4G");
            break;
        default:
            break;
        }
        break;

    default:
        break;
    }
}

size_t kb_screen_render(const struct kb_screen_layout *layout, const struct kb_screen_state *state,
                        uint8_t cols, uint8_t rows, struct kb_screen_item *items, size_t max_items)
{
    size_t n = 0;

    for (size_t i = 0; i < layout->count && i < KB_SCREEN_MAX_WIDGETS && n < max_items; i++) {
        const struct kb_screen_widget *w = &layout->widgets[i];
        char text[KB_SCREEN_ITEM_MAX];

        if (w->row >= rows) {
            continue;
        }

        widget_text(layout, w, state, text, sizeof(text));
        size_t len = strlen(text);
        if (len == 0) {
            continue;
        }

        if (len > cols) {
            len = cols;
        }

        size_t col;
        switch (w->align) {
        case KB_ALIGN_RIGHT:
            col = (size_t)w->col + len <= cols ? cols - w->col - len : cols;
            break;
        case KB_ALIGN_CENTER:
            col = (cols - len) / 2;
            break;
        default:
            col = w->col;
            break;
        }

        if (col >= cols) {
            continue;
        }
        if (col + len > cols) {
            len = cols - col;
        }

        items[n].row = w->row;
        items[n].col = (uint8_t)col;
        memcpy(items[n].text, text, len);
        items[n].text[len] = '\0';
        n++;
    }

    return n;
}
