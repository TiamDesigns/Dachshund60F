/*
 * Config protocol service. Requests arrive on the USB or Bluetooth thread,
 * wait in a small queue and are handled one at a time on their own thread,
 * so a slow command (a flash save) never holds up either. Each response
 * goes back the way its request came.
 */

#include "config.h"

#include <errno.h>
#include <string.h>

#include <zephyr/drivers/hwinfo.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/sys/util.h>

#include <app_version.h>

#include "bootloader.h"
#include "encoder_input.h"
#include "fw_info.h"
#include "indicators.h"
#include "kb/protocol.h"
#include "kb_ble.h"
#include "kb_usb.h"
#include "keyboard.h"
#include "output.h"

LOG_MODULE_REGISTER(config, LOG_LEVEL_INF);

#define CONFIG_STACK_SIZE 2048
#define CONFIG_PRIORITY K_PRIO_PREEMPT(5)
/* Long enough for the response to reach the host before the board drops off. */
#define REBOOT_DELAY_MS 100

struct request {
    uint8_t source; /* KB_OUTPUT_USB or KB_OUTPUT_BLE */
    uint8_t len;
    uint8_t data[KB_PROTO_REPORT_SIZE];
};

K_MSGQ_DEFINE(request_queue, sizeof(struct request), 4, 1);
K_THREAD_STACK_DEFINE(config_stack, CONFIG_STACK_SIZE);
static struct k_thread config_thread_data;

/* git describe of this build; missing when built outside git. */
#ifdef APP_BUILD_VERSION
#define BUILD_ID STRINGIFY(APP_BUILD_VERSION)
#else
#define BUILD_ID "unknown"
#endif

static bool reboot_to_bootloader;

static void reboot_fn(struct k_work *work)
{
    ARG_UNUSED(work);
    if (reboot_to_bootloader) {
        bootloader_reboot_to_uf2();
    } else {
        sys_reboot(SYS_REBOOT_COLD);
    }
}
static K_WORK_DELAYABLE_DEFINE(reboot_work, reboot_fn);

static uint8_t status_of(int err)
{
    switch (err) {
    case 0:
        return KB_PROTO_OK;
    case -ENOSPC:
        return KB_PROTO_STORAGE_FULL;
    default:
        return KB_PROTO_STORAGE_ERROR;
    }
}

static uint8_t op_save(void *ctx)
{
    ARG_UNUSED(ctx);
    return status_of(keyboard_save());
}

static uint8_t op_revert(void *ctx)
{
    ARG_UNUSED(ctx);
    return status_of(keyboard_revert());
}

static uint8_t op_factory_reset(void *ctx)
{
    ARG_UNUSED(ctx);
    return status_of(keyboard_factory_reset());
}

static void op_reboot(bool bootloader, void *ctx)
{
    ARG_UNUSED(ctx);
    LOG_INF("host asked for a reboot%s", bootloader ? " into UF2" : "");
    reboot_to_bootloader = bootloader;
    k_work_schedule(&reboot_work, K_MSEC(REBOOT_DELAY_MS));
}

static void op_identify(uint8_t seconds, void *ctx)
{
    ARG_UNUSED(ctx);
    indicators_identify(seconds);
}

static void op_features_changed(void *ctx)
{
    ARG_UNUSED(ctx);
    keyboard_features_changed();
}

static void op_settings_changed(void *ctx)
{
    ARG_UNUSED(ctx);
    keyboard_settings_changed();
}

static void op_get_connection(struct kb_proto_get_connection_resp *out, void *ctx)
{
    ARG_UNUSED(ctx);
    out->active_output = (uint8_t)output_active();
    out->preferred_output = output_preferred();
    out->usb = kb_usb_ready() ? 1u : 0u;
    out->ble = kb_ble_state();
    out->profile = kb_ble_profile();
    out->bonded = kb_ble_bonded();
}

static uint8_t op_set_output(uint8_t output, void *ctx)
{
    ARG_UNUSED(ctx);
    return output_set_preferred(output) == 0 ? KB_PROTO_OK : KB_PROTO_BAD_VALUE;
}

static uint8_t ble_status(int err)
{
    switch (err) {
    case 0:
        return KB_PROTO_OK;
    case -EINVAL:
        return KB_PROTO_OUT_OF_RANGE;
    default:
        return KB_PROTO_BUSY; /* Bluetooth isn't up */
    }
}

static uint8_t op_select_profile(uint8_t profile, void *ctx)
{
    ARG_UNUSED(ctx);
    return ble_status(kb_ble_select_profile(profile));
}

