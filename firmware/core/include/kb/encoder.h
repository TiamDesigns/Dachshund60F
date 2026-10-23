/*
 * Quadrature decoder for a mechanical rotary encoder, fed the two contact
 * levels at a fixed rate (the matrix scan). Hardware-independent.
 *
 * Each detent of the PEC12R-4220F-S0024 is one full quadrature cycle, four
 * transitions. Transitions are counted with direction, impossible jumps
 * (both contacts changing at once, from bounce or a missed sample) are
 * ignored, and a detent is reported once steps_per_detent transitions add
 * up the same way. Contact bounce goes back and forth, so it cancels out.
 */
#ifndef KB_ENCODER_H
#define KB_ENCODER_H

#include <stdbool.h>
#include <stdint.h>

struct kb_encoder {
    uint8_t state;           /* last (a << 1) | b */
    int8_t count;            /* transitions since the last detent, signed */
    uint8_t steps_per_detent;
};

/* Start from the contacts' current levels. steps_per_detent: 1, 2 or 4. */
void kb_encoder_init(struct kb_encoder *enc, bool a, bool b, uint8_t steps_per_detent);

/* One sample. Returns +1 for a detent clockwise (A leads B), -1 the other way, else 0. */
int kb_encoder_update(struct kb_encoder *enc, bool a, bool b);

#endif /* KB_ENCODER_H */
