/*
 * A display that turns a 1-bit, vertically tiled panel (SSD16xx e-paper) by
 * 90 or 270 degrees, so the character framebuffer can draw on it in portrait.
 * It reports the panel's size with width and height swapped (the height cut
 * to a multiple of 8, as tiles need), takes full frames only, as the
 * character framebuffer writes them, and passes each one to the panel turned
 * back to its own landscape layout.
 */

#define DT_DRV_COMPAT dachshund_display_rotate

#include <errno.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/logging/log.h>

#include "kb/screen.h"

LOG_MODULE_REGISTER(display_rotate, LOG_LEVEL_INF);

struct rotate_config {
    const struct device *panel;
    uint16_t rotation;
    uint8_t *frame;     /* the panel's landscape frame */
    size_t frame_size;
};

static void panel_caps(const struct device *dev, struct display_capabilities *caps)
{
    const struct rotate_config *cfg = dev->config;

    display_get_capabilities(cfg->panel, caps);
}

static void rotate_get_capabilities(const struct device *dev, struct display_capabilities *caps)
{
    panel_caps(dev, caps);

    uint16_t w = caps->x_resolution;

    caps->x_resolution = caps->y_resolution;
    caps->y_resolution = w - w % 8u;
}

static int rotate_write(const struct device *dev, const uint16_t x, const uint16_t y,
                        const struct display_buffer_descriptor *desc, const void *buf)
{
    const struct rotate_config *cfg = dev->config;
    struct display_capabilities panel, ours;

    panel_caps(dev, &panel);
    rotate_get_capabilities(dev, &ours);
    if (!(panel.screen_info & SCREEN_INFO_MONO_VTILED)) {
        return -ENOTSUP;
    }
    if (x != 0 || y != 0 || desc->width != ours.x_resolution ||
        desc->height != ours.y_resolution || desc->pitch != desc->width ||
        desc->buf_size < (size_t)desc->width * desc->height / 8u ||
        (size_t)panel.x_resolution * panel.y_resolution / 8u > cfg->frame_size) {
        return -EINVAL;
    }

    int err = kb_screen_rotate(buf, desc->width, desc->height, cfg->frame, panel.x_resolution,
                               panel.y_resolution, cfg->rotation,
                               (panel.screen_info & SCREEN_INFO_MONO_MSB_FIRST) != 0);
    if (err) {
        return err;
    }

    const struct display_buffer_descriptor out = {
        .buf_size = (size_t)panel.x_resolution * panel.y_resolution / 8u,
        .width = panel.x_resolution,
        .height = panel.y_resolution,
        .pitch = panel.x_resolution,
    };
    return display_write(cfg->panel, 0, 0, &out, cfg->frame);
}

static int rotate_blanking_on(const struct device *dev)
{
    const struct rotate_config *cfg = dev->config;

    return display_blanking_on(cfg->panel);
}

static int rotate_blanking_off(const struct device *dev)
{
    const struct rotate_config *cfg = dev->config;

    return display_blanking_off(cfg->panel);
}

static int rotate_set_pixel_format(const struct device *dev, const enum display_pixel_format pf)
{
    const struct rotate_config *cfg = dev->config;

    return display_set_pixel_format(cfg->panel, pf);
}

static int rotate_init(const struct device *dev)
{
    const struct rotate_config *cfg = dev->config;

    if (!device_is_ready(cfg->panel)) {
        LOG_ERR("panel not ready");
        return -ENODEV;
    }
    return 0;
}

static DEVICE_API(display, rotate_api) = {
    .blanking_on = rotate_blanking_on,
    .blanking_off = rotate_blanking_off,
    .write = rotate_write,
    .get_capabilities = rotate_get_capabilities,
    .set_pixel_format = rotate_set_pixel_format,
};

#define PANEL(n) DT_INST_PHANDLE(n, display)
#define FRAME_SIZE(n) (DT_PROP(PANEL(n), width) * DT_PROP(PANEL(n), height) / 8)

#define ROTATE_DEFINE(n)                                                                \
    static uint8_t rotate_frame_##n[FRAME_SIZE(n)];                                     \
    static const struct rotate_config rotate_config_##n = {                             \
        .panel = DEVICE_DT_GET(PANEL(n)),                                               \
        .rotation = DT_INST_PROP(n, rotation),                                          \
        .frame = rotate_frame_##n,                                                      \
        .frame_size = sizeof(rotate_frame_##n),                                         \
    };                                                                                  \
    DEVICE_DT_INST_DEFINE(n, rotate_init, NULL, NULL, &rotate_config_##n, POST_KERNEL,  \
                          CONFIG_KB_DISPLAY_ROTATE_INIT_PRIORITY, &rotate_api);

DT_INST_FOREACH_STATUS_OKAY(ROTATE_DEFINE)
