/*
 * Key matrix GPIO scanning (COL2ROW): drive one column high at a time and
 * read the rows, which have pull-downs. Pins come from the board
 * devicetree (zephyr,user matrix-row-gpios / matrix-col-gpios), which is
 * written from boards/<name>/board.json.
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

#endif /* MATRIX_H */
