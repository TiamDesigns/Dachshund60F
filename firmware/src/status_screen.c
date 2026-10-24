/*
 * Status screen: renders the kb/screen.h layout with Zephyr's character
 * framebuffer (CFB), which drives both SSD1306/SH1106 OLEDs and SSD16xx
 * e-paper panels.
 *
 * Updates are coalesced on the system work queue. OLEDs redraw shortly after
 * a change; e-paper redraws at most every CONFIG_KB_SCREEN_EPD_MIN_INTERVAL_MS
 * because each refresh is slow and wears the panel.
 */

#include "status_screen.h"

#include <string.h>

#include <zephyr/device.h>
#include <zephyr/display/cfb.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(status_screen, LOG_LEVEL_INF);

#define OLED_UPDATE_DELAY_MS 20

#define SHOW_FLAGS                                                                      \
    ((IS_ENABLED(CONFIG_KB_SCREEN_SHOW_LAYER) ? KB_SCREEN_SHOW_LAYER : 0) |              \
     (IS_ENABLED(CONFIG_KB_SCREEN_SHOW_BATTERY) ? KB_SCREEN_SHOW_BATTERY : 0) |          \
     (IS_ENABLED(CONFIG_KB_SCREEN_SHOW_LOCKS) ? KB_SCREEN_SHOW_LOCKS : 0) |              \
     (IS_ENABLED(CONFIG_KB_SCREEN_SHOW_CONNECTION) ? KB_SCREEN_SHOW_CONNECTION : 0) |    \
     (IS_ENABLED(CONFIG_KB_SCREEN_SHOW_TEXT) ? KB_SCREEN_SHOW_TEXT : 0))

static const struct device *const display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

static K_MUTEX_DEFINE(screen_lock);
static struct kb_screen_layout layout;
static struct kb_screen_state state;
static struct kb_screen_item items[KB_SCREEN_MAX_WIDGETS];

static bool ready;
static bool is_epd;
static uint8_t font_w;
static uint8_t font_h;
static uint8_t cols;
static uint8_t rows;
static int64_t last_draw_ms;

