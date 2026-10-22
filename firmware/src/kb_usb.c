/*
 * USB device (device_next stack) with one boot-protocol HID keyboard.
 *
 * Descriptors: VID/PID and strings from Kconfig (matching board.json), a
 * serial number unique per board from the factory device ID, and bcdDevice
 * from firmware/VERSION.
 */

#include "kb_usb.h"

#include <errno.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/class/usbd_hid.h>

#include <app_version.h>

#include "kb/version.h"
#include "status_screen.h"

LOG_MODULE_REGISTER(kb_usb, LOG_LEVEL_INF);

USBD_DEVICE_DEFINE(kb_usbd, DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
                   CONFIG_KB_USB_VID, CONFIG_KB_USB_PID);

USBD_DESC_LANG_DEFINE(kb_lang);
USBD_DESC_MANUFACTURER_DEFINE(kb_mfr, CONFIG_KB_USB_MANUFACTURER);
USBD_DESC_PRODUCT_DEFINE(kb_product, CONFIG_KB_USB_PRODUCT);
USBD_DESC_SERIAL_NUMBER_DEFINE(kb_sn);
USBD_DESC_CONFIG_DEFINE(kb_fs_cfg_desc, "Full-speed configuration");

/* bMaxPower is in 2 mA units. */
USBD_CONFIGURATION_DEFINE(kb_fs_config, 0, CONFIG_KB_USB_MAX_POWER_MA / 2, &kb_fs_cfg_desc);

static const uint8_t kbd_report_desc[] = HID_KEYBOARD_REPORT_DESC();

static const struct device *const hid_kbd = DEVICE_DT_GET(DT_NODELABEL(hid_kbd));

static atomic_t kbd_ready;
static uint8_t kbd_protocol = HID_PROTOCOL_REPORT;
static uint32_t kbd_idle_duration;
static uint8_t kbd_last_report[KB_USB_KBD_REPORT_SIZE];
static uint8_t kbd_led_state;

static void kbd_iface_ready(const struct device *dev, const bool ready)
{
    ARG_UNUSED(dev);
    atomic_set(&kbd_ready, ready);
    status_screen_set_transport(ready ? KB_TRANSPORT_USB : KB_TRANSPORT_NONE, 0);
    LOG_INF("keyboard interface %s", ready ? "ready" : "not ready");
}

static int kbd_get_report(const struct device *dev, const uint8_t type, const uint8_t id,
                          const uint16_t len, uint8_t *const buf)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(id);

    if (type != HID_REPORT_TYPE_INPUT || len < sizeof(kbd_last_report)) {
        return -ENOTSUP;
    }

    memcpy(buf, kbd_last_report, sizeof(kbd_last_report));
    return sizeof(kbd_last_report);
}

static int kbd_set_report(const struct device *dev, const uint8_t type, const uint8_t id,
                          const uint16_t len, const uint8_t *const buf)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(id);

    /* Lock-key LED output report (one byte): shown on the status screen now;
     * the indicator LEDs use it in phase 4.
     */
    if (type != HID_REPORT_TYPE_OUTPUT || len < 1) {
        return -ENOTSUP;
    }

    kbd_led_state = buf[0];
    status_screen_set_locks(kbd_led_state);
    return 0;
}

static void kbd_set_idle(const struct device *dev, const uint8_t id, const uint32_t duration)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(id);
    kbd_idle_duration = duration;
}

static uint32_t kbd_get_idle(const struct device *dev, const uint8_t id)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(id);
    return kbd_idle_duration;
}

static void kbd_set_protocol(const struct device *dev, const uint8_t proto)
{
    ARG_UNUSED(dev);
    kbd_protocol = proto;
    LOG_INF("host selected %s protocol", proto == HID_PROTOCOL_BOOT ? "boot" : "report");
}

static void kbd_output_report(const struct device *dev, const uint16_t len,
                              const uint8_t *const buf)
{
    ARG_UNUSED(dev);

    if (len >= 1) {
        kbd_led_state = buf[0];
        status_screen_set_locks(kbd_led_state);
    }
}

static const struct hid_device_ops kbd_ops = {
    .iface_ready = kbd_iface_ready,
    .get_report = kbd_get_report,
    .set_report = kbd_set_report,
    .set_idle = kbd_set_idle,
    .get_idle = kbd_get_idle,
    .set_protocol = kbd_set_protocol,
    .output_report = kbd_output_report,
};

int kb_usb_send_keyboard(const uint8_t report[KB_USB_KBD_REPORT_SIZE])
{
    if (!atomic_get(&kbd_ready)) {
        return -EAGAIN;
    }

    memcpy(kbd_last_report, report, sizeof(kbd_last_report));
    return hid_device_submit_report(hid_kbd, sizeof(kbd_last_report), kbd_last_report);
}

int kb_usb_init(void)
{
    int err;

    if (!device_is_ready(hid_kbd)) {
        LOG_ERR("HID keyboard device not ready");
        return -ENODEV;
    }

    err = hid_device_register(hid_kbd, kbd_report_desc, sizeof(kbd_report_desc), &kbd_ops);
    if (err) {
        LOG_ERR("HID register failed (%d)", err);
        return err;
    }

    err = usbd_add_descriptor(&kb_usbd, &kb_lang);
    err = err ? err : usbd_add_descriptor(&kb_usbd, &kb_mfr);
    err = err ? err : usbd_add_descriptor(&kb_usbd, &kb_product);
    err = err ? err : usbd_add_descriptor(&kb_usbd, &kb_sn);
    if (err) {
        LOG_ERR("adding string descriptors failed (%d)", err);
        return err;
    }

    err = usbd_add_configuration(&kb_usbd, USBD_SPEED_FS, &kb_fs_config);
    if (err) {
        LOG_ERR("adding configuration failed (%d)", err);
        return err;
    }

    err = usbd_register_all_classes(&kb_usbd, USBD_SPEED_FS, 1, NULL);
    if (err) {
        LOG_ERR("registering classes failed (%d)", err);
        return err;
    }

    /* Class codes come from the interface descriptors. */
    err = usbd_device_set_code_triple(&kb_usbd, USBD_SPEED_FS, 0, 0, 0);
    err = err ? err
              : usbd_device_set_bcd_device(&kb_usbd,
                                           kb_version_bcd_device(APP_VERSION_MAJOR,
                                                                 APP_VERSION_MINOR,
                                                                 APP_PATCHLEVEL));
    if (err) {
        LOG_ERR("setting device descriptor fields failed (%d)", err);
        return err;
    }

    err = usbd_init(&kb_usbd);
    if (err) {
        LOG_ERR("USB device init failed (%d)", err);
        return err;
    }

    err = usbd_enable(&kb_usbd);
    if (err) {
        LOG_ERR("USB device enable failed (%d)", err);
        return err;
    }

    return 0;
}