static uint8_t op_clear_profile(uint8_t profile, void *ctx)
{
    ARG_UNUSED(ctx);
    return ble_status(kb_ble_clear_profile(profile));
}

static void op_lock(void *ctx)
{
    ARG_UNUSED(ctx);
    keyboard_lock();
}

static void op_unlock(void *ctx)
{
    ARG_UNUSED(ctx);
    keyboard_unlock();
}

static const struct kb_protocol_ops ops = {
    .save = op_save,
    .revert = op_revert,
    .factory_reset = op_factory_reset,
    .reboot = op_reboot,
    .identify = op_identify,
    .features_changed = op_features_changed,
    .settings_changed = op_settings_changed,
    .get_connection = op_get_connection,
    .set_output = op_set_output,
    .select_profile = op_select_profile,
    .clear_profile = op_clear_profile,
    .lock = op_lock,
    .unlock = op_unlock,
};

static struct kb_protocol protocol = {
    .ops = &ops,
    .build = BUILD_ID,
    /* Matrix streaming and battery join in later phases. */
    .capabilities = KB_PROTO_CAP_KEYMAP | KB_PROTO_CAP_PERSISTENCE | KB_PROTO_CAP_IDENTIFY |
                    KB_PROTO_CAP_ADVANCED_KEYS | KB_PROTO_CAP_MACROS | KB_PROTO_CAP_SETTINGS |
                    KB_PROTO_CAP_INDICATORS | KB_PROTO_CAP_BLUETOOTH,
    .macro_buffer_size = KB_MACRO_BUFFER,
};

/* USB or Bluetooth thread: copy and queue. A host that floods requests loses the extras. */
static void queue_request(uint8_t source, const uint8_t *data, size_t len)
{
    struct request req = {.source = source, .len = (uint8_t)MIN(len, sizeof(req.data))};
    memcpy(req.data, data, req.len);
    if (k_msgq_put(&request_queue, &req, K_NO_WAIT) != 0) {
        LOG_WRN("config request dropped: queue full");
    }
}

static void on_usb_request(const uint8_t *data, size_t len)
{
    queue_request(KB_OUTPUT_USB, data, len);
}

static void on_ble_request(const uint8_t *data, size_t len)
{
    queue_request(KB_OUTPUT_BLE, data, len);
}

static void config_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    struct request req;
    uint8_t resp[KB_PROTO_REPORT_SIZE];

    for (;;) {
        k_msgq_get(&request_queue, &req, K_FOREVER);
        if (!kb_protocol_handle(&protocol, req.data, req.len, resp)) {
            continue; /* no header to answer */
        }
        int err = req.source == KB_OUTPUT_BLE ? kb_ble_send_config(resp) : kb_usb_send_config(resp);
        if (err) {
            LOG_WRN("config response dropped (%d)", err);
        }
    }
}

/* Same text as the USB serial number string (uppercase hex of the device ID). */
static char serial[KB_PROTO_GET_SERIAL_RESP_SIZE + 1];

static void load_serial(void)
{
    static const char hex[] = "0123456789ABCDEF";
    uint8_t id[16];
    ssize_t n = hwinfo_get_device_id(id, sizeof(id));

    for (ssize_t i = 0; i < n && (size_t)i * 2 + 1 < sizeof(serial) - 1; i++) {
        serial[i * 2] = hex[id[i] >> 4];
        serial[i * 2 + 1] = hex[id[i] & 0xF];
    }
    protocol.serial = serial;
}

int config_start(void)
{
    /* From the info block, which also keeps it in the image. */
    protocol.firmware_major = fw_info.major;
    protocol.firmware_minor = fw_info.minor;
    protocol.firmware_patch = fw_info.patch;
    protocol.model = fw_info.model;
    protocol.keymap = keyboard_keymap();
    protocol.features = keyboard_features();
    protocol.macro_edit = keyboard_macro_edit();
    protocol.settings = keyboard_settings();
    protocol.indicators = keyboard_indicators();
    protocol.encoders = (uint8_t)encoder_input_count();
    if (protocol.encoders > 0) {
        protocol.capabilities |= KB_PROTO_CAP_ENCODERS;
    }
    protocol.ble_profiles = kb_ble_profile_count();
    load_serial();
    kb_usb_set_config_rx(on_usb_request);
    kb_ble_set_config_rx(on_ble_request);

    k_thread_create(&config_thread_data, config_stack, K_THREAD_STACK_SIZEOF(config_stack),
                    config_thread, NULL, NULL, NULL, CONFIG_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(&config_thread_data, "kb_config");
    return 0;
}
