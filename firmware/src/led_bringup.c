#include "led_bringup.h"

#include <errno.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/drivers/regulator.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(led_bringup, LOG_LEVEL_INF);

#define STRIP_NODE  DT_ALIAS(led_strip)
#define STRIP_LEN   DT_PROP(STRIP_NODE, chain_length)
#define STEP_MS     500
/* Low brightness: six LEDs at full white would draw about 360 mA. */
#define LEVEL       24

static const struct device *const strip = DEVICE_DT_GET(STRIP_NODE);
static const struct device *const led_power = DEVICE_DT_GET(DT_NODELABEL(led_power));

static struct led_rgb pixels[STRIP_LEN];
static uint32_t step;

static void led_step(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(led_work, led_step);

/*
 * One LED lit at a time, in chain order, cycling red, green, blue. Index 0 is
 * the layer LED and index STRIP_LEN - 1 is Caps Lock (keyboard.json ids).
 */
static void led_step(struct k_work *work)
{
    ARG_UNUSED(work);

    uint32_t index = step % STRIP_LEN;
    uint32_t colour = (step / STRIP_LEN) % 3;

    memset(pixels, 0, sizeof(pixels));
    pixels[index].r = colour == 0 ? LEVEL : 0;
    pixels[index].g = colour == 1 ? LEVEL : 0;
    pixels[index].b = colour == 2 ? LEVEL : 0;

    int err = led_strip_update_rgb(strip, pixels, STRIP_LEN);
    if (err) {
        LOG_ERR("LED strip update failed (%d)", err);
    }

    step++;
    k_work_schedule(&led_work, K_MSEC(STEP_MS));
}

int led_bringup_start(void)
{
    if (!device_is_ready(led_power) || !device_is_ready(strip)) {
        LOG_ERR("LED power or strip device not ready");
        return -ENODEV;
    }

    int err = regulator_enable(led_power);
    if (err) {
        LOG_ERR("LED power enable failed (%d)", err);
        return err;
    }

    k_work_schedule(&led_work, K_NO_WAIT);
    return 0;
}
