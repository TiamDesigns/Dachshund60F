/*
 * Matrix debounce with a selectable algorithm and time.
 *
 * Fed with raw matrix samples and millisecond timestamps (which may wrap),
 * so it runs the same on the board and in host tests. Rows are bitmasks:
 * bit c of row r is the switch at (r, c), 1 = pressed.
 *
 * Algorithms (time = the configured debounce time):
 *   DEFER_GLOBAL      a change is taken once the whole matrix has been
 *                     stable for the debounce time
 *   DEFER_PER_KEY     a change is taken once that key has been stable for
 *                     the debounce time
 *   EAGER_GLOBAL      a change is taken at once, then the whole matrix
 *                     ignores changes for the debounce time
 *   EAGER_PER_KEY     a change is taken at once, then that key ignores
 *                     changes for the debounce time
 *   EAGER_PRESS       presses are eager, releases deferred, per key: the
 *                     lowest press latency, still safe against release
 *                     chatter (the default)
 *
 * Eager modes react to the first contact but trust a single noisy sample;
 * deferred modes filter noise but add the debounce time to every change.
 */
#ifndef KB_DEBOUNCE_H
#define KB_DEBOUNCE_H

#include <stdbool.h>
#include <stdint.h>

#include "kb/matrix.h"

#define KB_DEBOUNCE_MAX_MS 100u

enum kb_debounce_algo {
    KB_DEBOUNCE_DEFER_GLOBAL,
    KB_DEBOUNCE_DEFER_PER_KEY,
    KB_DEBOUNCE_EAGER_GLOBAL,
    KB_DEBOUNCE_EAGER_PER_KEY,
    KB_DEBOUNCE_EAGER_PRESS,
    KB_DEBOUNCE_ALGO_COUNT,
};

struct kb_debounce {
    uint8_t rows;
    uint8_t cols;
    uint8_t algo;
    uint8_t ms;
    bool started;
    uint32_t last_ms;
    uint8_t global_timer;
    kb_matrix_row_t stable[KB_MATRIX_MAX_ROWS];
    kb_matrix_row_t last_raw[KB_MATRIX_MAX_ROWS];
    /* Milliseconds left on each key's (or the global) timer. */
    uint8_t timer[KB_MATRIX_MAX_ROWS][KB_MATRIX_MAX_COLS];
};

/*
 * Set up for a rows x cols matrix with everything released. Returns 0, or
 * -1 if a size, the algorithm or the time is out of range.
 */
int kb_debounce_init(struct kb_debounce *db, uint8_t rows, uint8_t cols,
                     enum kb_debounce_algo algo, uint8_t ms);

/*
 * Change the algorithm or time at run time (a host setting). Keeps the
 * current stable state; pending changes restart. Returns 0, or -1 if out of
 * range, leaving the old settings in place.
 */
int kb_debounce_configure(struct kb_debounce *db, enum kb_debounce_algo algo, uint8_t ms);

/*
 * Feed one raw sample of every row. Returns true if the debounced state
 * changed; read it with kb_debounce_state(). Bits past cols are ignored.
 */
bool kb_debounce_update(struct kb_debounce *db, const kb_matrix_row_t *raw, uint32_t now_ms);

/*
 * Every key reads released and is debounced released: nothing can change
 * until a key goes down.
 */
bool kb_debounce_idle(const struct kb_debounce *db);

static inline const kb_matrix_row_t *kb_debounce_state(const struct kb_debounce *db)
{
    return db->stable;
}

#endif /* KB_DEBOUNCE_H */
