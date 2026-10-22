#include "bootloader.h"

#include <errno.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

#include <soc.h>

#include "kb/hold.h"

LOG_MODULE_REGISTER(bootloader, LOG_LEVEL_INF);

/* GPREGRET value the Adafruit bootloader checks to stay in UF2 mode. */
#define ADAFRUIT_DFU_MAGIC_UF2_RESET 0x57

#define POLL_MS 10

/* Esc is matrix (row 0, col 0). COL2ROW: drive the column, read the row. */
static const struct gpio_dt_spec esc_col =
    GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), matrix_col_gpios, 0);
static const struct gpio_dt_spec esc_row =
    GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), matrix_row_gpios, 0);

static struct kb_hold esc_hold;

static void key_poll(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(key_work, key_poll);

void bootloader_reboot_to_uf2(void)
{
    LOG_INF("rebooting into UF2 bootloader");
    LOG_PANIC();

    NRF_POWER->GPREGRET = ADAFRUIT_DFU_MAGIC_UF2_RESET;
    NVIC_SystemReset();
}

static void key_poll(struct k_work *work)
{
    ARG_UNUSED(work);

    gpio_pin_set_dt(&esc_col, 1);
    k_busy_wait(5); /* let the row settle through the diode */
    bool pressed = gpio_pin_get_dt(&esc_row) == 1;
    gpio_pin_set_dt(&esc_col, 0);

    if (kb_hold_update(&esc_hold, pressed, k_uptime_get_32())) {
        bootloader_reboot_to_uf2();
    }

    k_work_schedule(&key_work, K_MSEC(POLL_MS));
}

int bootloader_key_start(void)
{
    if (!gpio_is_ready_dt(&esc_col) || !gpio_is_ready_dt(&esc_row)) {
        return -ENODEV;
    }

    int err = gpio_pin_configure_dt(&esc_col, GPIO_OUTPUT_INACTIVE);
    err = err ? err : gpio_pin_configure_dt(&esc_row, GPIO_INPUT);
    if (err) {
        LOG_ERR("bootloader key GPIO setup failed (%d)", err);
        return err;
    }

    kb_hold_init(&esc_hold, CONFIG_KB_BOOTLOADER_HOLD_MS);
    k_work_schedule(&key_work, K_NO_WAIT);
    return 0;
}
