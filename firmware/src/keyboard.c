#include "keyboard.h"

#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/reboot.h>

#include "bootloader.h"
#include "default_keymap.h"
#include "encoder_input.h"
#include "kb/debounce.h"
#include "kb/engine.h"
#include "kb/features.h"
#include "kb/hid.h"
#include "kb/hold.h"
#include "kb/indicators.h"
#include "kb/keymap.h"
#include "kb/power.h"
#include "kb/settings.h"
#include "indicators.h"
#include "kb_ble.h"
#include "kb_esb.h"
#include "matrix.h"
#include "output.h"
#include "power.h"
#include "status_screen.h"
#include "storage.h"

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
 * Live keymap and settings in RAM, which the config protocol edits; save
 * writes them to flash (storage.c) and revert reads them back.
 */
#define KEYMAP_LEN (DEFAULT_KEYMAP_LAYERS * ROWS * COLS)
static kb_keycode_t keymap_keys[KEYMAP_LEN];
static struct kb_settings settings;
/* Indicator colours; their brightness is settings.indicator_brightness. */
static struct kb_indicator_config indicator_cfg;
/* Advanced keys (combos, tap dance, macros, encoder mapping...), and the macros the host is editing. */
static struct kb_features features;
static uint8_t macro_edit[KB_MACRO_BUFFER];
/* Encoded snapshot for save and load (one store sector at most); only used
 * under keymap_mutex or before the scan starts.
 */
static uint8_t blob[STORAGE_RECORD_MAX];
/* Held by the scan thread while it runs the engine, and by keymap and settings edits. */
static K_MUTEX_DEFINE(keymap_mutex);
static struct kb_keymap keymap;
static struct kb_engine engine;
static struct kb_hid_reporter reporter;
static struct kb_debounce debounce;
static struct kb_hold recovery_hold;
static kb_matrix_row_t raw[ROWS];
static kb_matrix_row_t stable[ROWS];

K_THREAD_STACK_DEFINE(scan_stack, SCAN_STACK_SIZE);
static struct k_thread scan_thread_data;

/*
 * The scan runs when this is given: by the scan timer every period, by a
 * key interrupt (matrix or encoder) as soon as a key goes down, or by
 * keyboard_poke() when a host changes something the scan passes on.
 */
static K_SEM_DEFINE(scan_sem, 0, 1);
static atomic_t key_woke;

static void scan_tick(struct k_timer *timer)
{
    ARG_UNUSED(timer);
    k_sem_give(&scan_sem);
}
K_TIMER_DEFINE(scan_timer, scan_tick, NULL);

/* From the matrix or encoder interrupt. */
static void on_key_wake(void)
{
    atomic_set(&key_woke, 1);
    k_sem_give(&scan_sem);
}

void keyboard_poke(void)
{
    k_sem_give(&scan_sem);
}

/* Power states (kb/power.h), under keymap_mutex like the settings they follow. */
static struct kb_power power;
static atomic_t host_activity;
static atomic_t sleep_request;

void keyboard_note_activity(void)
{
    atomic_set(&host_activity, 1);
}

static void power_config(struct kb_power_config *cfg)
{
    *cfg = (struct kb_power_config){
        .idle_ms = CONFIG_KB_IDLE_MS,
        .lights_off_ms = CONFIG_KB_LIGHTS_OFF_S * 1000u,
        .radio_off_ms = CONFIG_KB_ADVERTISING_OFF_S * 1000u,
        .sleep_timeout_min = settings.sleep_timeout_min,
    };
}

/*
 * Input timing for get_timing, in system clock cycles (30.5 us on the
 * nRF52840's 32.768 kHz clock: single times are that coarse, averages over
 * many scans finer).
 */
static struct k_spinlock timing_lock;
static struct {
    uint64_t scan_sum;
    uint32_t scan_max;
    uint32_t scans;
    uint64_t report_sum;
    uint32_t report_max;
    uint32_t reports;
    uint32_t key_wakes;
} timing;

