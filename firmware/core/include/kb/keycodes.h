/*
 * 16-bit keycode model.
 *
 * Names follow the QMK-style ids in host-lib/src/keycodes.ts, so a keycode
 * reads the same in the firmware, the mock board and Doghouse (the host-lib
 * tests check that every id there is defined here).
 *
 *   0x0000          KC_NO
 *   0x0001          KC_TRNS (transparent: use the key on the next lower layer)
 *   0x0004-0x00A4   HID keyboard usages (page 0x07)
 *   0x00E0-0x00E7   modifiers (page 0x07)
 *   0x0400-0x07FF   consumer control: 0x0400 | usage (page 0x0C, usage < 0x400)
 *   0x0800-0x08FF   system control: 0x0800 | usage (generic desktop page)
 *   0x2000-0x3FFF   key with modifiers held: 0x2000 | mods << 8 | key
 *   0x4000-0x4FFF   layer actions: 0x4000 | op << 8 | layer
 *   0x5000-0x501F   one-shot modifiers: 0x5000 | mods
 *   0x6000-0x6FFF   layer-tap: 0x6000 | layer << 8 | key (layers 0-15)
 *   0x7000-0x7FFF   firmware actions (bootloader, connection, mouse, ...)
 *   0x8000-0x9FFF   mod-tap: 0x8000 | mods << 8 | key
 *   0xA000-0xA01F   macros: M(n)
 *   0xA100-0xA10F   tap dance: TD(n)
 *   0xA200-0xFFFF   reserved
 *
 * "mods" is 5 bits: Ctrl, Shift, Alt, GUI, plus a right-hand flag in bit 4,
 * so one keycode holds left or right modifiers but not both.
 */
#ifndef KB_KEYCODES_H
#define KB_KEYCODES_H

#include <stdbool.h>
#include <stdint.h>

typedef uint16_t kb_keycode_t;

/* Ranges */
#define KB_KC_BASIC_MAX 0x00FFu
#define KB_KC_CONSUMER 0x0400u
#define KB_KC_CONSUMER_MAX 0x07FFu
#define KB_KC_SYSTEM 0x0800u
#define KB_KC_SYSTEM_MAX 0x08FFu
#define KB_KC_MODDED 0x2000u
#define KB_KC_MODDED_MAX 0x3FFFu
#define KB_KC_LAYER 0x4000u
#define KB_KC_LAYER_MAX 0x4FFFu
#define KB_KC_OSM 0x5000u
#define KB_KC_OSM_MAX 0x501Fu
#define KB_KC_LT 0x6000u
#define KB_KC_LT_MAX 0x6FFFu
#define KB_KC_ACTION 0x7000u
#define KB_KC_ACTION_MAX 0x7FFFu
#define KB_KC_MT 0x8000u
#define KB_KC_MT_MAX 0x9FFFu
#define KB_KC_MACRO 0xA000u
#define KB_KC_MACRO_MAX 0xA01Fu
#define KB_KC_TAP_DANCE 0xA100u
#define KB_KC_TAP_DANCE_MAX 0xA10Fu

/* 5-bit modifier field */
#define MOD_LCTL 0x01u
#define MOD_LSFT 0x02u
#define MOD_LALT 0x04u
#define MOD_LGUI 0x08u
#define MOD_RIGHT 0x10u
#define MOD_RCTL (MOD_RIGHT | MOD_LCTL)
#define MOD_RSFT (MOD_RIGHT | MOD_LSFT)
#define MOD_RALT (MOD_RIGHT | MOD_LALT)
#define MOD_RGUI (MOD_RIGHT | MOD_LGUI)

/* Layer action ops, in bits 8-11 of a layer keycode */
#define KB_LAYER_OP_MO 0u  /* momentary: on while held */
#define KB_LAYER_OP_TG 1u  /* toggle */
#define KB_LAYER_OP_TO 2u  /* turn this layer on and every other layer off */
#define KB_LAYER_OP_DF 3u  /* set the default (base) layer */
#define KB_LAYER_OP_OSL 4u /* one-shot: on for the next key press */
#define KB_LAYER_OP_COUNT 5u

