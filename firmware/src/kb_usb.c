/*
 * USB device (device_next stack) with two HID interfaces:
 * - Keyboard: boot subclass, the 8-byte 6KRO report and the lock-LED
 *   output report.
 * - Extra: NKRO keyboard, consumer control and system control, with
 *   report IDs.
 * - Config: the vendor-defined interface for the config protocol
 *   (protocol/protocol.yaml), one 64-byte report each way.
 * Report descriptors and formats are in kb/hid.h; keyboard.c decides what
 * to send.
 *
 * Descriptors: VID/PID and strings from Kconfig (matching board.json), a
 * serial number unique per board from the factory device ID, bcdDevice
 * from firmware/VERSION, and remote wakeup in the configuration.
 *
 * Suspend: reports stop, the indicators go dark, and the next key change
 * asks the host to wake up. On resume every report is sent again.
 *
 * With VBUS detection the device runs only while a cable is in, so
 * unplugging deconfigures it at once and keys move to Bluetooth (output.c).
 */

#include "kb_usb.h"

#include <errno.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/class/usbd_hid.h>

#include <app_version.h>

#include "indicators.h"
#include "kb/protocol_gen.h"
#include "kb/version.h"
#include "keyboard.h"
#include "power.h"

LOG_MODULE_REGISTER(kb_usb, LOG_LEVEL_INF);

USBD_DEVICE_DEFINE(kb_usbd, DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
                   CONFIG_KB_USB_VID, CONFIG_KB_USB_PID);

USBD_DESC_LANG_DEFINE(kb_lang);
USBD_DESC_MANUFACTURER_DEFINE(kb_mfr, CONFIG_KB_USB_MANUFACTURER);
USBD_DESC_PRODUCT_DEFINE(kb_product, CONFIG_KB_USB_PRODUCT);
USBD_DESC_SERIAL_NUMBER_DEFINE(kb_sn);
USBD_DESC_CONFIG_DEFINE(kb_fs_cfg_desc, "Full-speed configuration");

/* bMaxPower is in 2 mA units. */
USBD_CONFIGURATION_DEFINE(kb_fs_config, USB_SCD_REMOTE_WAKEUP, CONFIG_KB_USB_MAX_POWER_MA / 2,
                          &kb_fs_cfg_desc);

BUILD_ASSERT(DT_PROP(DT_NODELABEL(hid_kbd), in_report_size) == KB_HID_BOOT_REPORT_SIZE,
             "hid_kbd in-report-size must match the boot report");
BUILD_ASSERT(DT_PROP(DT_NODELABEL(hid_extra), in_report_size) == KB_HID_EXTRA_REPORT_MAX,
             "hid_extra in-report-size must match the longest extra report");
BUILD_ASSERT(DT_PROP(DT_NODELABEL(hid_config), in_report_size) == KB_PROTO_REPORT_SIZE &&
                 DT_PROP(DT_NODELABEL(hid_config), out_report_size) == KB_PROTO_REPORT_SIZE,
             "hid_config report sizes must match protocol.yaml");

/*
 * One HID interface's outgoing reports. They wait in the queue while one
 * is on the wire, so a quick tap (press and release in the same
 * millisecond) still reaches the host as two reports. The stack sends
 * straight from tx, so it stays untouched until input_report_done.
 */
struct report_msg {
    uint8_t len;
    uint8_t data[KB_PROTO_REPORT_SIZE];
};

struct hid_pipe {
    const struct device *dev;
    struct k_msgq *queue;
    atomic_t ready;
    atomic_t busy;
    uint8_t tx[KB_PROTO_REPORT_SIZE] __aligned(sizeof(void *));
};

#define QUEUE_LEN 16
K_MSGQ_DEFINE(kbd_queue, sizeof(struct report_msg), QUEUE_LEN, 1);
K_MSGQ_DEFINE(extra_queue, sizeof(struct report_msg), QUEUE_LEN, 1);
K_MSGQ_DEFINE(config_queue, sizeof(struct report_msg), 4, 1);

