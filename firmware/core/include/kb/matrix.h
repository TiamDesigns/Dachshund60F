/*
 * Matrix size limits and the row bitmask type shared by debounce and the
 * engine. Bit c of row r is the switch at (r, c), 1 = pressed.
 */
#ifndef KB_MATRIX_H
#define KB_MATRIX_H

#include <stdint.h>

#define KB_MATRIX_MAX_ROWS 8u
#define KB_MATRIX_MAX_COLS 32u

typedef uint32_t kb_matrix_row_t;

#endif /* KB_MATRIX_H */
