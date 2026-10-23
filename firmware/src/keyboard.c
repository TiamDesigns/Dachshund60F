#include "keyboard.h"

#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/sys/reboot.h>

#include "bootloader.h"
#include "default_keymap.h"
#include "kb/debounce.h"
#include "kb/engine.h"
#include "kb/hold.h"
#include "kb/keymap.h"
#include "kb_usb.h"
#include "matrix.h"
#include "status_screen.h"

LOG_MODULE_REGISTER(keyboard, LOG_LEVEL_INF);

/* Never log key events here: debug logs must not leak what is typed. */

#define ROWS DEFAULT_KEYMAP_ROWS
#define COLS DEFAULT_KEYMAP_COLS

BUILD_ASSERT(DT_PROP_LEN(DT_PATH(zephyr_user), matrix_row_gpios) == ROWS,
             "default keymap rows must match the devicetree matrix");
BUILD_ASSERT(DT_PROP_LEN(DT_PATH(zephyr_user), matrix_col_gpios) == COLS,
             "default keymap columns must match the devicetree matrix");

#if defined(CONFIG_KB_DEBOUNCE_DEFER_GLOBAL)
#define DEBOUNCE_ALGO KB_DEBOUNCE_DEFER_GLOBAL
#elif defined(CONFIG_KB_DEBOUNCE_DEFER_PER_KEY)
#define DEBOUNCE_ALGO KB_DEBOUNCE_DEFER_PER_KEY
#elif defined(CONFIG_KB_DEBOUNCE_EAGER_GLOBAL)
#define DEBOUNCE_ALGO KB_DEBOUNCE_EAGER_GLOBAL
#elif defined(CONFIG_KB_DEBOUNCE_EAGER_PER_KEY)
#define DEBOUNCE_ALGO KB_DEBOUNCE_EAGER_PER_KEY
#else
#define DEBOUNCE_ALGO KB_DEBOUNCE_EAGER_PRESS
#endif

/* The recovery key: (0, 0), Esc on the Dachshund60F. */
#define RECOVERY_ROW 0
#define RECOVERY_COL 0

#define SCAN_STACK_SIZE 2048
#define SCAN_PRIORITY K_PRIO_PREEMPT(2)

/* Live keymap in RAM; settings storage (phase 6) will load and save it. */
static kb_keycode_t keymap_keys[DEFAULT_KEYMAP_LAYERS * ROWS * COLS];
static struct kb_keymap keymap;
static struct kb_engine engine;
static struct kb_debounce debounce;
static struct kb_hold recovery_hold;
static kb_matrix_row_t raw[ROWS];
static kb_matrix_row_t stable[ROWS];

K_THREAD_STACK_DEFINE(scan_stack, SCAN_STACK_SIZE);
static struct k_thread scan_thread_data;
K_TIMER_DEFINE(scan_timer, NULL, NULL);

static void on_report(const struct kb_hid_state *hid, void *ctx)
{
    ARG_UNUSED(ctx);

    uint8_t report[KB_USB_KBD_REPORT_SIZE];
    kb_hid_boot_report(hid, report);
    /* Dropped while USB isn't configured; reports carry full state. */
    (void)kb_usb_send_keyboard(report);
}

static void on_action(kb_keycode_t kc, bool pressed, void *ctx)
{
    ARG_UNUSED(ctx);

    if (!pressed) {
        return;
    }

    switch (kc) {
    case QK_BOOT:
        bootloader_reboot_to_uf2();
        break;
    case QK_RBT:
        LOG_INF("rebooting");
        LOG_PANIC();
        sys_reboot(SYS_REBOOT_COLD);
        break;
    default:
        /* Connection, mouse and the other actions arrive in later phases. */
        break;
    }
}

static void on_layers(uint32_t layer_mask, uint8_t highest, void *ctx)
{
    ARG_UNUSED(layer_mask);
    ARG_UNUSED(ctx);
    status_screen_set_layer(highest);
}

static const struct kb_engine_ops engine_ops = {
    .report = on_report,
    .action = on_action,
    .layers = on_layers,
};

static void scan_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    k_timer_start(&scan_timer, K_USEC(CONFIG_KB_MATRIX_SCAN_PERIOD_US),
                  K_USEC(CONFIG_KB_MATRIX_SCAN_PERIOD_US));

    for (;;) {
        k_timer_status_sync(&scan_timer);
        uint32_t now = k_uptime_get_32();

        matrix_scan(raw);
        if (kb_debounce_update(&debounce, raw, now)) {
            const kb_matrix_row_t *next = kb_debounce_state(&debounce);
            kb_engine_matrix(&engine, stable, next, now);
            memcpy(stable, next, sizeof(stable));
        }
        kb_engine_tick(&engine, now);

        /* Recovery: holding (0, 0) reboots into UF2 whatever the keymap says,
         * so a broken keymap can always be reflashed.
         */
        bool recovery = (stable[RECOVERY_ROW] >> RECOVERY_COL) & 1u;
        if (kb_hold_update(&recovery_hold, recovery, now)) {
            bootloader_reboot_to_uf2();
        }
    }
}

int keyboard_start(void)
{
    int err = matrix_init();
    if (err) {
        LOG_ERR("matrix setup failed (%d)", err);
        return err;
    }

    int replaced = kb_keymap_init(&keymap, keymap_keys, DEFAULT_KEYMAP_LAYERS, ROWS, COLS,
                                  &default_keymap[0][0][0]);
    if (replaced < 0) {
        return -EINVAL;
    }
    if (replaced > 0) {
        LOG_WRN("%d invalid keycodes in the default keymap", replaced);
    }

    if (kb_debounce_init(&debounce, ROWS, COLS, DEBOUNCE_ALGO, CONFIG_KB_DEBOUNCE_MS) != 0) {
        return -EINVAL;
    }

    kb_engine_init(&engine, &keymap, &engine_ops, NULL);
    kb_engine_set_tapping_term(&engine, CONFIG_KB_TAPPING_TERM_MS);
    kb_hold_init(&recovery_hold, CONFIG_KB_BOOTLOADER_HOLD_MS);

    k_thread_create(&scan_thread_data, scan_stack, K_THREAD_STACK_SIZEOF(scan_stack),
                    scan_thread, NULL, NULL, NULL, SCAN_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(&scan_thread_data, "kb_scan");

    LOG_INF("scanning %dx%d every %d us, debounce %d ms", ROWS, COLS,
            CONFIG_KB_MATRIX_SCAN_PERIOD_US, CONFIG_KB_DEBOUNCE_MS);
    return 0;
}