static struct hid_pipe kbd_pipe = {
    .dev = DEVICE_DT_GET(DT_NODELABEL(hid_kbd)),
    .queue = &kbd_queue,
};
static struct hid_pipe extra_pipe = {
    .dev = DEVICE_DT_GET(DT_NODELABEL(hid_extra)),
    .queue = &extra_queue,
};
static struct hid_pipe config_pipe = {
    .dev = DEVICE_DT_GET(DT_NODELABEL(hid_config)),
    .queue = &config_queue,
};
static kb_usb_config_rx_fn config_rx;

static atomic_t suspended;
static atomic_t locks;
static atomic_t wake_requested;
static atomic_t boot_protocol;
static atomic_t resync;
static uint32_t kbd_idle_duration;

/* Last report sent of each kind, for GET_REPORT. */
static struct k_spinlock last_lock;
static uint8_t last_boot[KB_HID_BOOT_REPORT_SIZE];
static uint8_t last_nkro[KB_HID_NKRO_REPORT_SIZE] = {KB_HID_ID_NKRO};
static uint8_t last_consumer[KB_HID_CONSUMER_REPORT_SIZE] = {KB_HID_ID_CONSUMER};
static uint8_t last_system[KB_HID_SYSTEM_REPORT_SIZE] = {KB_HID_ID_SYSTEM};
static uint8_t last_mouse[KB_HID_MOUSE_REPORT_SIZE] = {KB_HID_ID_MOUSE};

/* A failed submit loses that report, maybe a key's release: send the whole
 * state again a little later. Later, so an endpoint that keeps failing isn't
 * retried in a tight loop; a pending retry isn't pushed back.
 */
#define REPORT_RETRY_MS 8

static void retry_handler(struct k_work *work)
{
    ARG_UNUSED(work);
    atomic_set(&resync, 1);
    keyboard_poke();
}
static K_WORK_DELAYABLE_DEFINE(retry_work, retry_handler);

static void pipe_submit_next(struct hid_pipe *p)
{
    struct report_msg msg;

    while (atomic_get(&p->ready) && !atomic_get(&suspended) && atomic_cas(&p->busy, 0, 1)) {
        if (k_msgq_get(p->queue, &msg, K_NO_WAIT) != 0) {
            atomic_set(&p->busy, 0);
            /* A report queued after the get but before the clear would be
             * stranded: look again.
             */
            if (k_msgq_num_used_get(p->queue) == 0) {
                return;
            }
            continue;
        }

        memcpy(p->tx, msg.data, msg.len);
        int err = hid_device_submit_report(p->dev, msg.len, p->tx);
        if (err == 0) {
            return;
        }
        LOG_WRN("report dropped (%d)", err);
        atomic_set(&p->busy, 0);
        if (p != &config_pipe) {
            k_work_schedule(&retry_work, K_MSEC(REPORT_RETRY_MS));
        }
    }
}

static void pipe_reset(struct hid_pipe *p, bool ready)
{
    atomic_set(&p->ready, ready);
    k_msgq_purge(p->queue);
    atomic_set(&p->busy, 0);
    if (p != &config_pipe) {
        atomic_set(&resync, 1);
        keyboard_poke();
    }
}

static struct hid_pipe *pipe_of(const struct device *dev)
{
    if (dev == kbd_pipe.dev) {
        return &kbd_pipe;
    }
    return dev == extra_pipe.dev ? &extra_pipe : &config_pipe;
}

/* Callbacks shared by both interfaces */

static void input_report_done(const struct device *dev, const uint8_t *const report)
{
    ARG_UNUSED(report);
    struct hid_pipe *p = pipe_of(dev);

    atomic_set(&p->busy, 0);
    pipe_submit_next(p);
}

static void iface_ready(const struct device *dev, const bool ready)
{
    struct hid_pipe *p = pipe_of(dev);

    pipe_reset(p, ready);
    if (p == &kbd_pipe) {
        /* A new configuration starts in report protocol. */
        atomic_set(&boot_protocol, 0);
        if (!ready) {
            atomic_set(&locks, 0);
            keyboard_poke();
        }
        LOG_INF("keyboard interface %s", ready ? "ready" : "not ready");
    } else {
        LOG_INF("%s interface %s", p == &extra_pipe ? "extra" : "config",
                ready ? "ready" : "not ready");
    }
}

