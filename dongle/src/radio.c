#include "radio.h"

#include <errno.h>
#include <string.h>

#include <zephyr/drivers/hwinfo.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

#include <esb.h>

#include "kb/hid.h"
#include "kb/link.h"
#include "kb/protocol_gen.h"
#include "kb_usb.h"
#include "led.h"

LOG_MODULE_REGISTER(radio, LOG_LEVEL_INF);

BUILD_ASSERT(CONFIG_ESB_MAX_PAYLOAD_LENGTH >= KB_LINK_PAYLOAD_MAX,
             "ESB payloads must hold a config frame");
BUILD_ASSERT(KB_PROTO_REPORT_SIZE == KB_LINK_CONFIG_SIZE, "config frames fit the link");

#define PIPE_LINK 0
#define PIPE_PAIR 1

/*
 * The keyboard polls at least every 100 ms while it is awake; longer than
 * this without a packet and it is asleep or gone, so its keys are let go.
 */
#define LINK_TIMEOUT_MS 400
#define WATCH_MS 100
#define PAIR_TIMEOUT_MS 30000
/* At most one fresh pairing offer this often (each request takes one). */
#define OFFER_EVERY_MS 200

static K_THREAD_STACK_DEFINE(radio_stack, 1536);
static struct k_work_q radio_q;
/* Every ESB call from a thread holds this; the interrupt only reads packets. */
static K_MUTEX_DEFINE(esb_lock);

static struct kb_link_address own;
static bool link_up;            /* radio_q */
static uint32_t last_rx;        /* radio_q */
static bool pairing;            /* radio_q */
static uint32_t pairing_until;  /* radio_q */
static uint32_t last_offer;     /* radio_q */
static struct kb_link_status sent_status; /* radio_q: what the keyboard was last told */
static bool status_sent;        /* radio_q */

K_MSGQ_DEFINE(rx_queue, sizeof(struct esb_payload), 8, 4);

static void rx_fn(struct k_work *work);
static void status_fn(struct k_work *work);
static void watch_fn(struct k_work *work);
static void pair_fn(struct k_work *work);
static K_WORK_DEFINE(rx_work, rx_fn);
static K_WORK_DEFINE(status_work, status_fn);
static K_WORK_DELAYABLE_DEFINE(watch_work, watch_fn);
static K_WORK_DEFINE(pair_work, pair_fn);

/* ESB's event interrupt: hand received packets to the work queue. */
static void on_esb_event(const struct esb_evt *evt)
{
    if (evt->evt_id != ESB_EVENT_RX_RECEIVED) {
        return;
    }
    struct esb_payload rx;
    while (esb_read_rx_payload(&rx) == 0) {
        if (k_msgq_put(&rx_queue, &rx, K_NO_WAIT) != 0) {
            LOG_WRN("radio packet dropped");
        }
    }
    k_work_submit_to_queue(&radio_q, &rx_work);
}

static int queue_ack(uint8_t pipe, const uint8_t *data, size_t len)
{
    if (len == 0) {
        return -EINVAL;
    }
    struct esb_payload p = {.pipe = pipe, .length = (uint8_t)len};
    memcpy(p.data, data, len);
    k_mutex_lock(&esb_lock, K_FOREVER);
    int err = esb_write_payload(&p);
    k_mutex_unlock(&esb_lock);
    return err;
}

static struct kb_link_status usb_status(void)
{
    struct kb_link_status s = {.locks = kb_usb_locks()};
    if (kb_usb_ready()) {
        s.flags |= KB_LINK_USB_READY;
    }
    if (kb_usb_boot_protocol()) {
        s.flags |= KB_LINK_BOOT_PROTOCOL;
    }
    return s;
}

/* radio_q: tell the keyboard the host's state, if it changed or the host wants everything again. */
static void send_status(bool force_resync)
{
    struct kb_link_status s = usb_status();
    if (kb_usb_take_resync() || force_resync) {
        s.flags |= KB_LINK_RESYNC;
    }
    if (status_sent && s.locks == sent_status.locks && s.flags == sent_status.flags) {
        return;
    }
    uint8_t buf[KB_LINK_PAYLOAD_MAX];
    int err = queue_ack(PIPE_LINK, buf, kb_link_encode_status(&s, buf));
    if (err == 0) {
        sent_status = s;
        /* Only the change is news: a resync isn't asked for twice. */
        sent_status.flags &= (uint8_t)~KB_LINK_RESYNC;
        status_sent = true;
    } else if (s.flags & KB_LINK_RESYNC) {
        LOG_WRN("status not queued (%d)", err);
    }
}

