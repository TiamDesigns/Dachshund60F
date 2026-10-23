#include "matrix.h"

#include <errno.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(matrix, LOG_LEVEL_INF);

#define USER_NODE DT_PATH(zephyr_user)

#define GPIO_SPEC(node, prop, idx) GPIO_DT_SPEC_GET_BY_IDX(node, prop, idx),

static const struct gpio_dt_spec rows[] = {
    DT_FOREACH_PROP_ELEM(USER_NODE, matrix_row_gpios, GPIO_SPEC)};
static const struct gpio_dt_spec cols[] = {
    DT_FOREACH_PROP_ELEM(USER_NODE, matrix_col_gpios, GPIO_SPEC)};

BUILD_ASSERT(ARRAY_SIZE(rows) <= KB_MATRIX_MAX_ROWS, "too many matrix rows");
BUILD_ASSERT(ARRAY_SIZE(cols) <= KB_MATRIX_MAX_COLS, "too many matrix columns");

int matrix_rows(void)
{
    return ARRAY_SIZE(rows);
}

int matrix_cols(void)
{
    return ARRAY_SIZE(cols);
}

int matrix_init(void)
{
    for (size_t i = 0; i < ARRAY_SIZE(cols); i++) {
        if (!gpio_is_ready_dt(&cols[i])) {
            return -ENODEV;
        }
        int err = gpio_pin_configure_dt(&cols[i], GPIO_OUTPUT_INACTIVE);
        if (err) {
            LOG_ERR("column %zu setup failed (%d)", i, err);
            return err;
        }
    }

    for (size_t i = 0; i < ARRAY_SIZE(rows); i++) {
        if (!gpio_is_ready_dt(&rows[i])) {
            return -ENODEV;
        }
        int err = gpio_pin_configure_dt(&rows[i], GPIO_INPUT);
        if (err) {
            LOG_ERR("row %zu setup failed (%d)", i, err);
            return err;
        }
    }

    return 0;
}

/* Wait (bounded) for every row to fall back low after a column is released. */
static void wait_rows_low(void)
{
    for (int us = 0; us < CONFIG_KB_MATRIX_RELEASE_US; us++) {
        bool any = false;
        for (size_t r = 0; r < ARRAY_SIZE(rows); r++) {
            any |= gpio_pin_get_dt(&rows[r]) > 0;
        }
        if (!any) {
            return;
        }
        k_busy_wait(1);
    }
}

void matrix_scan(kb_matrix_row_t *out)
{
    for (size_t r = 0; r < ARRAY_SIZE(rows); r++) {
        out[r] = 0;
    }

    for (size_t c = 0; c < ARRAY_SIZE(cols); c++) {
        gpio_pin_set_dt(&cols[c], 1);
        k_busy_wait(CONFIG_KB_MATRIX_SETTLE_US);

        for (size_t r = 0; r < ARRAY_SIZE(rows); r++) {
            if (gpio_pin_get_dt(&rows[r]) > 0) {
                out[r] |= (kb_matrix_row_t)1u << c;
            }
        }

        gpio_pin_set_dt(&cols[c], 0);
        /* A pressed key leaves its row high briefly; let it drain so the
         * next column doesn't see a ghost.
         */
        wait_rows_low();
    }
}