static int copy_last(const uint8_t *src, size_t size, uint16_t len, uint8_t *buf)
{
    if (len < size) {
        return -ENOTSUP;
    }
    K_SPINLOCK(&last_lock) {
        memcpy(buf, src, size);
    }
    return (int)size;
}

static int get_report(const struct device *dev, const uint8_t type, const uint8_t id,
                      const uint16_t len, uint8_t *const buf)
{
    if (type != HID_REPORT_TYPE_INPUT) {
        return -ENOTSUP;
    }

    if (dev == kbd_pipe.dev) {
        return copy_last(last_boot, sizeof(last_boot), len, buf);
    }

    switch (id) {
    case KB_HID_ID_NKRO:
        return copy_last(last_nkro, sizeof(last_nkro), len, buf);
    case KB_HID_ID_CONSUMER:
        return copy_last(last_consumer, sizeof(last_consumer), len, buf);
    case KB_HID_ID_SYSTEM:
        return copy_last(last_system, sizeof(last_system), len, buf);
    case KB_HID_ID_MOUSE:
        return copy_last(last_mouse, sizeof(last_mouse), len, buf);
    default:
        return -ENOTSUP;
    }
}

/* Keyboard interface only */

/* The indicators and screen follow these while USB is the active output (output.c). */
static void set_locks(uint8_t value)
{
    atomic_set(&locks, value);
    keyboard_poke();
}

static int kbd_set_report(const struct device *dev, const uint8_t type, const uint8_t id,
                          const uint16_t len, const uint8_t *const buf)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(id);

    /* Lock-key LED output report: one byte of KB_HID_LED_* bits. */
    if (type != HID_REPORT_TYPE_OUTPUT || len < 1) {
        return -ENOTSUP;
    }

    set_locks(buf[0]);
    return 0;
}

static void kbd_output_report(const struct device *dev, const uint16_t len,
                              const uint8_t *const buf)
{
    ARG_UNUSED(dev);

    if (len >= 1) {
        set_locks(buf[0]);
    }
}

static void kbd_set_idle(const struct device *dev, const uint8_t id, const uint32_t duration)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(id);
    kbd_idle_duration = duration;
}

static uint32_t kbd_get_idle(const struct device *dev, const uint8_t id)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(id);
    return kbd_idle_duration;
}

static void kbd_set_protocol(const struct device *dev, const uint8_t proto)
{
    ARG_UNUSED(dev);
    atomic_set(&boot_protocol, proto == HID_PROTOCOL_BOOT);
    LOG_INF("host selected %s protocol", proto == HID_PROTOCOL_BOOT ? "boot" : "report");
}

static const struct hid_device_ops kbd_ops = {
    .iface_ready = iface_ready,
    .get_report = get_report,
    .set_report = kbd_set_report,
    .set_idle = kbd_set_idle,
    .get_idle = kbd_get_idle,
    .set_protocol = kbd_set_protocol,
    .output_report = kbd_output_report,
    .input_report_done = input_report_done,
};

static const struct hid_device_ops extra_ops = {
    .iface_ready = iface_ready,
    .get_report = get_report,
    .input_report_done = input_report_done,
};

/* Config interface: requests arrive as output reports. */

static void config_request(const uint8_t *buf, uint16_t len)
{
    kb_usb_config_rx_fn rx = config_rx;
    if (rx) {
        rx(buf, MIN(len, KB_PROTO_REPORT_SIZE));
    }
}

static void config_output_report(const struct device *dev, const uint16_t len,
                                 const uint8_t *const buf)
{
    ARG_UNUSED(dev);
    config_request(buf, len);
}

static int config_set_report(const struct device *dev, const uint8_t type, const uint8_t id,
                             const uint16_t len, const uint8_t *const buf)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(id);

    /* Some hosts send output reports over the control pipe. */
    if (type != HID_REPORT_TYPE_OUTPUT) {
        return -ENOTSUP;
    }
    config_request(buf, len);
    return 0;
}

static int config_get_report(const struct device *dev, const uint8_t type, const uint8_t id,
                             const uint16_t len, uint8_t *const buf)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(type);
    ARG_UNUSED(id);
    ARG_UNUSED(len);
    ARG_UNUSED(buf);
    return -ENOTSUP;
}

