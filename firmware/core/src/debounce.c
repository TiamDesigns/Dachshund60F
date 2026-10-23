#include "kb/debounce.h"

#include <string.h>

static uint8_t sat_sub(uint8_t timer, uint32_t elapsed)
{
    return elapsed >= timer ? 0 : (uint8_t)(timer - elapsed);
}

static bool valid_settings(enum kb_debounce_algo algo, uint8_t ms)
{
    return (unsigned)algo < KB_DEBOUNCE_ALGO_COUNT && ms <= KB_DEBOUNCE_MAX_MS;
}

static void restart(struct kb_debounce *db)
{
    memcpy(db->last_raw, db->stable, sizeof(db->last_raw));
    memset(db->timer, 0, sizeof(db->timer));
    db->global_timer = 0;
}

int kb_debounce_init(struct kb_debounce *db, uint8_t rows, uint8_t cols,
                     enum kb_debounce_algo algo, uint8_t ms)
{
    if (rows == 0 || rows > KB_MATRIX_MAX_ROWS || cols == 0 || cols > KB_MATRIX_MAX_COLS ||
        !valid_settings(algo, ms)) {
        return -1;
    }

    memset(db, 0, sizeof(*db));
    db->rows = rows;
    db->cols = cols;
    db->algo = (uint8_t)algo;
    db->ms = ms;
    return 0;
}

int kb_debounce_configure(struct kb_debounce *db, enum kb_debounce_algo algo, uint8_t ms)
{
    if (!valid_settings(algo, ms)) {
        return -1;
    }

    db->algo = (uint8_t)algo;
    db->ms = ms;
    restart(db);
    return 0;
}

static bool update_global(struct kb_debounce *db, const kb_matrix_row_t *raw,
                          kb_matrix_row_t mask, uint32_t elapsed)
{
    bool changed = false;

    if (db->algo == KB_DEBOUNCE_DEFER_GLOBAL) {
        bool moved = false;
        for (uint8_t r = 0; r < db->rows; r++) {
            kb_matrix_row_t now = raw[r] & mask;
            if (now != db->last_raw[r]) {
                db->last_raw[r] = now;
                moved = true;
            }
        }
        db->global_timer = moved ? db->ms : sat_sub(db->global_timer, elapsed);
        if (db->global_timer == 0) {
            for (uint8_t r = 0; r < db->rows; r++) {
                if (db->stable[r] != db->last_raw[r]) {
                    db->stable[r] = db->last_raw[r];
                    changed = true;
                }
            }
        }
        return changed;
    }

    /* EAGER_GLOBAL */
    db->global_timer = sat_sub(db->global_timer, elapsed);
    if (db->global_timer == 0) {
        for (uint8_t r = 0; r < db->rows; r++) {
            kb_matrix_row_t now = raw[r] & mask;
            if (db->stable[r] != now) {
                db->stable[r] = now;
                changed = true;
            }
        }
        if (changed) {
            db->global_timer = db->ms;
        }
    }
    return changed;
}

static bool update_key(struct kb_debounce *db, uint8_t *timer, bool now, bool last, bool stable,
                       uint32_t elapsed)
{
    switch (db->algo) {
    case KB_DEBOUNCE_DEFER_PER_KEY:
        *timer = now != last ? db->ms : sat_sub(*timer, elapsed);
        return *timer == 0 && now != stable;

    case KB_DEBOUNCE_EAGER_PER_KEY:
        *timer = sat_sub(*timer, elapsed);
        if (*timer == 0 && now != stable) {
            *timer = db->ms;
            return true;
        }
        return false;

    default: /* KB_DEBOUNCE_EAGER_PRESS */
        if (!stable) {
            return now;
        }
        /* Pressed: release only after the key reads released for the full time. */
        *timer = (now || last) ? db->ms : sat_sub(*timer, elapsed);
        return !now && *timer == 0;
    }
}

bool kb_debounce_update(struct kb_debounce *db, const kb_matrix_row_t *raw, uint32_t now_ms)
{
    uint32_t elapsed = db->started ? (uint32_t)(now_ms - db->last_ms) : 0;
    kb_matrix_row_t mask =
        db->cols >= 32 ? (kb_matrix_row_t)0xFFFFFFFFu : (((kb_matrix_row_t)1u << db->cols) - 1u);
    bool changed = false;

    db->started = true;
    db->last_ms = now_ms;

    if (db->algo == KB_DEBOUNCE_DEFER_GLOBAL || db->algo == KB_DEBOUNCE_EAGER_GLOBAL) {
        return update_global(db, raw, mask, elapsed);
    }

    for (uint8_t r = 0; r < db->rows; r++) {
        kb_matrix_row_t now_row = raw[r] & mask;
        for (uint8_t c = 0; c < db->cols; c++) {
            kb_matrix_row_t bit = (kb_matrix_row_t)1u << c;
            if (update_key(db, &db->timer[r][c], (now_row & bit) != 0, (db->last_raw[r] & bit) != 0,
                           (db->stable[r] & bit) != 0, elapsed)) {
                db->stable[r] ^= bit;
                changed = true;
            }
        }
        db->last_raw[r] = now_row;
    }
    return changed;
}
