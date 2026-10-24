/*
 * 2.4 GHz dongle firmware: a USB receiver for the Dachshund60F.
 *
 * To the computer it is the keyboard (the same HID interfaces, from
 * firmware/src/kb_usb.c, with the dongle's own IDs); over the air it
 * relays the keyboard's reports and the config protocol (src/radio.c).
 * The button pairs a keyboard; the LED shows pairing and the link.
 */

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <app_version.h>

#include "kb_usb.h"
#include "led.h"
#include "radio.h"

LOG_MODULE_REGISTER(kb_dongle, LOG_LEVEL_INF);

/* From the board (nRF52840 Dongle, matching board.json's pairButton). */
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static struct gpio_callback button_cb;

#define DEBOUNCE_MS 30

static void button_fn(struct k_work *work)
{
    ARG_UNUSED(work);
    if (gpio_pin_get_dt(&button) > 0) {
        radio_toggle_pairing();
    }
}
static K_WORK_DELAYABLE_DEFINE(button_work, button_fn);

static void on_button(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);
    k_work_reschedule(&button_work, K_MSEC(DEBOUNCE_MS));
}

static int button_init(void)
{
    if (!gpio_is_ready_dt(&button)) {
        return -ENODEV;
    }
    int err = gpio_pin_configure_dt(&button, GPIO_INPUT);
    err = err ? err : gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
    if (err) {
        return err;
    }
    gpio_init_callback(&button_cb, on_button, BIT(button.pin));
    return gpio_add_callback_dt(&button, &button_cb);
}

int main(void)
{
    LOG_INF("dongle firmware %s", APP_VERSION_STRING);

    if (led_init() != 0) {
        LOG_ERR("status LED unavailable");
    }
    /* Before USB, so the first config request finds the radio. */
    if (radio_init() != 0) {
        LOG_ERR("radio unavailable");
    }
    if (kb_usb_init() != 0) {
        LOG_ERR("USB init failed");
    }
    if (button_init() != 0) {
        LOG_ERR("pairing button unavailable");
    }
    return 0;
}
