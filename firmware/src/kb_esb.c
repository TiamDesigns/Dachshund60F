#include "kb_esb.h"

#include <errno.h>
#include <string.h>

#include <zephyr/drivers/hwinfo.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/atomic.h>

#include <esb.h>

#include "kb/link.h"
#include "keyboard.h"

LOG_MODULE_REGISTER(kb_esb, LOG_LEVEL_INF);

BUILD_ASSERT(CONFIG_ESB_MAX_PAYLOAD_LENGTH >= KB_LINK_PAYLOAD_MAX,
             "ESB payloads must hold a config frame");

/* Pipe 0 talks to the paired dongle, pipe 1 to dongles in pairing mode. */
#define PIPE_LINK 0
#define PIPE_PAIR 1

/* Polling for what the dongle has: while config traffic flows, in use, and idle. */
#define POLL_CONFIG_MS 2
#define POLL_ACTIVE_MS 10
#define POLL_IDLE_MS 100
#define CONFIG_BURST_MS 1000
#define PAIR_POLL_MS 20
#define PAIR_TIMEOUT_MS 30000
/* Failed packets in a row (each after every retransmit) before the dongle counts as gone. */
#define LINK_LOST_FAILURES 3
/* Retransmits 600 us apart: long enough for an acknowledgement carrying a config frame. */
#define RETRANSMIT_DELAY_US 600
#define RETRANSMIT_COUNT 6

static K_THREAD_STACK_DEFINE(esb_stack, 1024);
static struct k_work_q esb_q;
/* Every ESB call from a thread holds this; the event handler only reads received packets. */
static K_MUTEX_DEFINE(esb_lock);

static bool esb_up;
static atomic_t paired;
static atomic_t link_up;
static atomic_t failures;
static atomic_t resync;
static atomic_t pairing;
static atomic_t status_locks;
static atomic_t status_flags;
static atomic_t power_idle;
static atomic_t radio_off;
static uint32_t pairing_until; /* esb_q only */
static atomic_t config_until;
static atomic_t sent_at;
static kb_usb_config_rx_fn config_rx;

static struct kb_link_address dongle; /* the paired dongle; esb_lock */
static struct kb_link_address offer;  /* esb_q: a pairing offer taken */
static struct k_spinlock offer_lock;
static bool offer_pending;

K_MSGQ_DEFINE(config_rx_queue, KB_LINK_CONFIG_SIZE, 2, 1);

static void poll_fn(struct k_work *work);
static void failed_fn(struct k_work *work);
static void config_fn(struct k_work *work);
static void offer_fn(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(poll_work, poll_fn);
static K_WORK_DEFINE(failed_work, failed_fn);
static K_WORK_DEFINE(config_work, config_fn);
static K_WORK_DEFINE(offer_work, offer_fn);

/* The paired dongle, in Zephyr settings next to the Bluetooth bonds. */

struct saved_dongle {
    uint8_t version;
    uint8_t base[4];
    uint8_t prefix;
};
#define SAVED_DONGLE_VERSION 1u

static int dongle_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg)
{
    struct saved_dongle v;
    if (!settings_name_steq(name, "dongle", NULL)) {
        return -ENOENT;
    }
    if (len == sizeof(v) && read_cb(cb_arg, &v, sizeof(v)) == sizeof(v) &&
        v.version == SAVED_DONGLE_VERSION) {
        struct kb_link_address a = {.prefix = v.prefix};
        memcpy(a.base, v.base, sizeof(a.base));
        if (kb_link_address_valid(&a)) {
            k_mutex_lock(&esb_lock, K_FOREVER);
            dongle = a;
            k_mutex_unlock(&esb_lock);
            atomic_set(&paired, 1);
        }
    }
    return 0;
}
SETTINGS_STATIC_HANDLER_DEFINE(kb_rf, "kb/rf", NULL, dongle_set, NULL, NULL);

static void save_dongle(const struct kb_link_address *a)
{
    int err;
    if (a) {
        struct saved_dongle v = {.version = SAVED_DONGLE_VERSION, .prefix = a->prefix};
        memcpy(v.base, a->base, sizeof(v.base));
        err = settings_save_one("kb/rf/dongle", &v, sizeof(v));
    } else {
        err = settings_delete("kb/rf/dongle");
    }
    if (err) {
        LOG_WRN("saving the dongle failed (%d)", err);
    }
}

/* Radio */

static void link_lost(void)
{
    if (atomic_set(&link_up, 0)) {
        LOG_INF("dongle out of range");
        keyboard_poke();
    }
}

static void on_status(const struct kb_link_status *s)
{
    atomic_val_t was_locks = atomic_set(&status_locks, s->locks);
    atomic_val_t was_flags = atomic_set(&status_flags, s->flags);
    if (s->flags & KB_LINK_RESYNC) {
        atomic_set(&resync, 1);
    }
    if (was_locks != s->locks || was_flags != s->flags || (s->flags & KB_LINK_RESYNC)) {
        keyboard_poke();
    }
}