static void timing_record(uint32_t scan_cyc, uint32_t report_cyc, bool reported, bool by_key)
{
    K_SPINLOCK(&timing_lock) {
        timing.scans++;
        timing.scan_sum += scan_cyc;
        timing.scan_max = MAX(timing.scan_max, scan_cyc);
        if (reported) {
            timing.reports++;
            timing.report_sum += report_cyc;
            timing.report_max = MAX(timing.report_max, report_cyc);
        }
        if (by_key) {
            timing.key_wakes++;
        }
    }
}

static uint16_t cyc_us(uint64_t cyc)
{
    return (uint16_t)MIN(k_cyc_to_us_near64(cyc), UINT16_MAX);
}

void keyboard_timing(struct kb_proto_get_timing_resp *out, bool reset)
{
    K_SPINLOCK(&timing_lock) {
        out->scan_period_us = CONFIG_KB_MATRIX_SCAN_PERIOD_US;
        out->scan_avg_us = timing.scans ? cyc_us(timing.scan_sum / timing.scans) : 0;
        out->scan_max_us = cyc_us(timing.scan_max);
        out->report_avg_us = timing.reports ? cyc_us(timing.report_sum / timing.reports) : 0;
        out->report_max_us = cyc_us(timing.report_max);
        out->key_wakes = timing.key_wakes;
        out->scans = timing.scans;
        if (reset) {
            memset(&timing, 0, sizeof(timing));
        }
    }
}

/*
 * The engine callbacks below run on the scan thread, or on the config
 * thread when it changes settings or tables; both hold keymap_mutex, so the
 * engine and reporter need no locks of their own.
 */

static void send_report(enum kb_hid_report report, const uint8_t *data, size_t len, void *ctx)
{
    ARG_UNUSED(ctx);
    /* Dropped while no host is ready, or while USB is suspended; the
     * transport then asks for a resync, and every report carries the full
     * state.
     */
    (void)output_send(report, data, len);
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
        /* A live setting: save keeps it. */
        settings.nkro = !settings.nkro;
        kb_hid_reporter_set_nkro(&reporter, settings.nkro, kb_engine_hid(&engine));
        LOG_INF("NKRO %s", settings.nkro ? "on" : "off");
        break;
    case OUT_USB:
        (void)output_set_preferred(KB_OUTPUT_USB);
        break;
    case OUT_BLE:
        (void)output_set_preferred(KB_OUTPUT_BLE);
        break;
    case OUT_2G4:
        /* -EINVAL in a build without the dongle: nothing happens. */
        (void)output_set_preferred(KB_OUTPUT_DONGLE);
        break;
    case RF_PAIR:
        if (IS_ENABLED(CONFIG_KB_DONGLE)) {
            (void)kb_esb_pair(KB_PROTO_PAIR_START);
        }
        break;
    case BT_CLR:
        (void)kb_ble_clear_profile(kb_ble_profile());
        break;
    case QK_SLEEP:
        /* Once every key is up, or the key that sent it would wake the board again. */
        if (!power_usb()) {
            atomic_set(&sleep_request, 1);
        }
        break;
    default:
        if ((kc & ~(kb_keycode_t)0x7u) == BT_SEL(0)) {
            /* -EINVAL for a profile this build doesn't have: nothing happens. */
            (void)kb_ble_select_profile((uint8_t)(kc & 0x7u));
        }
        break;
    }
}

static uint8_t mouse_buttons;

static void on_mouse(const struct kb_mouse_report *m, void *ctx)
{
    ARG_UNUSED(ctx);
    uint8_t report[KB_HID_MOUSE_REPORT_SIZE];
    kb_hid_pack_mouse(m, report);
    mouse_buttons = m->buttons;
    (void)output_send(KB_HID_REPORT_MOUSE, report, sizeof(report));
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
    .mouse = on_mouse,
};

static bool any_down(const kb_matrix_row_t *rows)
{
    for (int r = 0; r < ROWS; r++) {
        if (rows[r]) {
            return true;
        }
    }
    return false;
}

