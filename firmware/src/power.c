#include "power.h"

#include <errno.h>

#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/spinlock.h>
#include <zephyr/sys/poweroff.h>

#include <hal/nrf_power.h>

#include "encoder_input.h"
#include "indicators.h"
#include "kb/battery.h"
#include "kb_ble.h"
#include "matrix.h"
#include "status_screen.h"

LOG_MODULE_REGISTER(power, LOG_LEVEL_INF);

#define USER_NODE DT_PATH(zephyr_user)

/* Reading the cell costs a few microamps; on USB it only tracks the charger. */
#define BATTERY_PERIOD_MS 60000
#define USB_PERIOD_MS 10000
/* VDDH reaches the SAADC divided by 5. */
#define VDDH_DIVIDER 5

#if DT_NODE_HAS_PROP(USER_NODE, io_channels)
#define HAS_GAUGE 1
static const struct adc_dt_spec vddh = ADC_DT_SPEC_GET_BY_IDX(USER_NODE, 0);
#else
#define HAS_GAUGE 0
#endif

#if DT_NODE_HAS_PROP(USER_NODE, charge_status_gpios)
static const struct gpio_dt_spec chg_stat = GPIO_DT_SPEC_GET(USER_NODE, charge_status_gpios);
#define HAS_CHG_STAT 1
#else
#define HAS_CHG_STAT 0
#endif

static struct k_spinlock lock;
static struct kb_battery battery;
static bool gauge_ready;
static bool woke_from_sleep;

bool power_has_battery(void)
{
    return gauge_ready;
}

bool power_usb(void)
{
    return nrf_power_usbregstatus_vbusdet_get(NRF_POWER);
}

static bool charging(void)
{
#if HAS_CHG_STAT
    return gpio_pin_get_dt(&chg_stat) > 0;
#else
    return false;
#endif
}

/* VDDH in mV, or 0 if the reading failed. */
static uint16_t read_vddh(void)
{
#if HAS_GAUGE
    int16_t raw = 0;
    struct adc_sequence seq = {.buffer = &raw, .buffer_size = sizeof(raw)};
    int err = adc_sequence_init_dt(&vddh, &seq);
    err = err ? err : adc_read_dt(&vddh, &seq);
    if (err) {
        LOG_WRN("battery reading failed (%d)", err);
        return 0;
    }
    int32_t mv = raw < 0 ? 0 : raw;
    if (adc_raw_to_millivolts_dt(&vddh, &mv) != 0) {
        return 0;
    }
    return (uint16_t)CLAMP(mv * VDDH_DIVIDER, 0, UINT16_MAX);
#else
    return 0;
#endif
}

static void sample_fn(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(sample_work, sample_fn);

static void sample_fn(struct k_work *work)
{
    ARG_UNUSED(work);

    bool usb = power_usb();
    bool chg = charging();
    /* The cell is only worth reading while it powers the board. */
    uint16_t mv = usb ? 0 : read_vddh();

    bool changed;
    struct kb_battery now;
    K_SPINLOCK(&lock) {
        changed = kb_battery_update(&battery, usb, chg, mv);
        now = battery;
    }

    if (changed) {
        bool known = now.percent != KB_BATTERY_PERCENT_UNKNOWN;
        LOG_INF("battery %s%u%% (%u mV)%s", known ? "" : "unknown ", known ? now.percent : 0,
                now.mv, now.usb_power ? (now.charging ? ", charging" : ", on USB") : "");
        if (known) {
            kb_ble_set_battery(now.percent);
        }
        indicators_set_battery(now.percent);
        status_screen_set_battery(known ? (int8_t)now.percent : -1, now.charging);
    }
    k_work_schedule(&sample_work, K_MSEC(usb ? USB_PERIOD_MS : BATTERY_PERIOD_MS));
}

void power_poke(void)
{
    if (gauge_ready) {
        k_work_reschedule(&sample_work, K_NO_WAIT);
    }
}

bool power_battery_critical(void)
{
    bool critical;
    K_SPINLOCK(&lock) {
        critical = kb_battery_critical(&battery);
    }
    return critical;
}

void power_get(struct kb_proto_get_power_resp *out)
{
    K_SPINLOCK(&lock) {
        out->battery_percent = battery.percent;
        out->battery_mv = battery.mv;
        out->charging = battery.charging ? 1u : 0u;
    }
    out->usb_power = power_usb() ? 1u : 0u;
}

bool power_woke_from_sleep(void)
{
    return woke_from_sleep;
}

int power_init(void)
{
    uint32_t cause = 0;
    if (hwinfo_get_reset_cause(&cause) == 0) {
        woke_from_sleep = (cause & RESET_LOW_POWER_WAKE) != 0;
        (void)hwinfo_clear_reset_cause();
    }

    kb_battery_init(&battery);

#if HAS_CHG_STAT
    if (!gpio_is_ready_dt(&chg_stat) || gpio_pin_configure_dt(&chg_stat, GPIO_INPUT) != 0) {
        LOG_ERR("charge status pin unavailable");
    }
#endif
#if HAS_GAUGE
    if (!adc_is_ready_dt(&vddh) || adc_channel_setup_dt(&vddh) != 0) {
        LOG_ERR("battery gauge unavailable");
        return -ENODEV;
    }
    gauge_ready = true;
    k_work_schedule(&sample_work, K_NO_WAIT);
    return 0;
#else
    return -ENODEV;
#endif
}

FUNC_NORETURN void power_off(void)
{
    LOG_INF("deep sleep; a key wakes the board");

    kb_ble_shutdown();
    struct k_work_sync sync;
    (void)k_work_cancel_delayable_sync(&sample_work, &sync);
    indicators_off();
    status_screen_off();
    encoder_input_wake_disarm();
    LOG_PANIC();

    /*
     * Columns high and the rows' SENSE armed: the GPIO keeps both in System
     * OFF. Interrupts stay locked, so a key already down can't disarm them;
     * its latched DETECT wakes the board straight away instead.
     */
    (void)irq_lock();
    matrix_set_wake_handler(NULL);
    matrix_wake_arm();
    (void)hwinfo_clear_reset_cause();
    sys_poweroff();
}
