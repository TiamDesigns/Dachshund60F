#include "output.h"

#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/atomic.h>

#include "indicators.h"
#include "kb_ble.h"
#include "kb_esb.h"
#include "kb_usb.h"
#include "status_screen.h"

LOG_MODULE_REGISTER(output, LOG_LEVEL_INF);

static atomic_t preferred = ATOMIC_INIT(KB_OUTPUT_USB);
static atomic_t active = ATOMIC_INIT(KB_OUTPUT_NONE);

/* Scan thread only. */
static uint8_t shown_locks;
static uint8_t shown_profile = 0xFF;

#define HAS_DONGLE IS_ENABLED(CONFIG_KB_DONGLE)

bool output_available(uint8_t output)
{
    return kb_output_preferable(output) && (output != KB_OUTPUT_DONGLE || HAS_DONGLE);
}

static const char *output_name(uint8_t output)
{
    switch (output) {
    case KB_OUTPUT_USB:
        return "USB";
    case KB_OUTPUT_BLE:
        return "Bluetooth";
    case KB_OUTPUT_DONGLE:
        return "the dongle";
    default:
        return "no host";
    }
}

/* The preferred output, in Zephyr settings next to the Bluetooth bonds. */

struct saved_output {
    uint8_t version;
    uint8_t output;
};
#define SAVED_OUTPUT_VERSION 1u

static int output_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg)
{
    struct saved_output v;
    if (!settings_name_steq(name, "preferred", NULL)) {
        return -ENOENT;
    }
    if (len == sizeof(v) && read_cb(cb_arg, &v, sizeof(v)) == sizeof(v) &&
        v.version == SAVED_OUTPUT_VERSION && output_available(v.output)) {
        atomic_set(&preferred, v.output);
    }
    return 0;
}
SETTINGS_STATIC_HANDLER_DEFINE(kb_out, "kb/out", NULL, output_set, NULL, NULL);

static void save_fn(struct k_work *work)
{
    ARG_UNUSED(work);
    struct saved_output v = {.version = SAVED_OUTPUT_VERSION, .output = output_preferred()};
    int err = settings_save_one("kb/out/preferred", &v, sizeof(v));
    if (err) {
        LOG_WRN("saving the output failed (%d)", err);
    }
}
static K_WORK_DEFINE(save_work, save_fn);

int output_set_preferred(uint8_t output)
{
    if (!output_available(output)) {
        return -EINVAL;
    }
    if (atomic_set(&preferred, output) != output) {
        LOG_INF("preferring %s", output_name(output));
        /* Flash writes stall the CPU: not on the scan thread. */
        k_work_submit(&save_work);
    }
    return 0;
}

uint8_t output_preferred(void)
{
    return (uint8_t)atomic_get(&preferred);
}

enum kb_output output_active(void)
{
    return (enum kb_output)atomic_get(&active);
}

/* Sending */

static int send_on(enum kb_output out, enum kb_hid_report report, const uint8_t *data,
                   size_t len)
{
    switch (out) {
    case KB_OUTPUT_USB:
        return kb_usb_send(report, data, len);
    case KB_OUTPUT_BLE:
        return kb_ble_send(report, data, len);
    case KB_OUTPUT_DONGLE:
        return HAS_DONGLE ? kb_esb_send(report, data, len) : -EAGAIN;
    default:
        return -EAGAIN;
    }
}

int output_send(enum kb_hid_report report, const uint8_t *data, size_t len)
{
    return send_on(output_active(), report, data, len);
}

static void send_released(enum kb_hid_report report, const uint8_t *data, size_t len, void *ctx)
{
    (void)send_on((enum kb_output)(uintptr_t)ctx, report, data, len);
}

bool output_boot_protocol(void)
{
    switch (output_active()) {
    case KB_OUTPUT_USB:
        return kb_usb_boot_protocol();
    case KB_OUTPUT_BLE:
        return kb_ble_boot_protocol();
    case KB_OUTPUT_DONGLE:
        return HAS_DONGLE && kb_esb_boot_protocol();
    default:
        return false;
    }
}

static uint8_t active_locks(enum kb_output out)
{
    switch (out) {
    case KB_OUTPUT_USB:
        return kb_usb_locks();
    case KB_OUTPUT_BLE:
        return kb_ble_locks();
    case KB_OUTPUT_DONGLE:
        return HAS_DONGLE ? kb_esb_locks() : 0;
    default:
        return 0;
    }
}

bool output_poll(void)
{
    struct kb_output_status st = {
        .preferred = output_preferred(),
        .usb_ready = kb_usb_ready(),
        .ble = kb_ble_state(),
        .dongle_ready = HAS_DONGLE && kb_esb_ready(),
    };
    enum kb_output next = kb_output_select(&st);
    enum kb_output was = (enum kb_output)atomic_set(&active, next);

    /* Both flags are taken every time, so a stale one never fires later. */
    bool usb_resync = kb_usb_take_resync();
    bool ble_resync = kb_ble_take_resync();
    bool esb_resync = HAS_DONGLE && kb_esb_take_resync();
    bool resync = (next == KB_OUTPUT_USB && usb_resync) || (next == KB_OUTPUT_BLE && ble_resync) ||
                  (next == KB_OUTPUT_DONGLE && esb_resync);

    if (next != was) {
        /* Let go of everything on the host the keys are leaving. */
        if (was != KB_OUTPUT_NONE) {
            kb_hid_send_released(send_released, (void *)(uintptr_t)was);
        }
        LOG_INF("keys go to %s", output_name(next));
        resync = true;
    }

    uint8_t profile = kb_ble_profile();
    if (next != was || profile != shown_profile) {
        status_screen_set_transport((uint8_t)next, profile);
        shown_profile = profile;
    }
    uint8_t locks = active_locks(next);
    if (next != was || locks != shown_locks) {
        status_screen_set_locks(locks);
        indicators_set_locks(locks);
        shown_locks = locks;
    }
    return resync;
}
