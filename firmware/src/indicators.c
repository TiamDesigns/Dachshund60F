/*
 * Indicator LEDs: renders kb/indicators.h onto the SK6812 chain.
 *
 * At power-up the chain walks once, one LED at a time in chain order, red
 * then green then blue (CONFIG_KB_LED_SELFTEST), which shows both the chain
 * order and the colour order at bring-up. Then the LEDs follow the host's
 * lock state, the active layer and USB suspend.
 *
 * LED power (the TPS22917 load switch) is on only while an LED is lit:
 * six idle SK6812s draw about 6 mA.
 */

#include "indicators.h"

#include <errno.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/drivers/regulator.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "kb/indicators.h"

LOG_MODULE_REGISTER(indicators, LOG_LEVEL_INF);

#define STRIP_NODE DT_ALIAS(led_strip)
#define STRIP_LEN DT_PROP(STRIP_NODE, chain_length)

/*
 * The chain, in data order. Must match the indicators in
 * boards/Dachshund60F/keyboard.json (checked by host-lib's board tests).
 */
static const struct kb_led_def chain[] = {
    {KB_LED_ROLE_LAYER, 0},
    {KB_LED_ROLE_BATTERY, 1},
    {KB_LED_ROLE_BATTERY, 2},
    {KB_LED_ROLE_BATTERY, 3},
    {KB_LED_ROLE_BATTERY, 4},
    {KB_LED_ROLE_CAPS, 0},
};
BUILD_ASSERT(ARRAY_SIZE(chain) == STRIP_LEN, "indicator table must cover the LED chain");

#define SELFTEST_STEP_MS 200
#define SELFTEST_LEVEL 24

static const struct device *const strip = DEVICE_DT_GET(STRIP_NODE);
static const struct device *const led_power = DEVICE_DT_GET(DT_NODELABEL(led_power));

static struct k_spinlock lock;
static struct kb_indicator_state state = {.battery = KB_BATTERY_UNKNOWN};
static struct kb_indicator_config config;
static bool config_ready;

/* Defaults on first use: the keyboard sets the config before indicators_start. Hold lock. */
static void config_init_locked(void)
{
    if (!config_ready) {
        kb_indicators_default_config(&config);
        config_ready = true;
    }
}
static bool powered;
static bool started;
static uint32_t selftest_step;

static struct led_rgb pixels[STRIP_LEN];

/* Identify: every LED blinks until identify_end (uptime, ms). */
#define IDENTIFY_BLINK_MS 250
static int64_t identify_end;
static bool identify_on;

static void redraw(struct k_work *work);
static void selftest_step_fn(struct k_work *work);
static K_WORK_DEFINE(redraw_work, redraw);
static K_WORK_DELAYABLE_DEFINE(selftest_work, selftest_step_fn);
static void identify_fn(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(identify_work, identify_fn);

static void set_power(bool on)
{
    if (on == powered) {
        return;
    }
    int err = on ? regulator_enable(led_power) : regulator_disable(led_power);
    if (err) {
        LOG_ERR("LED power %s failed (%d)", on ? "on" : "off", err);
        return;
    }
    /* The devicetree's startup-delay-us lets the LEDs power up before data. */
    powered = on;
}

static void show(bool lit)
{
    if (lit) {
        set_power(true);
    }
    if (powered) {
        int err = led_strip_update_rgb(strip, pixels, STRIP_LEN);
        if (err) {
            LOG_ERR("LED strip update failed (%d)", err);
        }
    }
    if (!lit) {
        set_power(false);
    }
}

static void redraw(struct k_work *work)
{
    ARG_UNUSED(work);

    struct kb_indicator_state now;
    struct kb_rgb out[STRIP_LEN];

    if (!started) {
        return; /* the self-test still owns the strip */
    }
    struct kb_indicator_config cfg;
    K_SPINLOCK(&lock) {
        config_init_locked();
        now = state;
        cfg = config;
    }

    bool lit = kb_indicators_render(chain, STRIP_LEN, &cfg, &now, out);
    for (size_t i = 0; i < STRIP_LEN; i++) {
        pixels[i].r = out[i].r;
        pixels[i].g = out[i].g;
        pixels[i].b = out[i].b;
    }
    if (identify_end != 0) {
        /* Identify overrides everything: all LEDs blink a warm white. */
        for (size_t i = 0; i < STRIP_LEN; i++) {
            pixels[i] = (struct led_rgb){.r = identify_on ? SELFTEST_LEVEL : 0,
                                         .g = identify_on ? SELFTEST_LEVEL * 3 / 4 : 0,
                                         .b = identify_on ? SELFTEST_LEVEL / 2 : 0};
        }
        lit = identify_on;
    }
    show(lit);
}

static void selftest_step_fn(struct k_work *work)
{
    ARG_UNUSED(work);

    uint32_t index = selftest_step / 3u;
    uint32_t colour = selftest_step % 3u;

    if (index >= STRIP_LEN) {
        started = true;
        redraw(NULL);
        return;
    }

    memset(pixels, 0, sizeof(pixels));
    pixels[index].r = colour == 0 ? SELFTEST_LEVEL : 0;
    pixels[index].g = colour == 1 ? SELFTEST_LEVEL : 0;
    pixels[index].b = colour == 2 ? SELFTEST_LEVEL : 0;
    show(true);

    selftest_step++;
    k_work_schedule(&selftest_work, K_MSEC(SELFTEST_STEP_MS));
}

/* Runs on the system work queue, like redraw. */
static void identify_fn(struct k_work *work)
{
    ARG_UNUSED(work);

    if (identify_end != 0 && k_uptime_get() >= identify_end) {
        identify_end = 0;
        identify_on = false;
    } else {
        identify_on = !identify_on;
        k_work_schedule(&identify_work, K_MSEC(IDENTIFY_BLINK_MS));
    }
    redraw(NULL);
}

void indicators_identify(uint8_t seconds)
{
    /* Written here, read on the work queue; a stale read only shifts the end by a blink. */
    identify_end = k_uptime_get() + (int64_t)seconds * 1000;
    k_work_reschedule(&identify_work, K_NO_WAIT);
}

static void changed(void)
{
    k_work_submit(&redraw_work);
}

void indicators_set_locks(uint8_t locks)
{
    K_SPINLOCK(&lock) {
        state.locks = locks;
    }
    changed();
}

void indicators_set_layer(uint8_t layer)
{
    K_SPINLOCK(&lock) {
        state.layer = layer;
    }
    changed();
}

void indicators_set_config(const struct kb_indicator_config *cfg, uint8_t brightness_percent)
{
    K_SPINLOCK(&lock) {
        config = *cfg;
        config.brightness = (uint8_t)(MIN(brightness_percent, 100u) * 255u / 100u);
        config_ready = true;
    }
    changed();
}

void indicators_set_asleep(bool asleep)
{
    K_SPINLOCK(&lock) {
        state.asleep = asleep;
    }
    changed();
}

int indicators_start(void)
{
    if (!device_is_ready(led_power) || !device_is_ready(strip)) {
        LOG_ERR("LED power or strip device not ready");
        return -ENODEV;
    }


    if (IS_ENABLED(CONFIG_KB_LED_SELFTEST)) {
        k_work_schedule(&selftest_work, K_NO_WAIT);
    } else {
        started = true;
        changed();
    }
    return 0;
}
