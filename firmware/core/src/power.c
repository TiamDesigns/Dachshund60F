#include "kb/power.h"

#include <stddef.h>

#define MS_PER_MIN 60000u

void kb_power_init(struct kb_power *p, const struct kb_power_config *cfg, uint32_t now_ms)
{
    p->cfg = *cfg;
    p->last_activity = now_ms;
}

void kb_power_configure(struct kb_power *p, const struct kb_power_config *cfg)
{
    p->cfg = *cfg;
}

void kb_power_activity(struct kb_power *p, uint32_t now_ms)
{
    p->last_activity = now_ms;
}

/* Whether a threshold has passed, and the time left on it (folded into *next). */
static bool reached(uint32_t threshold, uint32_t quiet_for, uint32_t *next)
{
    if (threshold == 0) {
        return false;
    }
    if (quiet_for >= threshold) {
        return true;
    }
    uint32_t left = threshold - quiet_for;
    if (left < *next) {
        *next = left;
    }
    return false;
}

struct kb_power_status kb_power_update(struct kb_power *p, const struct kb_power_inputs *in,
                                       uint32_t now_ms)
{
    struct kb_power_status st = {.state = KB_POWER_ACTIVE, .next_ms = UINT32_MAX};

    if (!in->quiet) {
        p->last_activity = now_ms;
    }
    uint32_t quiet_for = now_ms - p->last_activity;

    if (!in->usb_power && in->battery_critical) {
        st.state = KB_POWER_OFF;
        st.lights_off = true;
        st.radio_off = true;
        st.next_ms = 0;
        return st;
    }

    bool idle = reached(p->cfg.idle_ms, quiet_for, &st.next_ms);
    if (!in->usb_power) {
        st.lights_off = reached(p->cfg.lights_off_ms, quiet_for, &st.next_ms);
        st.radio_off = reached(p->cfg.radio_off_ms, quiet_for, &st.next_ms);
        uint32_t sleep_ms = (uint32_t)p->cfg.sleep_timeout_min * MS_PER_MIN;
        if (reached(sleep_ms, quiet_for, &st.next_ms)) {
            st.state = KB_POWER_OFF;
            st.next_ms = 0;
            return st;
        }
    }
    if (idle) {
        st.state = KB_POWER_IDLE;
    }
    return st;
}