/* Lights and radios, on battery when the board is left alone. */
static void apply_power(const struct kb_power_status *st)
{
    static bool lights_off, radio_off, idle;

    if (st->lights_off != lights_off) {
        lights_off = st->lights_off;
        indicators_set_dark(lights_off);
        status_screen_set_dark(lights_off);
    }
    bool now_idle = st->state != KB_POWER_ACTIVE;
    if (st->radio_off != radio_off || now_idle != idle) {
        if (st->radio_off != radio_off) {
            kb_ble_set_advertising(!st->radio_off);
        }
        radio_off = st->radio_off;
        idle = now_idle;
        if (IS_ENABLED(CONFIG_KB_DONGLE)) {
            kb_esb_set_power(idle, radio_off);
        }
    }
}

static void scan_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    const k_timeout_t period = K_USEC(CONFIG_KB_MATRIX_SCAN_PERIOD_US);
    bool ticking = true;
    k_timeout_t idle_wait = K_FOREVER;

    k_timer_start(&scan_timer, period, period);

    for (;;) {
        k_sem_take(&scan_sem, ticking ? K_FOREVER : idle_wait);
        bool by_key = atomic_set(&key_woke, 0) != 0;
        encoder_input_wake_disarm();
        if (by_key) {
            /* Scan now, and the next one a full period later. */
            k_timer_start(&scan_timer, period, period);
            ticking = true;
        }
        uint32_t now = k_uptime_get_32();

        uint32_t t0 = k_cycle_get_32();
        matrix_scan(raw);
        uint32_t t1 = k_cycle_get_32();

        k_mutex_lock(&keymap_mutex, K_FOREVER);
        bool changed = kb_debounce_update(&debounce, raw, now);
        if (changed) {
            const kb_matrix_row_t *next = kb_debounce_state(&debounce);
            kb_engine_matrix(&engine, stable, next, now);
            memcpy(stable, next, sizeof(stable));
        }
        uint32_t t2 = k_cycle_get_32();
        for (int i = 0; i < encoder_input_count(); i++) {
            int detent = encoder_input_poll(i);
            if (detent) {
                kb_engine_encoder(&engine, (uint8_t)i, detent > 0, now);
            }
        }
        kb_engine_tick(&engine, now);

        /* Follow the output and its host: boot protocol forces 6KRO; on a
         * new output, a resume or a new connection, send everything again.
         */
        bool resync = output_poll();
        bool boot = output_boot_protocol();
        if (boot != reporter.boot_protocol) {
            kb_hid_reporter_set_boot_protocol(&reporter, boot, kb_engine_hid(&engine));
        }
        if (resync) {
            kb_hid_reporter_resync(&reporter, kb_engine_hid(&engine));
            /* Movement is never replayed, only the buttons held. */
            on_mouse(&(struct kb_mouse_report){.buttons = mouse_buttons}, NULL);
        }

        if (atomic_set(&host_activity, 0)) {
            kb_power_activity(&power, now);
        }
        bool down = any_down(raw);
        struct kb_power_inputs in = {
            /* A key interrupt counts as use: an encoder turn is several of them. */
            .quiet = !by_key && !down && kb_debounce_idle(&debounce) && kb_engine_idle(&engine),
            .usb_power = power_usb(),
            .battery_critical = power_battery_critical(),
        };
        struct kb_power_status st = kb_power_update(&power, &in, now);
        k_mutex_unlock(&keymap_mutex);

        timing_record(t1 - t0, t2 - t0, changed, by_key);

        /* Recovery: holding (0, 0) reboots into UF2 whatever the keymap says,
         * so a broken keymap can always be reflashed.
         */
        bool recovery = (stable[RECOVERY_ROW] >> RECOVERY_COL) & 1u;
        if (kb_hold_update(&recovery_hold, recovery, now)) {
            bootloader_reboot_to_uf2();
        }

        apply_power(&st);
        if (!down && (st.state == KB_POWER_OFF || atomic_get(&sleep_request))) {
            k_timer_stop(&scan_timer);
            power_off();
        }

        /* No key down: the next press starts a scan at once, not at the next tick. */
        if (!down && IS_ENABLED(CONFIG_KB_WAKE_ON_PRESS)) {
            matrix_wake_arm();
        }
        if (st.state == KB_POWER_IDLE && !down) {
            if (ticking) {
                k_timer_stop(&scan_timer);
                ticking = false;
            }
            matrix_wake_arm();
            encoder_input_wake_arm();
            idle_wait = st.next_ms == UINT32_MAX ? K_FOREVER : K_MSEC(st.next_ms);
        } else if (!ticking) {
            k_timer_start(&scan_timer, period, period);
            ticking = true;
        }
    }
}

