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
#include "kb/hid.h"
#include "kb/hold.h"
#include "kb/keymap.h"
#include "indicators.h"
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

/*
 * Live keymap in RAM, which the config protocol edits, and the saved copy
 * that revert goes back to. Phase 6 keeps the saved copy in flash.
 */
#define KEYMAP_LEN (DEFAULT_KEYMAP_LAYERS * ROWS * COLS)
static kb_keycode_t keymap_keys[KEYMAP_LEN];
static kb_keycode_t saved_keys[KEYMAP_LEN];
/* Held by the scan thread while it runs the engine, and by keymap edits. */
static K_MUTEX_DEFINE(keymap_mutex);
static struct kb_keymap keymap;
static struct kb_engine engine;
static struct kb_hid_reporter reporter;
static bool nkro = IS_ENABLED(CONFIG_KB_NKRO);
static struct kb_debounce debounce;
static struct kb_hold recovery_hold;
static kb_matrix_row_t raw[ROWS];
static kb_matrix_row_t stable[ROWS];

K_THREAD_STACK_DEFINE(scan_stack, SCAN_STACK_SIZE);
static struct k_thread scan_thread_data;
K_TIMER_DEFINE(scan_timer, NULL, NULL);

/* Everything below runs on the scan thread, so the engine and reporter need no locks. */

static void send_report(enum kb_hid_report report, const uint8_t *data, size_t len, void *ctx)
{
    ARG_UNUSED(ctx);
    /* Dropped while USB isn't configured or is suspended; kb_usb then asks
     * for a resync, and every report carries the full state.
     */
    (void)kb_usb_send(report, data, len);
}

static void on_report(const struct kb_hid_state *hid, void *ctx)
{
    ARG_UNUSED(ctx);
    kb_hid_reporter_update(&reporter, hid);
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
    case NK_TOGG:
        nkro = !nkro;
        kb_hid_reporter_set_nkro(&reporter, nkro, kb_engine_hid(&engine));
        LOG_INF("NKRO %s", nkro ? "on" : "off");
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
    indicators_set_layer(highest);
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
        k_mutex_lock(&keymap_mutex, K_FOREVER);
        if (kb_debounce_update(&debounce, raw, now)) {
            const kb_matrix_row_t *next = kb_debounce_state(&debounce);
            kb_engine_matrix(&engine, stable, next, now);
            memcpy(stable, next, sizeof(stable));
        }
        kb_engine_tick(&engine, now);

        /* Follow the host: boot protocol forces 6KRO; after a resume or a
         * new configuration, send everything again.
         */
        bool boot = kb_usb_boot_protocol();
        if (boot != reporter.boot_protocol) {
            kb_hid_reporter_set_boot_protocol(&reporter, boot, kb_engine_hid(&engine));
        }
        if (kb_usb_take_resync()) {
            kb_hid_reporter_resync(&reporter, kb_engine_hid(&engine));
        }
        k_mutex_unlock(&keymap_mutex);

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
    memcpy(saved_keys, keymap_keys, sizeof(saved_keys));

    if (kb_debounce_init(&debounce, ROWS, COLS, DEBOUNCE_ALGO, CONFIG_KB_DEBOUNCE_MS) != 0) {
        return -EINVAL;
    }

    kb_hid_reporter_init(&reporter, nkro, send_report, NULL);
    kb_engine_init(&engine, &keymap, &engine_ops, NULL);
    kb_engine_set_tapping_term(&engine, CONFIG_KB_TAPPING_TERM_MS);
    kb_hold_init(&recovery_hold, CONFIG_KB_BOOTLOADER_HOLD_MS);

    k_thread_create(&scan_thread_data, scan_stack, K_THREAD_STACK_SIZEOF(scan_stack),
                    scan_thread, NULL, NULL, NULL, SCAN_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(&scan_thread_data, "kb_scan");

    LOG_INF("scanning %dx%d every %d us, debounce %d ms, NKRO %s", ROWS, COLS,
            CONFIG_KB_MATRIX_SCAN_PERIOD_US, CONFIG_KB_DEBOUNCE_MS, nkro ? "on" : "off");
    return 0;
}

struct kb_keymap *keyboard_keymap(void)
{
    return &keymap;
}

void keyboard_lock(void)
{
    k_mutex_lock(&keymap_mutex, K_FOREVER);
}

void keyboard_unlock(void)
{
    k_mutex_unlock(&keymap_mutex);
}

void keyboard_save(void)
{
    keyboard_lock();
    memcpy(saved_keys, keymap_keys, sizeof(saved_keys));
    keyboard_unlock();
}

void keyboard_revert(void)
{
    keyboard_lock();
    memcpy(keymap_keys, saved_keys, sizeof(keymap_keys));
    keyboard_unlock();
}

void keyboard_factory_reset(void)
{
    keyboard_lock();
    (void)kb_keymap_init(&keymap, keymap_keys, DEFAULT_KEYMAP_LAYERS, ROWS, COLS,
                         &default_keymap[0][0][0]);
    memcpy(saved_keys, keymap_keys, sizeof(saved_keys));
    keyboard_unlock();
}