/* ESB's event interrupt. */
static void on_esb_event(const struct esb_evt *evt)
{
    switch (evt->evt_id) {
    case ESB_EVENT_TX_SUCCESS:
        atomic_set(&failures, 0);
        /* While pairing, an answer may come from a dongle in pairing mode instead. */
        if (atomic_get(&paired) && !atomic_get(&pairing) && !atomic_set(&link_up, 1)) {
            /* Back in range: the dongle's host gets everything again. */
            atomic_set(&resync, 1);
            keyboard_poke();
        }
        break;
    case ESB_EVENT_TX_FAILED:
        k_work_submit_to_queue(&esb_q, &failed_work);
        break;
    case ESB_EVENT_RX_RECEIVED: {
        struct esb_payload rx;
        struct kb_link_packet pkt;
        while (esb_read_rx_payload(&rx) == 0) {
            if (kb_link_decode(rx.data, rx.length, &pkt) != 0) {
                continue;
            }
            if (rx.pipe == PIPE_LINK && pkt.type == KB_LINK_STATUS) {
                on_status(&pkt.status);
            } else if (rx.pipe == PIPE_LINK && pkt.type == KB_LINK_CONFIG) {
                if (k_msgq_put(&config_rx_queue, pkt.config, K_NO_WAIT) == 0) {
                    k_work_submit_to_queue(&esb_q, &config_work);
                }
            } else if (rx.pipe == PIPE_PAIR && pkt.type == KB_LINK_PAIR_OFFER &&
                       atomic_get(&pairing)) {
                K_SPINLOCK(&offer_lock) {
                    offer = pkt.address;
                    offer_pending = true;
                }
                k_work_submit_to_queue(&esb_q, &offer_work);
            }
        }
        break;
    }
    default:
        break;
    }
}

static int write_locked(uint8_t pipe, const uint8_t *data, size_t len, bool noack)
{
    if (!esb_up || len == 0 || len > KB_LINK_PAYLOAD_MAX) {
        return -EAGAIN;
    }
    struct esb_payload p = {.pipe = pipe, .length = (uint8_t)len, .noack = noack};
    memcpy(p.data, data, len);
    int err = esb_write_payload(&p);
    return err == -ENOMEM ? -ENOBUFS : err;
}

static int send_packet(uint8_t pipe, const uint8_t *data, size_t len)
{
    k_mutex_lock(&esb_lock, K_FOREVER);
    int err = write_locked(pipe, data, len, false);
    k_mutex_unlock(&esb_lock);
    return err;
}

/* A packet went unanswered after every retransmit: drop it and carry on with the next. */
static void failed_fn(struct k_work *work)
{
    ARG_UNUSED(work);
    k_mutex_lock(&esb_lock, K_FOREVER);
    (void)esb_pop_tx();
    if (atomic_inc(&failures) + 1 >= LINK_LOST_FAILURES) {
        (void)esb_flush_tx();
        link_lost();
    } else {
        /* It may have been a report. */
        atomic_set(&resync, 1);
        keyboard_poke();
        (void)esb_start_tx();
    }
    k_mutex_unlock(&esb_lock);
}

static int set_link_address_locked(const struct kb_link_address *a)
{
    /* The radio must be idle to change addresses. */
    (void)esb_flush_tx();
    for (int i = 0; i < 50 && !esb_is_idle(); i++) {
        k_sleep(K_MSEC(1));
    }
    int err = esb_set_base_address_0(a->base);
    return err ? err : esb_update_prefix(PIPE_LINK, a->prefix);
}

static void stop_pairing(void)
{
    if (atomic_set(&pairing, 0)) {
        LOG_INF("stopped looking for a dongle");
    }
}

static void offer_fn(struct k_work *work)
{
    ARG_UNUSED(work);
    struct kb_link_address a;
    bool pending;
    K_SPINLOCK(&offer_lock) {
        a = offer;
        pending = offer_pending;
        offer_pending = false;
    }
    if (!pending || !atomic_get(&pairing) || !kb_link_address_valid(&a)) {
        return;
    }
    stop_pairing();

    k_mutex_lock(&esb_lock, K_FOREVER);
    dongle = a;
    int err = set_link_address_locked(&a);
    k_mutex_unlock(&esb_lock);
    if (err) {
        LOG_ERR("switching to the dongle failed (%d)", err);
        return;
    }
    atomic_set(&paired, 1);
    atomic_set(&link_up, 0);
    atomic_set(&failures, 0);
    save_dongle(&a);
    LOG_INF("paired with a dongle");
    k_work_reschedule_for_queue(&esb_q, &poll_work, K_NO_WAIT);
}