/* Constructors */
#define KB_KC_MODS(mods, kc) ((kb_keycode_t)(KB_KC_MODDED | (((mods) & 0x1Fu) << 8) | ((kc) & 0xFFu)))
#define KB_KC_LAYER_OP(op, layer) ((kb_keycode_t)(KB_KC_LAYER | (((op) & 0xFu) << 8) | ((layer) & 0xFFu)))

#define MO(layer) KB_KC_LAYER_OP(KB_LAYER_OP_MO, layer)
#define TG(layer) KB_KC_LAYER_OP(KB_LAYER_OP_TG, layer)
#define TO(layer) KB_KC_LAYER_OP(KB_LAYER_OP_TO, layer)
#define DF(layer) KB_KC_LAYER_OP(KB_LAYER_OP_DF, layer)
#define OSL(layer) KB_KC_LAYER_OP(KB_LAYER_OP_OSL, layer)
#define OSM(mods) ((kb_keycode_t)(KB_KC_OSM | ((mods) & 0x1Fu)))
#define LT(layer, kc) ((kb_keycode_t)(KB_KC_LT | (((layer) & 0xFu) << 8) | ((kc) & 0xFFu)))
#define MT(mods, kc) ((kb_keycode_t)(KB_KC_MT | (((mods) & 0x1Fu) << 8) | ((kc) & 0xFFu)))

#define LCTL(kc) KB_KC_MODS(MOD_LCTL, kc)
#define LSFT(kc) KB_KC_MODS(MOD_LSFT, kc)
#define LALT(kc) KB_KC_MODS(MOD_LALT, kc)
#define LGUI(kc) KB_KC_MODS(MOD_LGUI, kc)
#define RCTL(kc) KB_KC_MODS(MOD_RCTL, kc)
#define RSFT(kc) KB_KC_MODS(MOD_RSFT, kc)
#define RALT(kc) KB_KC_MODS(MOD_RALT, kc)
#define RGUI(kc) KB_KC_MODS(MOD_RGUI, kc)

#define LCTL_T(kc) MT(MOD_LCTL, kc)
#define LSFT_T(kc) MT(MOD_LSFT, kc)
#define LALT_T(kc) MT(MOD_LALT, kc)
#define LGUI_T(kc) MT(MOD_LGUI, kc)
#define RCTL_T(kc) MT(MOD_RCTL, kc)
#define RSFT_T(kc) MT(MOD_RSFT, kc)
#define RALT_T(kc) MT(MOD_RALT, kc)
#define RGUI_T(kc) MT(MOD_RGUI, kc)

/* Special */
#define KC_NO 0x0000u
#define KC_TRNS 0x0001u
#define XXXXXXX KC_NO
#define _______ KC_TRNS

