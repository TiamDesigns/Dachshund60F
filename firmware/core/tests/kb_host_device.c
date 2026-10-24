/*
 * The keyboard's config side, built for the host: the core protocol handler
 * over the firmware's default keymap, with save/revert/factory reset going
 * to a simulated flash through the real store and settings code. host-lib's
 * round-trip test spawns it and talks to it as it would to a board.
 *
 * Frames are lines of hex on stdin and stdout: each request line is one
 * output report (up to 64 bytes), each response line one 64-byte input
 * report. A request too short to answer gets an empty line. A reboot
 * reloads the keymap and settings from the simulated flash, as a power
 * cycle would.
 *
 * Its connection is simulated: plugged into USB, Bluetooth advertising on
 * host 1 of 3, with host 2 paired.
 */
#include <stdio.h>
#include <string.h>

#include "default_keymap.h"
#include "kb/output.h"
#include "kb/protocol.h"
#include "kb/settings.h"
#include "kb/store.h"

/* The firmware's storage partition: 32 KB in 4 KB sectors. */
#define SIM_SIZE 32768u
#define SIM_SECTOR 4096u
#include "sim_flash.h"

#define LAYERS DEFAULT_KEYMAP_LAYERS
#define ROWS DEFAULT_KEYMAP_ROWS
#define COLS DEFAULT_KEYMAP_COLS

static kb_keycode_t keys[LAYERS * ROWS * COLS];
static struct kb_keymap keymap;
static struct kb_settings settings;
static struct kb_indicator_config indicators;
static struct kb_features features;
static uint8_t macro_edit[KB_MACRO_BUFFER];
static struct sim_flash flash;
static struct kb_store store;
static uint8_t blob[SIM_SECTOR];

static void load_defaults(void)
{
    memset(&settings, 0, sizeof(settings));
    settings.debounce_algo = 4; /* eager on press, as the firmware */
    settings.debounce_ms = 5;
    settings.tapping_term_ms = 200;
    settings.nkro = true;
    settings.sleep_timeout_min = 15;
    settings.indicator_brightness = 13;
    kb_indicators_default_config(&indicators);
    (void)kb_keymap_init(&keymap, keys, LAYERS, ROWS, COLS, &default_keymap[0][0][0]);
    default_features(&features);
}

static int load_saved(void)
{
    uint16_t schema;
    size_t len;
    int err = kb_store_load(&store, &schema, blob, sizeof(blob), &len);
    if (err) {
        return err;
    }
    if (kb_settings_decode(schema, blob, len, &keymap, &settings) < 0 ||
        kb_features_decode(schema, blob, len, &features, LAYERS, ROWS, COLS) < 0 ||
        kb_indicators_decode(schema, blob, len, &indicators) < 0) {
        return -KB_STORE_EIO;
    }
    return 0;
}

static uint8_t status_of(int err)
{
    if (err == 0) {
        return KB_PROTO_OK;
    }
    return err == -KB_STORE_ENOSPC ? KB_PROTO_STORAGE_FULL : KB_PROTO_STORAGE_ERROR;
}

static uint8_t op_save(void *ctx)
{
    (void)ctx;
    size_t len = kb_settings_encode(&keymap, &settings, blob, sizeof(blob));
    size_t more = len ? kb_features_encode(&features, LAYERS, blob + len, sizeof(blob) - len) : 0;
    len = more ? len + more : 0;
    more = len ? kb_indicators_encode(&indicators, LAYERS, blob + len, sizeof(blob) - len) : 0;
    return status_of(more ? kb_store_save(&store, KB_SETTINGS_SCHEMA, blob, len + more)
                          : -KB_STORE_ENOSPC);
}

static uint8_t op_revert(void *ctx)
{
    (void)ctx;
    load_defaults();
    int err = load_saved();
    return status_of(err == -KB_STORE_ENOENT ? 0 : err);
}

static uint8_t op_factory_reset(void *ctx)
{
    (void)ctx;
    int err = kb_store_wipe(&store);
    load_defaults();
    return status_of(err);
}

static void op_reboot(bool bootloader, void *ctx)
{
    (void)ctx;
    (void)bootloader;
    load_defaults();
    if (kb_store_mount(&store, &flash.flash) == 0) {
        (void)load_saved();
    }
    memcpy(macro_edit, features.macros, KB_MACRO_BUFFER);
}

static void op_identify(uint8_t seconds, void *ctx)
{
    (void)seconds;
    (void)ctx;
}

#define BLE_PROFILES 3u
static uint8_t preferred = KB_OUTPUT_USB;
static uint8_t ble_profile;
static uint8_t ble_bonded = 0x2;