static void config_fn(struct k_work *work)
{
    ARG_UNUSED(work);
    uint8_t frame[KB_LINK_CONFIG_SIZE];
    while (k_msgq_get(&config_rx_queue, frame, K_NO_WAIT) == 0) {
        atomic_set(&config_until, (atomic_val_t)(k_uptime_get_32() + CONFIG_BURST_MS));
        kb_usb_config_rx_fn rx = config_rx;
        if (rx) {
            rx(frame, sizeof(frame));
        }
    }
    /* The response comes soon: look for the next request quickly. */
    k_work_reschedule_for_queue(&esb_q, &poll_work, K_MSEC(POLL_CONFIG_MS));
}

static bool before(uint32_t now, atomic_t *until)
{
    return (int32_t)((uint32_t)atomic_get(until) - now) > 0;
}

static void poll_fn(struct k_work *work)
{
    ARG_UNUSED(work);
    uint32_t now = k_uptime_get_32();
    uint8_t buf[KB_LINK_PAYLOAD_MAX];

    if (atomic_get(&pairing)) {
        if ((int32_t)(now - pairing_until) >= 0) {
            stop_pairing();
        } else {
            uint8_t id[KB_LINK_ID_SIZE] = {0};
            (void)hwinfo_get_device_id(id, sizeof(id));
            k_mutex_lock(&esb_lock, K_FOREVER);
            if (!esb_tx_full()) {
                (void)write_locked(PIPE_PAIR, buf, kb_link_encode_pair(id, buf), false);
            }
            k_mutex_unlock(&esb_lock);
            k_work_schedule_for_queue(&esb_q, &poll_work, K_MSEC(PAIR_POLL_MS));
            return;
        }
    }
    if (!atomic_get(&paired) || atomic_get(&radio_off)) {
        return; /* the next report or radio on starts again */
    }

    uint32_t interval = before(now, &config_until) ? POLL_CONFIG_MS
                        : atomic_get(&power_idle)  ? POLL_IDLE_MS
                                                   : POLL_ACTIVE_MS;
    /* A report just sent collected whatever the dongle had. */
    if ((uint32_t)(now - (uint32_t)atomic_get(&sent_at)) >= interval ||
        before(now, &config_until)) {
        k_mutex_lock(&esb_lock, K_FOREVER);
        if (!esb_tx_full()) {
            (void)write_locked(PIPE_LINK, buf, kb_link_encode_poll(buf), false);
        }
        k_mutex_unlock(&esb_lock);
    }
    k_work_schedule_for_queue(&esb_q, &poll_work, K_MSEC(interval));
}

/* API */

bool kb_esb_ready(void)
{
    return atomic_get(&paired) && atomic_get(&link_up) &&
           (atomic_get(&status_flags) & KB_LINK_USB_READY);
}

static void wake_radio(void)
{
    if (atomic_set(&radio_off, 0)) {
        k_work_reschedule_for_queue(&esb_q, &poll_work, K_NO_WAIT);
    }
}

int kb_esb_send(enum kb_hid_report report, const uint8_t *data, size_t len)
{
    uint8_t buf[KB_LINK_PAYLOAD_MAX];
    size_t n = kb_link_encode_report(report, data, len, buf);
    if (n == 0) {
        return -EINVAL;
    }
    if (!atomic_get(&paired) || !atomic_get(&link_up)) {
        return -EAGAIN;
    }
    wake_radio();
    int err = send_packet(PIPE_LINK, buf, n);
    if (err == -ENOBUFS) {
        /* The dongle isn't answering fast enough: start over from the current state. */
        atomic_set(&resync, 1);
        keyboard_poke();
    }
    if (err == 0) {
        atomic_set(&sent_at, (atomic_val_t)k_uptime_get_32());
    }
    return err;
}

bool kb_esb_boot_protocol(void)
{
    return (atomic_get(&status_flags) & KB_LINK_BOOT_PROTOCOL) != 0;
}

bool kb_esb_take_resync(void)
{
    return atomic_set(&resync, 0) != 0;
}

uint8_t kb_esb_locks(void)
{
    return (uint8_t)atomic_get(&status_locks);
}

void kb_esb_set_config_rx(kb_usb_config_rx_fn rx)
{
    config_rx = rx;
}

int kb_esb_send_config(const uint8_t frame[KB_PROTO_REPORT_SIZE])
{
    BUILD_ASSERT(KB_PROTO_REPORT_SIZE == KB_LINK_CONFIG_SIZE, "config frames fit the link");
    uint8_t buf[KB_LINK_PAYLOAD_MAX];
    if (!atomic_get(&paired) || !atomic_get(&link_up)) {
        return -EAGAIN;
    }
    return send_packet(PIPE_LINK, buf, kb_link_encode_config(frame, buf));
}

