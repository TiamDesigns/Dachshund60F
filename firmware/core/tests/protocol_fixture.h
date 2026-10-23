/*
 * A protocol instance over a small keymap with recording ops, shared by the
 * protocol tests and the fuzz targets.
 */
#ifndef PROTOCOL_FIXTURE_H
#define PROTOCOL_FIXTURE_H

#include <string.h>

#include "kb/keycodes.h"
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
    int saves, reverts, resets, reboots, identifies, locks, unlocks;
    bool reboot_bootloader;
    uint8_t identify_seconds;
    uint8_t save_status;
};

static struct fx_state fx;

static uint8_t fx_save(void *ctx)
{
    (void)ctx;
    fx.saves++;
    if (fx.save_status == KB_PROTO_OK) {
        memcpy(fx.saved, fx.live, sizeof(fx.saved));
    }
    return fx.save_status;
}

static uint8_t fx_revert(void *ctx)
{
    (void)ctx;
    fx.reverts++;
    memcpy(fx.live, fx.saved, sizeof(fx.live));
    return KB_PROTO_OK;
}

static uint8_t fx_factory_reset(void *ctx)
{
    (void)ctx;
    fx.resets++;
    memcpy(fx.live, fx_default, sizeof(fx.live));
    memcpy(fx.saved, fx_default, sizeof(fx.saved));
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
}

static void fx_unlock(void *ctx)
{
    (void)ctx;
    fx.unlocks++;
}

static const struct kb_protocol_ops fx_ops = {
    .save = fx_save,
    .revert = fx_revert,
    .factory_reset = fx_factory_reset,
    .reboot = fx_reboot,
    .identify = fx_identify,
    .lock = fx_lock,
    .unlock = fx_unlock,
};

static struct kb_protocol fx_proto;

static void fx_init(void)
{
    memset(&fx, 0, sizeof(fx));
    kb_keymap_init(&fx.keymap, fx.live, FX_LAYERS, FX_ROWS, FX_COLS, &fx_default[0][0][0]);
    memcpy(fx.saved, fx.live, sizeof(fx.saved));
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

#endif /* PROTOCOL_FIXTURE_H */
