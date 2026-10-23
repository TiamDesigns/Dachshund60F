#include "kb/keycodes.h"

static bool basic_usage(uint8_t usage)
{
    return (usage >= KC_A && usage <= 0xA4u) || (usage >= KC_LCTL && usage <= KC_RGUI);
}

static bool action_keycode(kb_keycode_t kc)
{
    switch (kc) {
    case QK_BOOT:
    case QK_RBT:
    case CW_TOGG:
    case QK_REP:
    case QK_LEAD:
    case NK_TOGG:
    case OUT_USB:
    case OUT_2G4:
    case OUT_BLE:
    case BT_CLR:
    case MS_UP:
    case MS_DOWN:
    case MS_LEFT:
    case MS_RGHT:
    case MS_BTN1:
    case MS_BTN2:
    case MS_BTN3:
    case MS_WHLU:
    case MS_WHLD:
        return true;
    default:
        return kc >= BT_SEL(0) && kc < BT_SEL(0) + KB_BT_PROFILE_MAX;
    }
}

enum kb_keycode_kind kb_keycode_kind(kb_keycode_t kc, uint8_t layer_count)
{
    uint8_t key = kb_keycode_key(kc);
    uint8_t mods = kb_keycode_mods(kc);

    if (kc == KC_NO) {
        return KB_KIND_NO;
    }
    if (kc == KC_TRNS) {
        return KB_KIND_TRNS;
    }
    if (kc <= KB_KC_BASIC_MAX) {
        return basic_usage(key) ? KB_KIND_BASIC : KB_KIND_INVALID;
    }
    if (kc >= KB_KC_CONSUMER && kc <= KB_KC_CONSUMER_MAX) {
        return (kc & 0x3FFu) != 0 ? KB_KIND_CONSUMER : KB_KIND_INVALID;
    }
    if (kc >= KB_KC_SYSTEM && kc <= KB_KC_SYSTEM_MAX) {
        return key >= 0x81u && key <= 0xB7u ? KB_KIND_SYSTEM : KB_KIND_INVALID;
    }
    if (kc >= KB_KC_MODDED && kc <= KB_KC_MODDED_MAX) {
        return (mods & 0x0Fu) != 0 && basic_usage(key) ? KB_KIND_MODDED : KB_KIND_INVALID;
    }
    if (kc >= KB_KC_LAYER && kc <= KB_KC_LAYER_MAX) {
        uint8_t op = (uint8_t)((kc >> 8) & 0xFu);
        return op < KB_LAYER_OP_COUNT && key < layer_count ? KB_KIND_LAYER : KB_KIND_INVALID;
    }
    if (kc >= KB_KC_OSM && kc <= KB_KC_OSM_MAX) {
        return (kc & 0x0Fu) != 0 ? KB_KIND_OSM : KB_KIND_INVALID;
    }
    if (kc >= KB_KC_LT && kc <= KB_KC_LT_MAX) {
        uint8_t layer = (uint8_t)((kc >> 8) & 0xFu);
        return layer < layer_count && basic_usage(key) ? KB_KIND_LT : KB_KIND_INVALID;
    }
    if (kc >= KB_KC_ACTION && kc <= KB_KC_ACTION_MAX) {
        return action_keycode(kc) ? KB_KIND_ACTION : KB_KIND_INVALID;
    }
    if (kc >= KB_KC_MT && kc <= KB_KC_MT_MAX) {
        return (mods & 0x0Fu) != 0 && basic_usage(key) ? KB_KIND_MT : KB_KIND_INVALID;
    }
    return KB_KIND_INVALID;
}
