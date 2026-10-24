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
 */
#include <stdio.h>
#include <string.h>

#include "default_keymap.h"
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

static const struct kb_protocol_ops ops = {
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
                        KB_PROTO_CAP_SETTINGS | KB_PROTO_CAP_INDICATORS,
        .encoders = 1,
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