/* HID keyboard page (0x07) */
#define KC_A 0x0004u
#define KC_B 0x0005u
#define KC_C 0x0006u
#define KC_D 0x0007u
#define KC_E 0x0008u
#define KC_F 0x0009u
#define KC_G 0x000Au
#define KC_H 0x000Bu
#define KC_I 0x000Cu
#define KC_J 0x000Du
#define KC_K 0x000Eu
#define KC_L 0x000Fu
#define KC_M 0x0010u
#define KC_N 0x0011u
#define KC_O 0x0012u
#define KC_P 0x0013u
#define KC_Q 0x0014u
#define KC_R 0x0015u
#define KC_S 0x0016u
#define KC_T 0x0017u
#define KC_U 0x0018u
#define KC_V 0x0019u
#define KC_W 0x001Au
#define KC_X 0x001Bu
#define KC_Y 0x001Cu
#define KC_Z 0x001Du
#define KC_1 0x001Eu
#define KC_2 0x001Fu
#define KC_3 0x0020u
#define KC_4 0x0021u
#define KC_5 0x0022u
#define KC_6 0x0023u
#define KC_7 0x0024u
#define KC_8 0x0025u
#define KC_9 0x0026u
#define KC_0 0x0027u
#define KC_ENT 0x0028u
#define KC_ESC 0x0029u
#define KC_BSPC 0x002Au
#define KC_TAB 0x002Bu
#define KC_SPC 0x002Cu
#define KC_MINS 0x002Du
#define KC_EQL 0x002Eu
#define KC_LBRC 0x002Fu
#define KC_RBRC 0x0030u
#define KC_BSLS 0x0031u
#define KC_NUHS 0x0032u
#define KC_SCLN 0x0033u
#define KC_QUOT 0x0034u
#define KC_GRV 0x0035u
#define KC_COMM 0x0036u
#define KC_DOT 0x0037u
#define KC_SLSH 0x0038u
#define KC_CAPS 0x0039u
#define KC_F1 0x003Au
#define KC_F2 0x003Bu
#define KC_F3 0x003Cu
#define KC_F4 0x003Du
#define KC_F5 0x003Eu
#define KC_F6 0x003Fu
#define KC_F7 0x0040u
#define KC_F8 0x0041u
#define KC_F9 0x0042u
#define KC_F10 0x0043u
#define KC_F11 0x0044u
#define KC_F12 0x0045u
#define KC_PSCR 0x0046u
#define KC_SCRL 0x0047u
#define KC_PAUS 0x0048u
#define KC_INS 0x0049u
#define KC_HOME 0x004Au
#define KC_PGUP 0x004Bu
#define KC_DEL 0x004Cu
#define KC_END 0x004Du
#define KC_PGDN 0x004Eu
#define KC_RGHT 0x004Fu
#define KC_LEFT 0x0050u
#define KC_DOWN 0x0051u
#define KC_UP 0x0052u
#define KC_NUM 0x0053u
#define KC_PSLS 0x0054u
#define KC_PAST 0x0055u
#define KC_PMNS 0x0056u
#define KC_PPLS 0x0057u
#define KC_PENT 0x0058u
#define KC_P1 0x0059u
#define KC_P2 0x005Au
#define KC_P3 0x005Bu
#define KC_P4 0x005Cu
#define KC_P5 0x005Du
#define KC_P6 0x005Eu
#define KC_P7 0x005Fu
#define KC_P8 0x0060u
#define KC_P9 0x0061u
#define KC_P0 0x0062u
#define KC_PDOT 0x0063u
#define KC_NUBS 0x0064u
#define KC_APP 0x0065u
#define KC_PEQL 0x0067u
#define KC_F13 0x0068u
#define KC_F14 0x0069u
#define KC_F15 0x006Au
#define KC_F16 0x006Bu
#define KC_F17 0x006Cu
#define KC_F18 0x006Du
#define KC_F19 0x006Eu
#define KC_F20 0x006Fu
#define KC_F21 0x0070u
#define KC_F22 0x0071u
#define KC_F23 0x0072u
#define KC_F24 0x0073u

#define KC_LCTL 0x00E0u
#define KC_LSFT 0x00E1u
#define KC_LALT 0x00E2u
#define KC_LGUI 0x00E3u
#define KC_RCTL 0x00E4u
#define KC_RSFT 0x00E5u
#define KC_RALT 0x00E6u
#define KC_RGUI 0x00E7u

/* Consumer page (0x0C) */
#define KC_BRIU (KB_KC_CONSUMER | 0x006Fu)
#define KC_BRID (KB_KC_CONSUMER | 0x0070u)
#define KC_MNXT (KB_KC_CONSUMER | 0x00B5u)
#define KC_MPRV (KB_KC_CONSUMER | 0x00B6u)
#define KC_MSTP (KB_KC_CONSUMER | 0x00B7u)
#define KC_MPLY (KB_KC_CONSUMER | 0x00CDu)
#define KC_MUTE (KB_KC_CONSUMER | 0x00E2u)
#define KC_VOLU (KB_KC_CONSUMER | 0x00E9u)
#define KC_VOLD (KB_KC_CONSUMER | 0x00EAu)
#define KC_CALC (KB_KC_CONSUMER | 0x0192u)