static void op_get_connection(struct kb_proto_get_connection_resp *out, void *ctx)
{
    (void)ctx;
    struct kb_output_status st = {.preferred = preferred, .usb_ready = true, .ble = KB_BLE_ADVERTISING};
    out->active_output = (uint8_t)kb_output_select(&st);
    out->preferred_output = preferred;
    out->usb = 1;
    out->ble = st.ble;
    out->profile = ble_profile;
    out->bonded = ble_bonded;
}

static uint8_t op_set_output(uint8_t output, void *ctx)
{
    (void)ctx;
    preferred = output;
    return KB_PROTO_OK;
}

static uint8_t op_select_profile(uint8_t profile, void *ctx)
{
    (void)ctx;
    ble_profile = profile;
    return KB_PROTO_OK;
}

static uint8_t op_clear_profile(uint8_t profile, void *ctx)
{
    (void)ctx;
    ble_bonded &= (uint8_t)~(1u << profile);
    return KB_PROTO_OK;
}

/* A dongle is paired but out of range (link 1), as on a desk with it unplugged. */
static struct kb_proto_get_dongle_resp dongle = {.paired = 1, .link = 1};

static void op_get_dongle(struct kb_proto_get_dongle_resp *out, void *ctx)
{
    (void)ctx;
    *out = dongle;
}

static uint8_t op_pair_dongle(uint8_t mode, void *ctx)
{
    (void)ctx;
    dongle.pairing = mode == KB_PROTO_PAIR_START ? 1u : 0u;
    if (mode == KB_PROTO_PAIR_FORGET) {
        dongle.paired = 0;
        dongle.link = 0;
    }
    return KB_PROTO_OK;
}

/* On USB, charging, last read at 72% on battery. */
static void op_get_power(struct kb_proto_get_power_resp *out, void *ctx)
{
    (void)ctx;
    out->battery_percent = 72;
    out->battery_mv = 3960;
    out->charging = 1;
    out->usb_power = 1;
}

static uint32_t timing_scans = 5000;

static void op_get_timing(struct kb_proto_get_timing_resp *out, bool reset, void *ctx)
{
    (void)ctx;
    out->scan_period_us = 1000;
    out->scan_avg_us = 95;
    out->scan_max_us = 140;
    out->report_avg_us = 30;
    out->report_max_us = 80;
    out->key_wakes = 12;
    out->scans = timing_scans;
    if (reset) {
        timing_scans = 0;
    }
}

static const struct kb_protocol_ops ops = {
    .get_power = op_get_power,
    .get_timing = op_get_timing,
    .get_connection = op_get_connection,
    .set_output = op_set_output,
    .get_dongle = op_get_dongle,
    .pair_dongle = op_pair_dongle,
    .select_profile = op_select_profile,
    .clear_profile = op_clear_profile,
    .save = op_save,
    .revert = op_revert,
    .factory_reset = op_factory_reset,
    .reboot = op_reboot,
    .identify = op_identify,
};

static int hex_value(int c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

int main(void)
{
    struct kb_protocol protocol = {
        .keymap = &keymap,
        .ops = &ops,
        .firmware_major = 0,
        .firmware_minor = 0,
        .firmware_patch = 0,
        .build = "host",
        .model = "Dachshund60F",
        .serial = "HOST0001",
        .capabilities = KB_PROTO_CAP_KEYMAP | KB_PROTO_CAP_PERSISTENCE | KB_PROTO_CAP_IDENTIFY |
                        KB_PROTO_CAP_ADVANCED_KEYS | KB_PROTO_CAP_MACROS | KB_PROTO_CAP_ENCODERS |
                        KB_PROTO_CAP_SETTINGS | KB_PROTO_CAP_INDICATORS | KB_PROTO_CAP_BLUETOOTH |
                        KB_PROTO_CAP_BATTERY | KB_PROTO_CAP_TIMING | KB_PROTO_CAP_DONGLE,
        .encoders = 1,
        .ble_profiles = BLE_PROFILES,
        .macro_buffer_size = KB_MACRO_BUFFER,
        .features = &features,
        .macro_edit = macro_edit,
        .settings = &settings,
        .indicators = &indicators,
    };

    sim_init(&flash);
    op_reboot(false, NULL);

    char line[4 * KB_PROTO_REPORT_SIZE];
    while (fgets(line, sizeof(line), stdin)) {
        uint8_t req[KB_PROTO_REPORT_SIZE];
        size_t len = 0;
        for (size_t i = 0; line[i] && line[i + 1] && len < sizeof(req); i += 2) {
            int hi = hex_value(line[i]);
            int lo = hex_value(line[i + 1]);
            if (hi < 0 || lo < 0) {
                break;
            }
            req[len++] = (uint8_t)(hi << 4 | lo);
        }

        uint8_t resp[KB_PROTO_REPORT_SIZE];
        if (kb_protocol_handle(&protocol, req, len, resp)) {
            for (size_t i = 0; i < sizeof(resp); i++) {
                printf("%02x", resp[i]);
            }
        }
        printf("\n");
        fflush(stdout);
    }
    return 0;
}