static const struct hid_device_ops config_ops = {
    .iface_ready = iface_ready,
    .get_report = config_get_report,
    .set_report = config_set_report,
    .output_report = config_output_report,
    .input_report_done = input_report_done,
};

/* Suspend and resume */

static void on_usbd_msg(struct usbd_context *const ctx, const struct usbd_msg *const msg)
{
    switch (msg->type) {
    case USBD_MSG_SUSPEND:
        atomic_set(&suspended, 1);
        indicators_set_asleep(true);
        LOG_INF("suspended");
        break;
    case USBD_MSG_RESUME:
        atomic_set(&suspended, 0);
        atomic_set(&wake_requested, 0);
        /* The host may have missed changes made while it slept. */
        k_msgq_purge(kbd_pipe.queue);
        k_msgq_purge(extra_pipe.queue);
        atomic_set(&resync, 1);
        keyboard_poke();
        indicators_set_asleep(false);
        LOG_INF("resumed");
        break;
    case USBD_MSG_VBUS_READY:
        /* Power and the battery gauge follow the cable too. */
        power_poke();
        keyboard_poke();
        /* Cable in: start the device (unless it never stopped). */
        if (usbd_can_detect_vbus(ctx)) {
            int err = usbd_enable(ctx);
            if (err && err != -EALREADY) {
                LOG_ERR("USB device enable failed (%d)", err);
            }
        }
        break;
    case USBD_MSG_VBUS_REMOVED:
        power_poke();
        keyboard_poke();
        /* Cable out: deconfigure, so keys move to Bluetooth at once. */
        if (usbd_can_detect_vbus(ctx)) {
            (void)usbd_disable(ctx);
        }
        __fallthrough;
    case USBD_MSG_RESET:
        atomic_set(&wake_requested, 0);
        if (atomic_set(&suspended, 0)) {
            indicators_set_asleep(false);
        }
        break;
    default:
        break;
    }
}

static void wake_host(void)
{
    /* Once per suspend: the first key change wakes the host. */
    if (!usbd_is_suspended(&kb_usbd) || !atomic_cas(&wake_requested, 0, 1)) {
        return;
    }
    /* Fails with -EACCES if the host didn't enable remote wakeup. */
    int err = usbd_wakeup_request(&kb_usbd);
    if (err && err != -EACCES) {
        LOG_WRN("remote wakeup failed (%d)", err);
    }
}

/* Sending */

int kb_usb_send(enum kb_hid_report report, const uint8_t *data, size_t len)
{
    struct hid_pipe *p;
    uint8_t *last;
    size_t size;

    switch (report) {
    case KB_HID_REPORT_BOOT:
        p = &kbd_pipe;
        last = last_boot;
        size = sizeof(last_boot);
        break;
    case KB_HID_REPORT_NKRO:
        p = &extra_pipe;
        last = last_nkro;
        size = sizeof(last_nkro);
        break;
    case KB_HID_REPORT_CONSUMER:
        p = &extra_pipe;
        last = last_consumer;
        size = sizeof(last_consumer);
        break;
    case KB_HID_REPORT_SYSTEM:
        p = &extra_pipe;
        last = last_system;
        size = sizeof(last_system);
        break;
    case KB_HID_REPORT_MOUSE:
        p = &extra_pipe;
        last = last_mouse;
        size = sizeof(last_mouse);
        break;
    default:
        return -EINVAL;
    }
    if (len != size) {
        return -EINVAL;
    }

    K_SPINLOCK(&last_lock) {
        memcpy(last, data, size);
    }

    if (atomic_get(&suspended)) {
        /* Sent again in full after the resume. */
        atomic_set(&resync, 1);
        keyboard_poke();
        wake_host();
        return -EAGAIN;
    }
    if (!atomic_get(&p->ready)) {
        return -EAGAIN;
    }

    struct report_msg msg = {.len = (uint8_t)len};
    memcpy(msg.data, data, len);
    if (k_msgq_put(p->queue, &msg, K_NO_WAIT) != 0) {
        /* The host isn't polling. Start over from the current state, so no
         * release is lost in between.
         */
        k_msgq_purge(p->queue);
        atomic_set(&resync, 1);
        keyboard_poke();
        return -ENOBUFS;
    }

    pipe_submit_next(p);
    return 0;
}

