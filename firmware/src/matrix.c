#include "matrix.h"

#include <errno.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/spinlock.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(matrix, LOG_LEVEL_INF);

#define USER_NODE DT_PATH(zephyr_user)

#define GPIO_SPEC(node, prop, idx) GPIO_DT_SPEC_GET_BY_IDX(node, prop, idx),

static const struct gpio_dt_spec rows[] = {
    DT_FOREACH_PROP_ELEM(USER_NODE, matrix_row_gpios, GPIO_SPEC)};

#if DT_NODE_HAS_PROP(USER_NODE, matrix_col_gpios)
static const struct gpio_dt_spec cols[] = {
    DT_FOREACH_PROP_ELEM(USER_NODE, matrix_col_gpios, GPIO_SPEC)};
#define NUM_COLS ARRAY_SIZE(cols)
#define MATRIX_COLS NUM_COLS
#else
/* Direct wiring, no columns: each row pin is one key, read as column 0. */
static const struct gpio_dt_spec *const cols;
#define NUM_COLS 0
#define MATRIX_COLS 1
#endif

BUILD_ASSERT(ARRAY_SIZE(rows) <= KB_MATRIX_MAX_ROWS, "too many matrix rows");
BUILD_ASSERT(MATRIX_COLS <= KB_MATRIX_MAX_COLS, "too many matrix columns");

int matrix_rows(void)
{
    return ARRAY_SIZE(rows);
}

int matrix_cols(void)
{
    return MATRIX_COLS;
}

static struct gpio_callback row_cb[ARRAY_SIZE(rows)];
static void (*wake_fn)(void);
/* Arming and disarming as a whole, so a row firing midway can't interleave. */
static struct k_spinlock wake_lock;
static bool armed;

void matrix_wake_disarm(void)
{
    K_SPINLOCK(&wake_lock) {
        if (!armed) {
            K_SPINLOCK_BREAK;
        }
        armed = false;
        for (size_t r = 0; r < ARRAY_SIZE(rows); r++) {
            (void)gpio_pin_interrupt_configure_dt(&rows[r], GPIO_INT_DISABLE);
        }
        for (size_t c = 0; c < NUM_COLS; c++) {
            gpio_pin_set_dt(&cols[c], 0);
        }
    }
}

static void row_isr(const struct device *port, struct gpio_callback *cb, gpio_port_pins_t pins)
{
    ARG_UNUSED(port);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);
    /* A level interrupt fires for as long as the key is down: off first. */
    matrix_wake_disarm();
    if (wake_fn) {
        wake_fn();
    }
}

void matrix_set_wake_handler(void (*fn)(void))
{
    wake_fn = fn;
}

void matrix_wake_arm(void)
{
    K_SPINLOCK(&wake_lock) {
        if (armed) {
            K_SPINLOCK_BREAK;
        }
        armed = true;
        for (size_t c = 0; c < NUM_COLS; c++) {
            gpio_pin_set_dt(&cols[c], 1);
        }
        k_busy_wait(CONFIG_KB_MATRIX_SETTLE_US);
        /* A key already down fires as soon as the lock is let go. */
        for (size_t r = 0; r < ARRAY_SIZE(rows); r++) {
            (void)gpio_pin_interrupt_configure_dt(&rows[r], GPIO_INT_LEVEL_ACTIVE);
        }
    }
}

int matrix_init(void)
{
    for (size_t i = 0; i < NUM_COLS; i++) {
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
        gpio_init_callback(&row_cb[i], row_isr, BIT(rows[i].pin));
        err = gpio_add_callback_dt(&rows[i], &row_cb[i]);
        if (err) {
            LOG_ERR("row %zu interrupt setup failed (%d)", i, err);
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
    matrix_wake_disarm();
    for (size_t r = 0; r < ARRAY_SIZE(rows); r++) {
        out[r] = 0;
    }

    if (NUM_COLS == 0) {
        for (size_t r = 0; r < ARRAY_SIZE(rows); r++) {
            out[r] = gpio_pin_get_dt(&rows[r]) > 0;
        }
        return;
    }

    for (size_t c = 0; c < NUM_COLS; c++) {
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