static void draw(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(draw_work, draw);

static void draw(struct k_work *work)
{
    ARG_UNUSED(work);

    k_mutex_lock(&screen_lock, K_FOREVER);
    size_t n = kb_screen_render(&layout, &state, cols, rows, items, ARRAY_SIZE(items));
    k_mutex_unlock(&screen_lock);

    cfb_framebuffer_clear(display, false);
    for (size_t i = 0; i < n; i++) {
        cfb_print(display, items[i].text, items[i].col * font_w, items[i].row * font_h);
    }

    int err = cfb_framebuffer_finalize(display);
    if (err) {
        LOG_ERR("display update failed (%d)", err);
    }

    last_draw_ms = k_uptime_get();
}

static void request_draw(void)
{
    if (!ready) {
        return;
    }

    k_timeout_t delay = K_MSEC(OLED_UPDATE_DELAY_MS);

    if (is_epd) {
        int64_t next = last_draw_ms + CONFIG_KB_SCREEN_EPD_MIN_INTERVAL_MS;
        int64_t now = k_uptime_get();

        delay = next > now ? K_MSEC(next - now) : K_NO_WAIT;
    }

    /* Does nothing if a draw is already pending, so bursts coalesce. */
    k_work_schedule(&draw_work, delay);
}

#define UPDATE_STATE(assignments)                                                       \
    do {                                                                                \
        k_mutex_lock(&screen_lock, K_FOREVER);                                          \
        struct kb_screen_state before = state;                                          \
        assignments;                                                                    \
        bool changed = memcmp(&before, &state, sizeof(state)) != 0;                     \
        k_mutex_unlock(&screen_lock);                                                   \
        if (changed) {                                                                  \
            request_draw();                                                             \
        }                                                                               \
    } while (0)

void status_screen_set_layer(uint8_t layer)
{
    UPDATE_STATE(state.layer = layer);
}

void status_screen_set_locks(uint8_t locks)
{
    UPDATE_STATE(state.locks = locks & (KB_LOCK_NUM | KB_LOCK_CAPS | KB_LOCK_SCROLL));
}

void status_screen_set_battery(int8_t pct, bool charging)
{
    UPDATE_STATE({
        state.battery_pct = pct;
        state.charging = charging;
    });
}

void status_screen_set_transport(uint8_t transport, uint8_t ble_profile)
{
    UPDATE_STATE({
        state.transport = transport;
        state.ble_profile = ble_profile;
    });
}

int status_screen_set_layout(const struct kb_screen_layout *new_layout)
{
    if (kb_screen_layout_validate(new_layout) != 0) {
        return -EINVAL;
    }

    k_mutex_lock(&screen_lock, K_FOREVER);
    layout = *new_layout;
    k_mutex_unlock(&screen_lock);

    request_draw();
    return 0;
}

static void blank(bool on)
{
    /* E-paper keeps its picture without power; only an OLED goes dark. */
    if (is_epd) {
        return;
    }
    int err = on ? display_blanking_on(display) : display_blanking_off(display);
    if (err && err != -ENOSYS) {
        LOG_WRN("blanking %s failed (%d)", on ? "on" : "off", err);
    }
}

void status_screen_set_dark(bool dark)
{
    static bool is_dark;

    if (!ready || dark == is_dark) {
        return;
    }
    is_dark = dark;
    blank(dark);
}

void status_screen_off(void)
{
    struct k_work_sync sync;

    if (!ready) {
        return;
    }
    ready = false;
    (void)k_work_cancel_delayable_sync(&draw_work, &sync);
    blank(true);
}

/* Index of the configured font, or of the shortest one if none is set. */
static int pick_font(void)
{
    int count = cfb_get_numof_fonts(display);
    int best = -1;
    uint8_t best_h = UINT8_MAX;

    for (int i = 0; i < count; i++) {
        uint8_t w, h;

        if (cfb_get_font_size(display, i, &w, &h) != 0) {
            continue;
        }
        if (i == CONFIG_KB_SCREEN_FONT_INDEX) {
            return i;
        }
        if (h < best_h) {
            best = i;
            best_h = h;
        }
    }

    return best;
}

int status_screen_init(void)
{
    struct display_capabilities caps;
    int err;

    if (!device_is_ready(display)) {
        LOG_ERR("display not ready (is the panel connected?)");
        return -ENODEV;
    }

    display_get_capabilities(display, &caps);
    is_epd = (caps.screen_info & SCREEN_INFO_EPD) != 0;

    err = cfb_framebuffer_init(display);
    if (err) {
        LOG_ERR("framebuffer init failed (%d)", err);
        return err;
    }

    int font = pick_font();
    if (font < 0 || cfb_framebuffer_set_font(display, font) != 0 ||
        cfb_get_font_size(display, font, &font_w, &font_h) != 0 || font_w == 0 || font_h == 0) {
        LOG_ERR("no usable font");
        return -ENOENT;
    }
    cfb_set_kerning(display, 0);

    int width = cfb_get_display_parameter(display, CFB_DISPLAY_WIDTH);
    int height = cfb_get_display_parameter(display, CFB_DISPLAY_HEIGHT);
    cols = (uint8_t)MIN(width / font_w, UINT8_MAX);
    rows = (uint8_t)MIN(height / font_h, UINT8_MAX);

    if (IS_ENABLED(CONFIG_KB_SCREEN_INVERT)) {
        cfb_framebuffer_invert(display);
    }

    kb_screen_state_init(&state);
    kb_screen_default_layout(&layout, SHOW_FLAGS, CONFIG_KB_SCREEN_TEXT);

    err = display_blanking_off(display);
    if (err && err != -ENOSYS) {
        LOG_WRN("blanking off failed (%d)", err);
    }

    LOG_INF("%s %dx%d, font %ux%u, %ux%u cells", is_epd ? "e-paper" : "OLED", width, height,
            font_w, font_h, cols, rows);

    ready = true;
    k_work_schedule(&draw_work, K_NO_WAIT);
    return 0;
}