void kb_esb_get(struct kb_proto_get_dongle_resp *out)
{
    bool is_paired = atomic_get(&paired) != 0;
    out->paired = is_paired ? 1u : 0u;
    out->pairing = atomic_get(&pairing) ? 1u : 0u;
    if (!is_paired || !esb_up || atomic_get(&radio_off)) {
        out->link = 0;
    } else if (!atomic_get(&link_up)) {
        out->link = 1;
    } else {
        out->link = kb_esb_ready() ? 3u : 2u;
    }
}

static void pair_start_fn(struct k_work *work)
{
    ARG_UNUSED(work);
    pairing_until = k_uptime_get_32() + PAIR_TIMEOUT_MS;
    atomic_set(&pairing, 1);
    atomic_set(&radio_off, 0);
    LOG_INF("looking for a dongle in pairing mode");
    k_work_reschedule_for_queue(&esb_q, &poll_work, K_NO_WAIT);
}
static K_WORK_DEFINE(pair_start_work, pair_start_fn);

static void forget_fn(struct k_work *work)
{
    ARG_UNUSED(work);
    stop_pairing();
    if (!atomic_set(&paired, 0)) {
        return;
    }
    link_lost();
    k_mutex_lock(&esb_lock, K_FOREVER);
    (void)esb_flush_tx();
    k_mutex_unlock(&esb_lock);
    save_dongle(NULL);
    LOG_INF("forgot the dongle");
}
static K_WORK_DEFINE(forget_work, forget_fn);

int kb_esb_pair(uint8_t mode)
{
    if (!esb_up) {
        return -EAGAIN;
    }
    switch (mode) {
    case KB_PROTO_PAIR_STOP:
        stop_pairing();
        return 0;
    case KB_PROTO_PAIR_START:
        k_work_submit_to_queue(&esb_q, &pair_start_work);
        return 0;
    case KB_PROTO_PAIR_FORGET:
        k_work_submit_to_queue(&esb_q, &forget_work);
        return 0;
    default:
        return -EINVAL;
    }
}

void kb_esb_set_power(bool idle, bool off)
{
    atomic_set(&power_idle, idle ? 1 : 0);
    if (off) {
        atomic_set(&radio_off, 1);
    } else {
        wake_radio();
    }
}

void kb_esb_shutdown(void)
{
    atomic_set(&radio_off, 1);
    stop_pairing();
    struct k_work_sync sync;
    (void)k_work_cancel_delayable_sync(&poll_work, &sync);
    if (k_mutex_lock(&esb_lock, K_MSEC(100)) == 0) {
        if (esb_up) {
            esb_disable();
            esb_up = false;
        }
        k_mutex_unlock(&esb_lock);
    }
}

int kb_esb_init(void)
{
    k_work_queue_start(&esb_q, esb_stack, K_THREAD_STACK_SIZEOF(esb_stack),
                       K_PRIO_COOP(CONFIG_NUM_COOP_PRIORITIES - 2), NULL);
    k_thread_name_set(&esb_q.thread, "kb_esb");

    struct esb_config cfg = ESB_DEFAULT_CONFIG;
    cfg.protocol = ESB_PROTOCOL_ESB_DPL;
    cfg.mode = ESB_MODE_PTX;
    cfg.bitrate = ESB_BITRATE_2MBPS;
    cfg.event_handler = on_esb_event;
    cfg.retransmit_delay = RETRANSMIT_DELAY_US;
    cfg.retransmit_count = RETRANSMIT_COUNT;
    cfg.selective_auto_ack = true;

    k_mutex_lock(&esb_lock, K_FOREVER);
    /* Unpaired, pipe 0 still needs a usable address; nothing goes out on it. */
    struct kb_link_address link = dongle;
    if (!atomic_get(&paired)) {
        link = (struct kb_link_address){.base = {0x5A, 0x3C, 0x96, 0x69}, .prefix = 0x3C};
    }
    const struct kb_link_address *pair = &kb_link_pairing_address;
    const uint8_t prefixes[2] = {link.prefix, pair->prefix};
    int err = esb_init(&cfg);
    err = err ? err : esb_set_rf_channel(KB_LINK_CHANNEL);
    err = err ? err : esb_set_base_address_0(link.base);
    err = err ? err : esb_set_base_address_1(pair->base);
    err = err ? err : esb_set_prefixes(prefixes, ARRAY_SIZE(prefixes));
    esb_up = err == 0;
    k_mutex_unlock(&esb_lock);
    if (err) {
        LOG_ERR("ESB init failed (%d)", err);
        return err;
    }

    LOG_INF("2.4 GHz radio up, %s", atomic_get(&paired) ? "paired" : "no dongle paired");
    k_work_schedule_for_queue(&esb_q, &poll_work, K_NO_WAIT);
    return 0;
}