/* Generic desktop page (0x01), system control */
#define KC_PWR (KB_KC_SYSTEM | 0x81u)
#define KC_SLEP (KB_KC_SYSTEM | 0x82u)
#define KC_WAKE (KB_KC_SYSTEM | 0x83u)

/*
 * Firmware actions. The engine runs Caps Word, Repeat, Leader, Auto Shift,
 * mouse keys and dynamic macros itself and hands the rest (QK_BOOT, QK_RBT,
 * NK_TOGG, connection keys) to the firmware's action callback.
 */
#define QK_BOOT 0x7000u
#define QK_RBT 0x7001u
#define CW_TOGG 0x7002u
#define QK_REP 0x7003u
#define QK_LEAD 0x7004u
#define NK_TOGG 0x7005u /* N-key rollover on or off */
#define AS_TOGG 0x7006u /* Auto Shift on or off */

#define OUT_USB 0x7100u
#define OUT_2G4 0x7101u
#define OUT_BLE 0x7102u
#define BT_CLR 0x7110u
#define KB_BT_PROFILE_MAX 8u
#define BT_SEL(n) ((kb_keycode_t)(0x7120u | ((n) & 0x7u)))

#define MS_UP 0x7200u
#define MS_DOWN 0x7201u
#define MS_LEFT 0x7202u
#define MS_RGHT 0x7203u
#define MS_BTN1 0x7210u
#define MS_BTN2 0x7211u
#define MS_BTN3 0x7212u
#define MS_BTN4 0x7213u
#define MS_BTN5 0x7214u
#define MS_WHLU 0x7220u
#define MS_WHLD 0x7221u
#define MS_WHLL 0x7222u
#define MS_WHLR 0x7223u

/* Dynamic macros: record keys into slot 1 or 2, stop, play back. */
#define DM_REC1 0x7300u
#define DM_REC2 0x7301u
#define DM_RSTP 0x7302u
#define DM_PLY1 0x7303u
#define DM_PLY2 0x7304u

#define M(n) ((kb_keycode_t)(KB_KC_MACRO | ((n) & 0x1Fu)))
#define TD(n) ((kb_keycode_t)(KB_KC_TAP_DANCE | ((n) & 0xFu)))

/* Classification and field access */
enum kb_keycode_kind {
    KB_KIND_INVALID,
    KB_KIND_NO,
    KB_KIND_TRNS,
    KB_KIND_BASIC,
    KB_KIND_CONSUMER,
    KB_KIND_SYSTEM,
    KB_KIND_MODDED,
    KB_KIND_LAYER,
    KB_KIND_OSM,
    KB_KIND_LT,
    KB_KIND_ACTION,
    KB_KIND_MT,
    KB_KIND_MACRO,
    KB_KIND_TAP_DANCE,
};

/*
 * Kind of a keycode, or KB_KIND_INVALID for anything not defined above
 * (including layer keycodes for layers >= layer_count). The protocol uses
 * this to refuse bad keycodes from the host.
 */
enum kb_keycode_kind kb_keycode_kind(kb_keycode_t kc, uint8_t layer_count);

static inline bool kb_keycode_is_valid(kb_keycode_t kc, uint8_t layer_count)
{
    return kb_keycode_kind(kc, layer_count) != KB_KIND_INVALID;
}

/* HID keyboard usage carried by a basic, modded, layer-tap or mod-tap keycode. */
static inline uint8_t kb_keycode_key(kb_keycode_t kc)
{
    return (uint8_t)(kc & 0xFFu);
}

/* 5-bit mods of a modded, one-shot-mod or mod-tap keycode. */
static inline uint8_t kb_keycode_mods(kb_keycode_t kc)
{
    return (uint8_t)((kc >> 8) & 0x1Fu);
}

/* 5-bit mods as an 8-bit HID modifier byte (bit 0 = Left Ctrl). */
static inline uint8_t kb_mods_to_hid(uint8_t mods5)
{
    uint8_t m = mods5 & 0x0Fu;
    return (mods5 & MOD_RIGHT) ? (uint8_t)(m << 4) : m;
}

#endif /* KB_KEYCODES_H */
