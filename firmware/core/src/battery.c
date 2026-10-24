#include "kb/battery.h"

#include <stddef.h>

/* A typical LiPo discharge curve at light load: (mV, percent), falling. */
static const struct {
    uint16_t mv;
    uint8_t percent;
} curve[] = {
    {4200, 100}, {4150, 95}, {4110, 90}, {4080, 85}, {4020, 80}, {3980, 75},
    {3950, 70},  {3910, 65}, {3870, 60}, {3850, 55}, {3840, 50}, {3820, 45},
    {3800, 40},  {3790, 35}, {3770, 30}, {3750, 25}, {3730, 20}, {3710, 15},
    {3690, 10},  {3610, 5},  {3400, 0},
};
#define CURVE_LEN (sizeof(curve) / sizeof(curve[0]))

uint8_t kb_battery_percent_for(uint16_t mv)
{
    if (mv >= curve[0].mv) {
        return 100;
    }
    for (size_t i = 1; i < CURVE_LEN; i++) {
        if (mv >= curve[i].mv) {
            /* Linear between the two points around it. */
            uint32_t span_mv = curve[i - 1].mv - curve[i].mv;
            uint32_t span_pct = curve[i - 1].percent - curve[i].percent;
            uint32_t above = mv - curve[i].mv;
            return (uint8_t)(curve[i].percent + (above * span_pct + span_mv / 2) / span_mv);
        }
    }
    return 0;
}

void kb_battery_init(struct kb_battery *b)
{
    b->mv = 0;
    b->percent = KB_BATTERY_PERCENT_UNKNOWN;
    b->fresh = true;
    b->usb_power = false;
    b->charging = false;
}

bool kb_battery_update(struct kb_battery *b, bool usb_power, bool charging, uint16_t vddh_mv)
{
    uint8_t was_percent = b->percent;
    bool was_usb = b->usb_power;
    bool was_charging = b->charging;

    b->usb_power = usb_power;
    b->charging = usb_power && charging;

    if (usb_power) {
        b->fresh = true;
        /* Charge done: full, if there is a cell we know of. */
        if (!b->charging && b->mv != 0) {
            b->percent = 100;
        }
    } else if (vddh_mv >= KB_BATTERY_MIN_MV && vddh_mv <= KB_BATTERY_MAX_MV) {
        if (b->fresh || b->mv == 0) {
            b->mv = vddh_mv;
            b->percent = kb_battery_percent_for(vddh_mv);
            b->fresh = false;
        } else {
            b->mv = (uint16_t)((3u * b->mv + vddh_mv) / 4u);
            uint8_t pct = kb_battery_percent_for(b->mv);
            if (pct < b->percent) {
                b->percent = pct;
            }
        }
    }

    return b->percent != was_percent || b->usb_power != was_usb || b->charging != was_charging;
}

bool kb_battery_critical(const struct kb_battery *b)
{
    return !b->usb_power && b->mv != 0 && b->mv < KB_BATTERY_CRITICAL_MV;
}