static void status_fn(struct k_work *work)
{
    ARG_UNUSED(work);
    if (link_up) {
        send_status(false);
    }
}

void radio_status_changed(void)
{
    k_work_submit_to_queue(&radio_q, &status_work);
}

/* The last report of each kind sent to USB (radio_q), to know what is still held. */
#define REPORT_KINDS (KB_HID_REPORT_MOUSE + 1)
static uint8_t last_report[REPORT_KINDS][KB_HID_EXTRA_REPORT_MAX];
static bool report_seen[REPORT_KINDS];

BUILD_ASSERT(KB_HID_BOOT_REPORT_SIZE <= KB_HID_EXTRA_REPORT_MAX &&
                 KB_HID_MOUSE_REPORT_SIZE <= KB_HID_EXTRA_REPORT_MAX,
             "every report fits last_report");

static void relay_report(enum kb_hid_report kind, const uint8_t *data, size_t len)
{
    if ((unsigned)kind < REPORT_KINDS && len <= sizeof(last_report[0])) {
        memcpy(last_report[kind], data, len);
        report_seen[kind] = true;
    }
    (void)kb_usb_send(kind, data, len);
}

/*
 * The keyboard went quiet: let go of whatever it left held. Only reports
 * that aren't already released, so a keyboard falling asleep never wakes a
 * sleeping computer.
 */
static void release(enum kb_hid_report report, const uint8_t *data, size_t len, void *ctx)
{
    ARG_UNUSED(ctx);
    if ((unsigned)report >= REPORT_KINDS || !report_seen[report] ||
        memcmp(last_report[report], data, len) == 0) {
        return;
    }
    relay_report(report, data, len);
}

static void set_pipes(bool with_pairing)
{
    k_mutex_lock(&esb_lock, K_FOREVER);
    (void)esb_stop_rx();
    (void)esb_enable_pipes(with_pairing ? BIT(PIPE_LINK) | BIT(PIPE_PAIR) : BIT(PIPE_LINK));
    int err = esb_start_rx();
    k_mutex_unlock(&esb_lock);
    if (err) {
        LOG_ERR("radio restart failed (%d)", err);
    }
}

static void stop_pairing(void)
{
    if (!pairing) {
        return;
    }
    pairing = false;
    set_pipes(false);
    /* Drop a leftover offer; anything else queued is sent again. */
    k_mutex_lock(&esb_lock, K_FOREVER);
    (void)esb_flush_tx();
    k_mutex_unlock(&esb_lock);
    if (link_up) {
        status_sent = false;
        send_status(false);
    }
    led_show(LED_OFF);
    LOG_INF("pairing off");
}

static void offer(uint32_t now)
{
    uint8_t buf[KB_LINK_PAYLOAD_MAX];
    if (queue_ack(PIPE_PAIR, buf, kb_link_encode_pair_offer(&own, buf)) == 0) {
        last_offer = now;
    }
}

static void pair_fn(struct k_work *work)
{
    ARG_UNUSED(work);
    if (pairing) {
        stop_pairing();
        return;
    }
    pairing = true;
    pairing_until = k_uptime_get_32() + PAIR_TIMEOUT_MS;
    set_pipes(true);
    /* Ready before the keyboard asks: an acknowledgement carries only what is already queued. */
    offer(k_uptime_get_32());
    led_show(LED_PAIRING);
    LOG_INF("pairing on for %u s", PAIR_TIMEOUT_MS / 1000);
}

void radio_toggle_pairing(void)
{
    k_work_submit_to_queue(&radio_q, &pair_work);
}

static void on_link_packet(const struct kb_link_packet *pkt, uint32_t now)
{
    last_rx = now;
    if (!link_up) {
        LOG_INF("keyboard in range");
        if (pairing) {
            /* The keyboard took the offer and moved to our address. */
            stop_pairing();
            led_show(LED_PAIRED);
        } else {
            led_show(LED_LINK_UP);
        }
        link_up = true;
        /* It may have missed anything: the host's state, and all its reports again. */
        status_sent = false;
        send_status(true);
    }

    switch (pkt->type) {
    case KB_LINK_REPORT:
        relay_report((enum kb_hid_report)pkt->report.kind, pkt->report.data, pkt->report.len);
        break;
    case KB_LINK_CONFIG:
        if (kb_usb_send_config(pkt->config) != 0) {
            LOG_WRN("config response dropped");
        }
        break;
    default:
        break;
    }
}

