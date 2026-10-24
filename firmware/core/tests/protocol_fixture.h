/*
 * A protocol instance over a small keymap with recording ops, shared by the
 * protocol tests and the fuzz targets.
 */
#ifndef PROTOCOL_FIXTURE_H
#define PROTOCOL_FIXTURE_H

#include <string.h>

#include "kb/keycodes.h"
#include "kb/output.h"
#include "kb/protocol.h"

#define FX_LAYERS 4
#define FX_ROWS 2
#define FX_COLS 5
#define FX_KEYS (FX_LAYERS * FX_ROWS * FX_COLS)

static const kb_keycode_t fx_default[FX_LAYERS][FX_ROWS][FX_COLS] = {
    {{KC_A, KC_B, KC_C, KC_D, MO(1)}, {KC_1, KC_2, KC_3, KC_4, KC_5}},
    {{KC_F1, KC_F2, _______, _______, _______}, {_______, _______, KC_VOLU, QK_BOOT, _______}},
    {{_______, _______, _______, _______, _______}, {_______, _______, _______, _______, _______}},
    {{_______, _______, _______, _______, _______}, {_______, _______, _______, _______, _______}},
};

struct fx_state {
    kb_keycode_t live[FX_KEYS];
    kb_keycode_t saved[FX_KEYS];
    struct kb_keymap keymap;
    struct kb_features features, saved_features;
    uint8_t macro_edit[KB_MACRO_BUFFER];
    struct kb_settings settings, saved_settings;
    struct kb_indicator_config indicators, saved_indicators;
    int saves, reverts, resets, reboots, identifies, locks, unlocks, feature_changes,
        settings_changes;
    bool locked;
    bool reboot_bootloader;
    uint8_t identify_seconds;
    uint8_t save_status;
    /* Bluetooth: what get_connection reports, and what the ops return. */
    bool usb_ready;
    uint8_t ble, preferred, profile, bonded, ble_status;
    int profile_selects, profile_clears;
    /* get_power and get_timing */
    struct kb_proto_get_power_resp power;
    uint32_t scans;
    int timing_resets;
};

static struct fx_state fx;

static uint8_t fx_save(void *ctx)
{
    (void)ctx;
    fx.saves++;
    if (fx.save_status == KB_PROTO_OK) {
        memcpy(fx.saved, fx.live, sizeof(fx.saved));
        fx.saved_features = fx.features;
        fx.saved_settings = fx.settings;
        fx.saved_indicators = fx.indicators;
    }
    return fx.save_status;
}

static uint8_t fx_revert(void *ctx)
{
    (void)ctx;
    fx.reverts++;
    memcpy(fx.live, fx.saved, sizeof(fx.live));
    fx.features = fx.saved_features;
    fx.settings = fx.saved_settings;
    fx.indicators = fx.saved_indicators;
    return KB_PROTO_OK;
}

static void fx_default_settings(void)
{
    memset(&fx.settings, 0, sizeof(fx.settings));
    fx.settings.debounce_algo = 4;
    fx.settings.debounce_ms = 5;
    fx.settings.tapping_term_ms = 200;
    fx.settings.nkro = true;
    fx.settings.sleep_timeout_min = 15;
    fx.settings.indicator_brightness = 13;
    kb_indicators_default_config(&fx.indicators);
    fx.saved_settings = fx.settings;
    fx.saved_indicators = fx.indicators;
}

static uint8_t fx_factory_reset(void *ctx)
{
    (void)ctx;
    fx.resets++;
    memcpy(fx.live, fx_default, sizeof(fx.live));
    memcpy(fx.saved, fx_default, sizeof(fx.saved));
    kb_features_default(&fx.features);
    fx.saved_features = fx.features;
    fx_default_settings();
    return KB_PROTO_OK;
}

static void fx_reboot(bool bootloader, void *ctx)
{
    (void)ctx;
    fx.reboots++;
    fx.reboot_bootloader = bootloader;
}

static void fx_identify(uint8_t seconds, void *ctx)
{
    (void)ctx;
    fx.identifies++;
    fx.identify_seconds = seconds;
}

static void fx_lock(void *ctx)
{
    (void)ctx;
    fx.locks++;
    fx.locked = true;
}

static void fx_unlock(void *ctx)
{
    (void)ctx;
    fx.unlocks++;
    fx.locked = false;
}

static void fx_features_changed(void *ctx)
{
    (void)ctx;
    /* Called with the lock held. */
    fx.feature_changes += fx.locked ? 1 : 1000;
}

static void fx_settings_changed(void *ctx)
{
    (void)ctx;
    fx.settings_changes += fx.locked ? 1 : 1000;
}

