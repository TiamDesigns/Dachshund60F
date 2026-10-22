#include "kb/hold.h"

void kb_hold_init(struct kb_hold *hold, uint32_t hold_ms)
{
    hold->hold_ms = hold_ms;
    hold->pressed_since = 0;
    hold->pressed = false;
    hold->fired = false;
}

bool kb_hold_update(struct kb_hold *hold, bool pressed, uint32_t now_ms)
{
    if (!pressed) {
        hold->pressed = false;
        hold->fired = false;
        return false;
    }

    if (!hold->pressed) {
        hold->pressed = true;
        hold->pressed_since = now_ms;
    }

    /* Unsigned subtraction keeps working across timestamp wrap-around. */
    if (!hold->fired && (uint32_t)(now_ms - hold->pressed_since) >= hold->hold_ms) {
        hold->fired = true;
        return true;
    }

    return false;
}