/* Settings and storage. Callers hold keymap_mutex, or the scan hasn't started. */

static void default_settings(struct kb_settings *st)
{
    memset(st, 0, sizeof(*st));
    st->debounce_algo = DEBOUNCE_ALGO;
    st->debounce_ms = CONFIG_KB_DEBOUNCE_MS;
    st->tapping_term_ms = CONFIG_KB_TAPPING_TERM_MS;
    st->nkro = IS_ENABLED(CONFIG_KB_NKRO);
    st->sleep_timeout_min = CONFIG_KB_SLEEP_TIMEOUT_MIN;
    st->indicator_brightness = CONFIG_KB_INDICATOR_BRIGHTNESS;
}

/* Compiled-in keymap and settings. Returns the keycodes replaced, or -1. */
static int load_defaults(void)
{
    default_settings(&settings);
    kb_indicators_default_config(&indicator_cfg);
    default_features(&features);
    return kb_keymap_init(&keymap, keymap_keys, DEFAULT_KEYMAP_LAYERS, ROWS, COLS,
                          &default_keymap[0][0][0]);
}

/* The saved snapshot over the defaults. Returns 0, or -ENOENT and friends. */
static int load_saved(void)
{
    uint16_t schema;
    size_t len;
    int err = storage_load(&schema, blob, sizeof(blob), &len);
    if (err) {
        if (err != -ENOENT) {
            LOG_ERR("reading saved settings failed (%d); using defaults", err);
        }
        return err;
    }

    int loaded = kb_settings_decode(schema, blob, len, &keymap, &settings);
    if (loaded < 0) {
        LOG_ERR("saved settings have schema %u, unknown here; using defaults", schema);
        return -EINVAL;
    }
    int advanced = kb_features_decode(schema, blob, len, &features, keymap.layers, ROWS, COLS);
    int colours = kb_indicators_decode(schema, blob, len, &indicator_cfg);
    LOG_INF("loaded saved%s%s%s%s", (loaded & KB_LOADED_KEYMAP) ? " keymap" : "",
            (loaded & KB_LOADED_SETTINGS) ? " settings" : "", advanced > 0 ? " advanced keys" : "",
            colours > 0 ? " indicator colours" : "");
    return 0;
}

/* Put the settings and colours into effect. Debounce restarts only if it changed (or on first). */
static int apply_settings(bool first)
{
    static uint8_t debounce_algo, debounce_ms;

    if (first || settings.debounce_algo != debounce_algo || settings.debounce_ms != debounce_ms) {
        if (kb_debounce_init(&debounce, ROWS, COLS, settings.debounce_algo,
                             settings.debounce_ms) != 0) {
            return -EINVAL;
        }
        /* Debounce starts from all keys up: let the engine release what it holds. */
        if (!first) {
            kb_engine_matrix(&engine, stable, kb_debounce_state(&debounce), k_uptime_get_32());
            memcpy(stable, kb_debounce_state(&debounce), sizeof(stable));
        }
        debounce_algo = settings.debounce_algo;
        debounce_ms = settings.debounce_ms;
    }
    kb_engine_set_tapping_term(&engine, settings.tapping_term_ms);
    kb_hid_reporter_set_nkro(&reporter, settings.nkro, kb_engine_hid(&engine));
    indicators_set_config(&indicator_cfg, settings.indicator_brightness);
    struct kb_power_config pc;
    power_config(&pc);
    kb_power_configure(&power, &pc);
    return 0;
}

/* New tables (start, revert, factory reset): the engine lets go of the old ones. */
static void apply_features(void)
{
    kb_engine_set_features(&engine, &features);
    memcpy(macro_edit, features.macros, sizeof(macro_edit));
}

