/*
 * Key matrix GPIO scanning (COL2ROW): drive one column high at a time and
 * read the rows, which have pull-downs. Pins come from the board
 * devicetree (zephyr,user matrix-row-gpios / matrix-col-gpios), which is
 * written from boards/<name>/board.json.
 *
 * A board without matrix-col-gpios is wired directly: each row pin is one
 * key (its devicetree flags say which level is pressed), all in column 0.
 */
#ifndef MATRIX_H
#define MATRIX_H

#include "kb/matrix.h"

int matrix_rows(void);
int matrix_cols(void);

/* Configure the pins: columns as outputs, driven low; rows as inputs. */
int matrix_init(void);

/* Read the whole matrix once into rows[0 .. matrix_rows() - 1]. */
void matrix_scan(kb_matrix_row_t *rows);

/*
 * Wake on a key press: drive every column high and arm a level interrupt
 * on every row, so the first key to go down calls fn (from the interrupt)
 * at once. The interrupt disarms itself; matrix_scan disarms it too. Set
 * fn before arming. Also the deep sleep wake-up: the rows' SENSE stays
 * armed in System OFF.
 */
void matrix_set_wake_handler(void (*fn)(void));
void matrix_wake_arm(void);
void matrix_wake_disarm(void);

#endif /* MATRIX_H */
