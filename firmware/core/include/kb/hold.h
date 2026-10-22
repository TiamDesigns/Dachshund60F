/*
 * Long-press detector: reports once when an input has been held
 * continuously for a set time. Fed with samples and millisecond timestamps,
 * so it runs the same on the board and in host tests. Timestamps may wrap.
 */
#ifndef KB_HOLD_H
#define KB_HOLD_H

#include <stdbool.h>
#include <stdint.h>

struct kb_hold {
    uint32_t hold_ms;
    uint32_t pressed_since;
    bool pressed;
    bool fired;
};

void kb_hold_init(struct kb_hold *hold, uint32_t hold_ms);

/*
 * Feed one sample. Returns true exactly once per press, on the first sample
 * at least hold_ms after the press began. Releasing re-arms the detector.
 */
bool kb_hold_update(struct kb_hold *hold, bool pressed, uint32_t now_ms);

#endif /* KB_HOLD_H */