int keyboard_start(void)
{
    int err = matrix_init();
    if (err) {
        LOG_ERR("matrix setup failed (%d)", err);
        return err;
    }
    err = encoder_input_init();
    if (err) {
        /* Typing still works without the roller. */
        LOG_ERR("encoder setup failed (%d)", err);
    }

    int replaced = load_defaults();
    if (replaced < 0) {
        return -EINVAL;
    }
    if (replaced > 0) {
        LOG_WRN("%d invalid keycodes in the default keymap", replaced);
    }
    if (storage_init() == 0) {
        (void)load_saved();
    }

    kb_hid_reporter_init(&reporter, settings.nkro, send_report, NULL);
    kb_engine_init(&engine, &keymap, &engine_ops, NULL);
    struct kb_power_config pc;
    power_config(&pc);
    kb_power_init(&power, &pc, k_uptime_get_32());
    matrix_set_wake_handler(on_key_wake);
    encoder_input_set_wake_handler(on_key_wake);
    if (apply_settings(true) != 0) {
        return -EINVAL;
    }
    apply_features();
    kb_hold_init(&recovery_hold, CONFIG_KB_BOOTLOADER_HOLD_MS);

    k_thread_create(&scan_thread_data, scan_stack, K_THREAD_STACK_SIZEOF(scan_stack),
                    scan_thread, NULL, NULL, NULL, SCAN_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(&scan_thread_data, "kb_scan");

    LOG_INF("scanning %dx%d every %d us (idle after %d ms), debounce %d ms, NKRO %s", ROWS, COLS,
            CONFIG_KB_MATRIX_SCAN_PERIOD_US, CONFIG_KB_IDLE_MS, settings.debounce_ms,
            settings.nkro ? "on" : "off");
    return 0;
}

struct kb_keymap *keyboard_keymap(void)
{
    return &keymap;
}

struct kb_features *keyboard_features(void)
{
    return &features;
}

uint8_t *keyboard_macro_edit(void)
{
    return macro_edit;
}

void keyboard_features_changed(void)
{
    kb_engine_set_features(&engine, &features);
}

struct kb_settings *keyboard_settings(void)
{
    return &settings;
}

struct kb_indicator_config *keyboard_indicators(void)
{
    return &indicator_cfg;
}

void keyboard_settings_changed(void)
{
    /* The protocol checked every field, so this can't fail. */
    (void)apply_settings(false);
}

void keyboard_lock(void)
{
    k_mutex_lock(&keymap_mutex, K_FOREVER);
}

void keyboard_unlock(void)
{
    k_mutex_unlock(&keymap_mutex);
}

int keyboard_save(void)
{
    uint16_t schema = KB_SETTINGS_SCHEMA;

    keyboard_lock();
    size_t len = kb_settings_encode(&keymap, &settings, blob, sizeof(blob));
    size_t more = len ? kb_features_encode(&features, keymap.layers, blob + len, sizeof(blob) - len) : 0;
    len = more ? len + more : 0;
    more = len ? kb_indicators_encode(&indicator_cfg, keymap.layers, blob + len, sizeof(blob) - len) : 0;
    len = more ? len + more : 0;
    /* The flash write stalls the CPU anyway (an erase takes up to 85 ms), so
     * keep the lock: nothing can change the blob or the keymap meanwhile.
     */
    int err = len ? storage_save(schema, blob, len) : -ENOSPC;
    keyboard_unlock();
    return err;
}

int keyboard_revert(void)
{
    keyboard_lock();
    (void)load_defaults();
    int err = load_saved();
    (void)apply_settings(false);
    apply_features();
    keyboard_unlock();
    /* Nothing saved yet: the defaults are what's saved. */
    return err == -ENOENT ? 0 : err;
}

int keyboard_factory_reset(void)
{
    keyboard_lock();
    int err = storage_wipe();
    (void)load_defaults();
    (void)apply_settings(false);
    apply_features();
    keyboard_unlock();
    return err;
}