static void rx_fn(struct k_work *work)
{
    ARG_UNUSED(work);
    struct esb_payload rx;
    struct kb_link_packet pkt;

    while (k_msgq_get(&rx_queue, &rx, K_NO_WAIT) == 0) {
        uint32_t now = k_uptime_get_32();
        if (kb_link_decode(rx.data, rx.length, &pkt) != 0) {
            continue;
        }
        if (rx.pipe == PIPE_LINK) {
            on_link_packet(&pkt, now);
        } else if (rx.pipe == PIPE_PAIR && pairing && pkt.type == KB_LINK_PAIR &&
                   (uint32_t)(now - last_offer) >= OFFER_EVERY_MS) {
            /* This request took the queued offer; queue the next for a retry. */
            offer(now);
        }
    }
}

static void watch_fn(struct k_work *work)
{
    ARG_UNUSED(work);
    uint32_t now = k_uptime_get_32();

    if (link_up && (uint32_t)(now - last_rx) > LINK_TIMEOUT_MS) {
        link_up = false;
        LOG_INF("keyboard quiet");
        kb_hid_send_released(release, NULL);
        /* Nothing queued for it is current any more. */
        k_mutex_lock(&esb_lock, K_FOREVER);
        (void)esb_flush_tx();
        k_mutex_unlock(&esb_lock);
        if (pairing) {
            offer(now);
        } else {
            led_show(LED_LINK_DOWN);
        }
    }
    if (pairing && (int32_t)(now - pairing_until) >= 0) {
        stop_pairing();
    }
    k_work_schedule_for_queue(&radio_q, &watch_work, K_MSEC(WATCH_MS));
}

/* USB thread: a config request from Doghouse goes to the keyboard, or is refused at once. */
static void on_config_request(const uint8_t *data, size_t len)
{
    uint8_t frame[KB_PROTO_REPORT_SIZE] = {0};
    memcpy(frame, data, MIN(len, sizeof(frame)));
    if (len < KB_PROTO_HEADER_SIZE) {
        return; /* no header to answer */
    }

    uint8_t buf[KB_LINK_PAYLOAD_MAX];
    /* link_up belongs to radio_q; a stale read only costs one refused request. */
    if (link_up && queue_ack(PIPE_LINK, buf, kb_link_encode_config(frame, buf)) == 0) {
        return;
    }
    /* No keyboard to ask: the app waits for it to come back. */
    uint8_t resp[KB_PROTO_REPORT_SIZE] = {0};
    resp[KB_PROTO_OFF_COMMAND] = frame[KB_PROTO_OFF_COMMAND];
    resp[KB_PROTO_OFF_SEQUENCE] = frame[KB_PROTO_OFF_SEQUENCE];
    resp[KB_PROTO_OFF_STATUS] = KB_PROTO_UNREACHABLE;
    (void)kb_usb_send_config(resp);
}

int radio_init(void)
{
    uint8_t id[16];
    ssize_t n = hwinfo_get_device_id(id, sizeof(id));
    if (n <= 0) {
        LOG_ERR("no device ID");
        return -EIO;
    }
    kb_link_address_from_id(id, (size_t)n, &own);

    k_work_queue_start(&radio_q, radio_stack, K_THREAD_STACK_SIZEOF(radio_stack),
                       K_PRIO_COOP(CONFIG_NUM_COOP_PRIORITIES - 2), NULL);
    k_thread_name_set(&radio_q.thread, "radio");

    struct esb_config cfg = ESB_DEFAULT_CONFIG;
    cfg.protocol = ESB_PROTOCOL_ESB_DPL;
    cfg.mode = ESB_MODE_PRX;
    cfg.bitrate = ESB_BITRATE_2MBPS;
    cfg.event_handler = on_esb_event;
    cfg.selective_auto_ack = true;

    const struct kb_link_address *pair = &kb_link_pairing_address;
    const uint8_t prefixes[2] = {own.prefix, pair->prefix};
    k_mutex_lock(&esb_lock, K_FOREVER);
    int err = esb_init(&cfg);
    err = err ? err : esb_set_rf_channel(KB_LINK_CHANNEL);
    err = err ? err : esb_set_base_address_0(own.base);
    err = err ? err : esb_set_base_address_1(pair->base);
    err = err ? err : esb_set_prefixes(prefixes, ARRAY_SIZE(prefixes));
    err = err ? err : esb_enable_pipes(BIT(PIPE_LINK));
    err = err ? err : esb_start_rx();
    k_mutex_unlock(&esb_lock);
    if (err) {
        LOG_ERR("ESB init failed (%d)", err);
        return err;
    }

    kb_usb_set_config_rx(on_config_request);
    k_work_schedule_for_queue(&radio_q, &watch_work, K_MSEC(WATCH_MS));
    LOG_INF("listening on channel %u", KB_LINK_CHANNEL);
    return 0;
}
