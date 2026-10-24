#include "led.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

LOG_MODULE_REGISTER(led, LOG_LEVEL_INF);

/* Pins from the board (nRF52840 Dongle, matching board.json's dongle section). */
static const struct gpio_dt_spec red = GPIO_DT_SPEC_GET(DT_ALIAS(led1_red), gpios);
static const struct gpio_dt_spec green = GPIO_DT_SPEC_GET(DT_ALIAS(led1_green), gpios);
static const struct gpio_dt_spec blue = GPIO_DT_SPEC_GET(DT_ALIAS(led1_blue), gpios);

#define R 1u
#define G 2u
#define B 4u

#define BLINK_MS 250
#define SHOW_MS 1000

static atomic_t pattern = ATOMIC_INIT(LED_OFF);
static atomic_t asleep;
static atomic_t step;

static void set(unsigned rgb)
{
    if (atomic_get(&asleep)) {
        rgb = 0;
    }
    (void)gpio_pin_set_dt(&red, (rgb & R) != 0);
    (void)gpio_pin_set_dt(&green, (rgb & G) != 0);
    (void)gpio_pin_set_dt(&blue, (rgb & B) != 0);
}

static void led_fn(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(led_work, led_fn);

static void led_fn(struct k_work *work)
{
    ARG_UNUSED(work);
    atomic_val_t n = atomic_inc(&step);

    switch ((enum led_pattern)atomic_get(&pattern)) {
    case LED_PAIRING:
        set(n % 2 == 0 ? B : 0);
        k_work_schedule(&led_work, K_MSEC(BLINK_MS));
        return;
    case LED_PAIRED:
        if (n < 6) {
            set(n % 2 == 0 ? G : 0);
            k_work_schedule(&led_work, K_MSEC(BLINK_MS));
            return;
        }
        break;
    case LED_LINK_UP:
    case LED_LINK_DOWN:
    case LED_HELLO:
        if (n == 0) {
            enum led_pattern p = (enum led_pattern)atomic_get(&pattern);
            set(p == LED_LINK_UP ? G : p == LED_LINK_DOWN ? R : R | G | B);
            k_work_schedule(&led_work, K_MSEC(p == LED_HELLO ? SHOW_MS / 2 : SHOW_MS));
            return;
        }
        break;
    default:
        break;
    }
    atomic_set(&pattern, LED_OFF);
    set(0);
}

void led_show(enum led_pattern p)
{
    /* Pairing outranks the one-off patterns until it ends. */
    if (atomic_get(&pattern) == LED_PAIRING && p != LED_OFF && p != LED_PAIRED) {
        return;
    }
    atomic_set(&pattern, p);
    atomic_set(&step, 0);
    k_work_reschedule(&led_work, K_NO_WAIT);
}

void led_set_asleep(bool on)
{
    atomic_set(&asleep, on ? 1 : 0);
    if (on) {
        set(0);
    }
}

int led_init(void)
{
    const struct gpio_dt_spec *leds[] = {&red, &green, &blue};
    for (size_t i = 0; i < ARRAY_SIZE(leds); i++) {
        if (!gpio_is_ready_dt(leds[i]) || gpio_pin_configure_dt(leds[i], GPIO_OUTPUT_INACTIVE)) {
            LOG_ERR("status LED unavailable");
            return -ENODEV;
        }
    }
    led_show(LED_HELLO);
    return 0;
}