static void fx_get_connection(struct kb_proto_get_connection_resp *out, void *ctx)
{
    (void)ctx;
    struct kb_output_status st = {.preferred = fx.preferred, .usb_ready = fx.usb_ready, .ble = fx.ble};
    out->active_output = (uint8_t)kb_output_select(&st);
    out->preferred_output = fx.preferred;
    out->usb = fx.usb_ready ? 1u : 0u;
    out->ble = fx.ble;
    out->profile = fx.profile;
    out->bonded = fx.bonded;
}

static uint8_t fx_set_output(uint8_t output, void *ctx)
{
    (void)ctx;
    fx.preferred = output;
    return KB_PROTO_OK;
}

static uint8_t fx_select_profile(uint8_t profile, void *ctx)
{
    (void)ctx;
    fx.profile_selects++;
    if (fx.ble_status == KB_PROTO_OK) {
        fx.profile = profile;
        fx.ble = KB_BLE_ADVERTISING;
    }
    return fx.ble_status;
}

static uint8_t fx_clear_profile(uint8_t profile, void *ctx)
{
    (void)ctx;
    fx.profile_clears++;
    if (fx.ble_status == KB_PROTO_OK) {
        fx.bonded &= (uint8_t)~(1u << profile);
    }
    return fx.ble_status;
}

static void fx_get_power(struct kb_proto_get_power_resp *out, void *ctx)
{
    (void)ctx;
    *out = fx.power;
}

static void fx_get_timing(struct kb_proto_get_timing_resp *out, bool reset, void *ctx)
{
    (void)ctx;
    out->scan_period_us = 1000;
    out->scans = fx.scans;
    if (reset) {
        fx.scans = 0;
        fx.timing_resets++;
    }
}

static const struct kb_protocol_ops fx_ops = {
    .get_power = fx_get_power,
    .get_timing = fx_get_timing,
    .get_connection = fx_get_connection,
    .set_output = fx_set_output,
    .select_profile = fx_select_profile,
    .clear_profile = fx_clear_profile,
    .save = fx_save,
    .revert = fx_revert,
    .factory_reset = fx_factory_reset,
    .reboot = fx_reboot,
    .identify = fx_identify,
    .lock = fx_lock,
    .unlock = fx_unlock,
    .features_changed = fx_features_changed,
    .settings_changed = fx_settings_changed,
};

static struct kb_protocol fx_proto;

static void fx_init(void)
{
    memset(&fx, 0, sizeof(fx));
    kb_keymap_init(&fx.keymap, fx.live, FX_LAYERS, FX_ROWS, FX_COLS, &fx_default[0][0][0]);
    memcpy(fx.saved, fx.live, sizeof(fx.saved));
    kb_features_default(&fx.features);
    fx.saved_features = fx.features;
    memcpy(fx.macro_edit, fx.features.macros, KB_MACRO_BUFFER);
    fx_default_settings();
    fx.usb_ready = true;
    fx.ble = KB_BLE_SECURE;
    fx.preferred = KB_OUTPUT_USB;
    fx.bonded = 0x1;
    fx.power = (struct kb_proto_get_power_resp){
        .battery_percent = 80, .battery_mv = 4020, .charging = 0, .usb_power = 1};
    fx.scans = 1234;
    fx_proto = (struct kb_protocol){
        .keymap = &fx.keymap,
        .ops = &fx_ops,
        .firmware_major = 0,
        .firmware_minor = 4,
        .firmware_patch = 0,
        .build = "abc1234",
        .model = "Dachshund60F",
        .serial = "0123456789ABCDEF",
        .capabilities = KB_PROTO_CAP_KEYMAP | KB_PROTO_CAP_IDENTIFY,
        .encoders = 1,
        .ble_profiles = 3,
        .macro_buffer_size = KB_MACRO_BUFFER,
        .features = &fx.features,
        .macro_edit = fx.macro_edit,
        .settings = &fx.settings,
        .indicators = &fx.indicators,
    };
}

/* Every keycode in the live keymap is one the firmware accepts. */
static bool fx_keymap_valid(void)
{
    for (size_t i = 0; i < FX_KEYS; i++) {
        if (!kb_keycode_is_valid(fx.live[i], FX_LAYERS)) {
            return false;
        }
    }
    return true;
}

/* The live tables are a set the engine accepts. */
static bool fx_features_valid(void)
{
    return kb_features_valid(&fx.features, FX_LAYERS, FX_ROWS, FX_COLS);
}

#endif /* PROTOCOL_FIXTURE_H */