void kb_usb_set_config_rx(kb_usb_config_rx_fn rx)
{
    config_rx = rx;
}

int kb_usb_send_config(const uint8_t frame[KB_PROTO_REPORT_SIZE])
{
    if (!atomic_get(&config_pipe.ready) || atomic_get(&suspended)) {
        return -EAGAIN;
    }

    struct report_msg msg = {.len = KB_PROTO_REPORT_SIZE};
    memcpy(msg.data, frame, KB_PROTO_REPORT_SIZE);
    if (k_msgq_put(config_pipe.queue, &msg, K_NO_WAIT) != 0) {
        /* The host sends one request at a time, so this means it stopped reading. */
        return -ENOBUFS;
    }
    pipe_submit_next(&config_pipe);
    return 0;
}

bool kb_usb_ready(void)
{
    return atomic_get(&kbd_pipe.ready) != 0;
}

uint8_t kb_usb_locks(void)
{
    return (uint8_t)atomic_get(&locks);
}

bool kb_usb_boot_protocol(void)
{
    return atomic_get(&boot_protocol) != 0;
}

bool kb_usb_take_resync(void)
{
    return atomic_set(&resync, 0) != 0;
}

/* Setup */

static int register_hid(struct hid_pipe *p, const uint8_t *desc, size_t len,
                        const struct hid_device_ops *ops)
{
    if (!device_is_ready(p->dev)) {
        LOG_ERR("HID device %s not ready", p->dev->name);
        return -ENODEV;
    }

    int err = hid_device_register(p->dev, desc, len, ops);
    if (err) {
        LOG_ERR("HID register of %s failed (%d)", p->dev->name, err);
    }
    return err;
}

int kb_usb_init(void)
{
    int err;

    err = register_hid(&kbd_pipe, kb_hid_keyboard_desc, kb_hid_keyboard_desc_len, &kbd_ops);
    err = err ? err
              : register_hid(&extra_pipe, kb_hid_extra_desc, kb_hid_extra_desc_len, &extra_ops);
    err = err ? err
              : register_hid(&config_pipe, kb_hid_config_desc, kb_hid_config_desc_len, &config_ops);
    if (err) {
        return err;
    }

    err = usbd_add_descriptor(&kb_usbd, &kb_lang);
    err = err ? err : usbd_add_descriptor(&kb_usbd, &kb_mfr);
    err = err ? err : usbd_add_descriptor(&kb_usbd, &kb_product);
    err = err ? err : usbd_add_descriptor(&kb_usbd, &kb_sn);
    if (err) {
        LOG_ERR("adding string descriptors failed (%d)", err);
        return err;
    }

    err = usbd_add_configuration(&kb_usbd, USBD_SPEED_FS, &kb_fs_config);
    if (err) {
        LOG_ERR("adding configuration failed (%d)", err);
        return err;
    }

    err = usbd_register_all_classes(&kb_usbd, USBD_SPEED_FS, 1, NULL);
    if (err) {
        LOG_ERR("registering classes failed (%d)", err);
        return err;
    }

    /* Class codes come from the interface descriptors. */
    err = usbd_device_set_code_triple(&kb_usbd, USBD_SPEED_FS, 0, 0, 0);
    err = err ? err
              : usbd_device_set_bcd_device(&kb_usbd,
                                           kb_version_bcd_device(APP_VERSION_MAJOR,
                                                                 APP_VERSION_MINOR,
                                                                 APP_PATCHLEVEL));
    if (err) {
        LOG_ERR("setting device descriptor fields failed (%d)", err);
        return err;
    }

    err = usbd_msg_register_cb(&kb_usbd, on_usbd_msg);
    if (err) {
        LOG_ERR("registering the message callback failed (%d)", err);
        return err;
    }

    err = usbd_init(&kb_usbd);
    if (err) {
        LOG_ERR("USB device init failed (%d)", err);
        return err;
    }

    /* With VBUS detection the device starts when a cable goes in (on_usbd_msg). */
    if (!usbd_can_detect_vbus(&kb_usbd)) {
        err = usbd_enable(&kb_usbd);
        if (err) {
            LOG_ERR("USB device enable failed (%d)", err);
            return err;
        }
    }

    return 0;
}
