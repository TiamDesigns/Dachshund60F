/*
 * Bluetooth LE peripheral: HID over GATT (Nordic's HIDS library), the
 * Battery and Device Information services, and host profiles.
 *
 * Profiles: profile n advertises with identity n, its own random static
 * address, so each paired computer sees its own "Dachshund60F" and keeps
 * its own bond. A profile with a bond only takes that host: a stranger that
 * connects is dropped, and pairing from anyone else is refused, until
 * BT_CLR (or the configurator) forgets the host. The selected profile is
 * kept in Zephyr settings, next to the bonds.
 *
 * Reports: one report map (kb_hid_ble_desc) with every USB report, the
 * boot keyboard report and lock LEDs taking report ID 5 and the config
 * protocol report ID 6. Sending goes through a queue and its own thread,
 * because notifications can wait for buffers and the HIDS library isn't
 * thread-safe.
 */

#include "kb_ble.h"

#include <errno.h>
#include <string.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/byteorder.h>

#include <bluetooth/services/hids.h>

#include "kb/keycodes.h"
#include "kb/output.h"

LOG_MODULE_REGISTER(kb_ble, LOG_LEVEL_INF);

#define PROFILES CONFIG_BT_ID_MAX
BUILD_ASSERT(PROFILES <= KB_BT_PROFILE_MAX, "BT_SEL reaches 8 profiles at most");
BUILD_ASSERT(PROFILES <= 8, "get_connection's bonded mask is 8 bits");

/* Report indices in the HIDS input and output groups. */
enum {
    IN_KEYBOARD,
    IN_NKRO,
    IN_CONSUMER,
    IN_SYSTEM,
    IN_MOUSE,
    IN_CONFIG,
    IN_COUNT,
};
enum {
    OUT_LEDS,
    OUT_CONFIG,
    OUT_COUNT,
};
BUILD_ASSERT(IN_COUNT == CONFIG_BT_HIDS_INPUT_REP_MAX, "CONFIG_BT_HIDS_INPUT_REP_MAX");
BUILD_ASSERT(OUT_COUNT == CONFIG_BT_HIDS_OUTPUT_REP_MAX, "CONFIG_BT_HIDS_OUTPUT_REP_MAX");

/* Report payloads over GATT: the USB reports without their report ID byte. */
#define LEN_NKRO (KB_HID_NKRO_REPORT_SIZE - 1u)
#define LEN_CONSUMER (KB_HID_CONSUMER_REPORT_SIZE - 1u)
#define LEN_SYSTEM (KB_HID_SYSTEM_REPORT_SIZE - 1u)
#define LEN_MOUSE (KB_HID_MOUSE_REPORT_SIZE - 1u)

BT_HIDS_DEF(hids, KB_HID_BOOT_REPORT_SIZE, LEN_NKRO, LEN_CONSUMER, LEN_SYSTEM, LEN_MOUSE,
            KB_PROTO_REPORT_SIZE, 1, KB_PROTO_REPORT_SIZE);

static uint8_t rep_map[KB_HID_BLE_DESC_MAX];

/* The connected host (at most one), under conn_lock. */
static struct k_spinlock conn_lock;
static struct bt_conn *current;

static atomic_t up;
static atomic_t state = ATOMIC_INIT(KB_BLE_OFF);
static atomic_t profile;
static atomic_t bonded;
static atomic_t boot_protocol;
static atomic_t resync;
static atomic_t locks;
static kb_usb_config_rx_fn config_rx;

/* Sending */

#define TX_CONFIG 0xFFu

struct tx_msg {
    uint8_t report; /* enum kb_hid_report, or TX_CONFIG */
    uint8_t len;
    uint8_t data[KB_PROTO_REPORT_SIZE];
};

#define TX_STACK_SIZE 1536
#define TX_PRIORITY K_PRIO_PREEMPT(3)
K_MSGQ_DEFINE(tx_queue, sizeof(struct tx_msg), 16, 4);
K_THREAD_STACK_DEFINE(tx_stack, TX_STACK_SIZE);
static struct k_thread tx_thread_data;

static struct bt_conn *conn_get(void)
{
    struct bt_conn *c = NULL;
    K_SPINLOCK(&conn_lock) {
        if (current) {
            c = bt_conn_ref(current);
        }
    }
    return c;
}

static int tx_one(struct bt_conn *c, const struct tx_msg *m)
{
    bool boot = atomic_get(&boot_protocol);

    switch (m->report) {
    case TX_CONFIG:
        return bt_hids_inp_rep_send(&hids, c, IN_CONFIG, m->data, m->len, NULL);
    case KB_HID_REPORT_BOOT:
        return boot ? bt_hids_boot_kb_inp_rep_send(&hids, c, m->data, m->len, NULL)
                    : bt_hids_inp_rep_send(&hids, c, IN_KEYBOARD, m->data, m->len, NULL);
    case KB_HID_REPORT_NKRO:
    case KB_HID_REPORT_CONSUMER:
    case KB_HID_REPORT_SYSTEM:
    case KB_HID_REPORT_MOUSE: {
        /* A boot-protocol host only reads the boot keyboard report. */
        if (boot) {
            return 0;
        }
        static const uint8_t index[] = {
            [KB_HID_REPORT_NKRO] = IN_NKRO,
            [KB_HID_REPORT_CONSUMER] = IN_CONSUMER,
            [KB_HID_REPORT_SYSTEM] = IN_SYSTEM,
            [KB_HID_REPORT_MOUSE] = IN_MOUSE,
        };
        return bt_hids_inp_rep_send(&hids, c, index[m->report], &m->data[1], m->len - 1u, NULL);
    }
    default:
        return -EINVAL;
    }
}

static void tx_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    struct tx_msg m;
    for (;;) {
        k_msgq_get(&tx_queue, &m, K_FOREVER);
        struct bt_conn *c = conn_get();
        if (!c) {
            continue;
        }
        int err = tx_one(c, &m);
        bt_conn_unref(c);
        if (err && m.report != TX_CONFIG) {
            /* Lost (out of buffers, or the host hasn't enabled that report
             * yet): send the whole state again once it can take it.
             */
            atomic_set(&resync, 1);
        } else if (err) {
            LOG_WRN("config response dropped (%d)", err);
        }
    }
}

static int queue(uint8_t report, const uint8_t *data, size_t len)
{
    if (atomic_get(&state) != KB_BLE_SECURE) {
        return -EAGAIN;
    }
    struct tx_msg m = {.report = report, .len = (uint8_t)len};
    memcpy(m.data, data, len);
    if (k_msgq_put(&tx_queue, &m, K_NO_WAIT) != 0) {
        return -ENOBUFS;
    }
    return 0;
}

int kb_ble_send(enum kb_hid_report report, const uint8_t *data, size_t len)
{
    static const uint8_t sizes[] = {
        [KB_HID_REPORT_BOOT] = KB_HID_BOOT_REPORT_SIZE,
        [KB_HID_REPORT_NKRO] = KB_HID_NKRO_REPORT_SIZE,
        [KB_HID_REPORT_CONSUMER] = KB_HID_CONSUMER_REPORT_SIZE,
        [KB_HID_REPORT_SYSTEM] = KB_HID_SYSTEM_REPORT_SIZE,
        [KB_HID_REPORT_MOUSE] = KB_HID_MOUSE_REPORT_SIZE,
    };
    if ((unsigned)report >= ARRAY_SIZE(sizes) || len != sizes[report]) {
        return -EINVAL;
    }
    int err = queue((uint8_t)report, data, len);
    if (err == -ENOBUFS) {
        /* The link is slower than the keys: start over from the current
         * state, so no release is lost in between.
         */
        k_msgq_purge(&tx_queue);
        atomic_set(&resync, 1);
    }
    return err;
}

int kb_ble_send_config(const uint8_t frame[KB_PROTO_REPORT_SIZE])
{
    return queue(TX_CONFIG, frame, KB_PROTO_REPORT_SIZE);
}

bool kb_ble_boot_protocol(void)
{
    return atomic_get(&boot_protocol) != 0;
}

bool kb_ble_take_resync(void)
{
    return atomic_set(&resync, 0) != 0;
}

uint8_t kb_ble_locks(void)
{
    return (uint8_t)atomic_get(&locks);
}

void kb_ble_set_config_rx(kb_usb_config_rx_fn rx)
{
    config_rx = rx;
}

/* HID service */

static void leds_written(struct bt_hids_rep *rep, struct bt_conn *conn, bool write)
{
    ARG_UNUSED(conn);
    if (write && rep->size >= 1) {
        atomic_set(&locks, rep->data[0]);
    }
}

static void config_written(struct bt_hids_rep *rep, struct bt_conn *conn, bool write)
{
    ARG_UNUSED(conn);
    kb_usb_config_rx_fn rx = config_rx;
    if (write && rx) {
        rx(rep->data, MIN(rep->size, KB_PROTO_REPORT_SIZE));
    }
}

static void notify_changed(enum bt_hids_notify_evt evt)
{
    /* Reports sent before the host listened were lost. */
    if (evt == BT_HIDS_CCCD_EVT_NOTIFY_ENABLED) {
        atomic_set(&resync, 1);
    }
}

static void protocol_mode(enum bt_hids_pm_evt evt, struct bt_conn *conn)
{
    ARG_UNUSED(conn);
    bool boot = evt == BT_HIDS_PM_EVT_BOOT_MODE_ENTERED;
    atomic_set(&boot_protocol, boot);
    atomic_set(&resync, 1);
    LOG_INF("host selected %s protocol", boot ? "boot" : "report");
}

static int hids_init(void)
{
    struct bt_hids_init_param p = {0};
    static const uint8_t in_sizes[] = {
        [IN_KEYBOARD] = KB_HID_BOOT_REPORT_SIZE, [IN_NKRO] = LEN_NKRO,
        [IN_CONSUMER] = LEN_CONSUMER,            [IN_SYSTEM] = LEN_SYSTEM,
        [IN_MOUSE] = LEN_MOUSE,                  [IN_CONFIG] = KB_PROTO_REPORT_SIZE,
    };
    static const uint8_t in_ids[] = {
        [IN_KEYBOARD] = KB_HID_ID_KEYBOARD, [IN_NKRO] = KB_HID_ID_NKRO,
        [IN_CONSUMER] = KB_HID_ID_CONSUMER, [IN_SYSTEM] = KB_HID_ID_SYSTEM,
        [IN_MOUSE] = KB_HID_ID_MOUSE,       [IN_CONFIG] = KB_HID_ID_CONFIG,
    };

    p.rep_map.data = rep_map;
    p.rep_map.size = (uint16_t)kb_hid_ble_desc(rep_map);
    p.info.bcd_hid = 0x0111;
    p.info.flags = BT_HIDS_REMOTE_WAKE | BT_HIDS_NORMALLY_CONNECTABLE;

    for (int i = 0; i < IN_COUNT; i++) {
        struct bt_hids_inp_rep *r = &p.inp_rep_group_init.reports[i];
        r->size = in_sizes[i];
        r->id = in_ids[i];
        r->handler = notify_changed;
    }
    p.inp_rep_group_init.cnt = IN_COUNT;

    p.outp_rep_group_init.reports[OUT_LEDS] = (struct bt_hids_outp_feat_rep){
        .id = KB_HID_ID_KEYBOARD, .size = 1, .handler = leds_written};
    p.outp_rep_group_init.reports[OUT_CONFIG] = (struct bt_hids_outp_feat_rep){
        .id = KB_HID_ID_CONFIG, .size = KB_PROTO_REPORT_SIZE, .handler = config_written};
    p.outp_rep_group_init.cnt = OUT_COUNT;

    p.is_kb = true;
    p.boot_kb_notif_handler = notify_changed;
    p.boot_kb_outp_rep_handler = leds_written;
    p.pm_evt_handler = protocol_mode;
    return bt_hids_init(&hids, &p);
}

/* Bonds */

struct bond_find {
    bool found;
    bt_addr_le_t addr;
};

static void bond_found(const struct bt_bond_info *info, void *user_data)
{
    struct bond_find *f = user_data;
    if (!f->found) {
        f->found = true;
        f->addr = info->addr;
    }
}

static bool profile_bond(uint8_t id, bt_addr_le_t *addr)
{
    struct bond_find f = {0};
    bt_foreach_bond(id, bond_found, &f);
    if (f.found && addr) {
        *addr = f.addr;
    }
    return f.found;
}

static void refresh_bonded(void)
{
    uint8_t mask = 0;
    for (uint8_t id = 0; id < PROFILES; id++) {
        if (profile_bond(id, NULL)) {
            mask |= BIT(id);
        }
    }
    atomic_set(&bonded, mask);
}

uint8_t kb_ble_profile_count(void)
{
    return PROFILES;
}

uint8_t kb_ble_profile(void)
{
    return (uint8_t)atomic_get(&profile);
}

uint8_t kb_ble_bonded(void)
{
    return (uint8_t)atomic_get(&bonded);
}

uint8_t kb_ble_state(void)
{
    return (uint8_t)atomic_get(&state);
}

/* Advertising */

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_GAP_APPEARANCE, (CONFIG_BT_DEVICE_APPEARANCE & 0xFF),
                  (CONFIG_BT_DEVICE_APPEARANCE >> 8)),
    BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(BT_UUID_HIDS_VAL),
                  BT_UUID_16_ENCODE(BT_UUID_BAS_VAL)),
};

static const struct bt_data sd[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static void advertise(void)
{
    struct bt_le_adv_param param = BT_LE_ADV_PARAM_INIT(
        BT_LE_ADV_OPT_CONN, BT_GAP_ADV_FAST_INT_MIN_2, BT_GAP_ADV_FAST_INT_MAX_2, NULL);
    param.id = kb_ble_profile();

    int err = bt_le_adv_start(&param, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    if (err && err != -EALREADY) {
        LOG_ERR("advertising failed (%d)", err);
        atomic_set(&state, KB_BLE_OFF);
        return;
    }
    atomic_set(&state, KB_BLE_ADVERTISING);
    LOG_INF("advertising for host %u", param.id + 1);
}

/* Profile changes, on the system work queue (they write flash). */

#define NO_PROFILE -1
static atomic_t select_request = ATOMIC_INIT(NO_PROFILE);
static atomic_t clear_request = ATOMIC_INIT(NO_PROFILE);

struct saved_profile {
    uint8_t version;
    uint8_t profile;
};
#define SAVED_PROFILE_VERSION 1u

static int profile_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg)
{
    struct saved_profile v;
    if (!settings_name_steq(name, "profile", NULL)) {
        return -ENOENT;
    }
    /* Anything else (an older or newer layout) keeps the default. */
    if (len == sizeof(v) && read_cb(cb_arg, &v, sizeof(v)) == sizeof(v) &&
        v.version == SAVED_PROFILE_VERSION && v.profile < PROFILES) {
        atomic_set(&profile, v.profile);
    }
    return 0;
}
SETTINGS_STATIC_HANDLER_DEFINE(kb_ble, "kb/ble", NULL, profile_set, NULL, NULL);

static void reconnect(void)
{
    struct bt_conn *c = conn_get();
    if (c) {
        /* Advertising for the new profile starts once it is gone (recycled). */
        bt_conn_disconnect(c, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        bt_conn_unref(c);
        return;
    }
    (void)bt_le_adv_stop();
    advertise();
}

static void profile_work_fn(struct k_work *work)
{
    ARG_UNUSED(work);

    int clear = (int)atomic_set(&clear_request, NO_PROFILE);
    if (clear != NO_PROFILE) {
        /* Also drops the host if it is connected on that profile. */
        int err = bt_unpair((uint8_t)clear, NULL);
        LOG_INF("forgot host %d (%d)", clear + 1, err);
        refresh_bonded();
    }

    int next = (int)atomic_set(&select_request, NO_PROFILE);
    if (next != NO_PROFILE && next != (int)kb_ble_profile()) {
        atomic_set(&profile, next);
        struct saved_profile v = {.version = SAVED_PROFILE_VERSION, .profile = (uint8_t)next};
        int err = settings_save_one("kb/ble/profile", &v, sizeof(v));
        if (err) {
            LOG_WRN("saving the profile failed (%d)", err);
        }
        LOG_INF("switching to host %d", next + 1);
        reconnect();
    } else if (clear != NO_PROFILE && clear == (int)kb_ble_profile() && !current) {
        /* The cleared profile is open for a new host now. */
        reconnect();
    }
}
static K_WORK_DEFINE(profile_work, profile_work_fn);

int kb_ble_select_profile(uint8_t p)
{
    if (p >= PROFILES) {
        return -EINVAL;
    }
    if (!atomic_get(&up)) {
        return -EAGAIN;
    }
    atomic_set(&select_request, p);
    k_work_submit(&profile_work);
    return 0;
}

int kb_ble_clear_profile(uint8_t p)
{
    if (p >= PROFILES) {
        return -EINVAL;
    }
    if (!atomic_get(&up)) {
        return -EAGAIN;
    }
    atomic_set(&clear_request, p);
    k_work_submit(&profile_work);
    return 0;
}

static void advertise_work_fn(struct k_work *work)
{
    ARG_UNUSED(work);
    if (!current) {
        advertise();
    }
}
static K_WORK_DEFINE(advertise_work, advertise_work_fn);

/* Connections */

static struct bt_gatt_exchange_params mtu_params;

static void mtu_done(struct bt_conn *conn, uint8_t err, struct bt_gatt_exchange_params *params)
{
    ARG_UNUSED(params);
    uint16_t mtu = bt_gatt_get_mtu(conn);
    if (mtu < KB_PROTO_REPORT_SIZE + 3u) {
        LOG_WRN("ATT MTU %u (err %u): the configurator can't reach the board over Bluetooth", mtu,
                err);
    }
}

static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        LOG_WRN("connection failed (0x%02x)", err);
        k_work_submit(&advertise_work);
        return;
    }

    struct bt_conn_info info;
    bt_addr_le_t bond;
    if (bt_conn_get_info(conn, &info) == 0 && profile_bond(info.id, &bond) &&
        !bt_addr_le_eq(&bond, bt_conn_get_dst(conn))) {
        /* Not the host this profile is paired with. */
        LOG_INF("dropping a host that isn't paired with host %u", info.id + 1);
        bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        return;
    }

    K_SPINLOCK(&conn_lock) {
        current = bt_conn_ref(conn);
    }
    atomic_set(&boot_protocol, 0);
    atomic_set(&state, KB_BLE_CONNECTED);
    int e = bt_hids_connected(&hids, conn);
    if (e) {
        LOG_ERR("HID service didn't take the connection (%d)", e);
    }
    /* A bonded host encrypts with its keys; a new one is asked to pair. */
    e = bt_conn_set_security(conn, BT_SECURITY_L2);
    if (e) {
        LOG_WRN("asking for encryption failed (%d)", e);
    }
    LOG_INF("connected on host %u", info.id + 1);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    struct bt_conn *was = NULL;
    K_SPINLOCK(&conn_lock) {
        if (current == conn) {
            was = current;
            current = NULL;
        }
    }
    if (!was) {
        return;
    }
    (void)bt_hids_disconnected(&hids, conn);
    bt_conn_unref(was);
    atomic_set(&boot_protocol, 0);
    atomic_set(&locks, 0);
    atomic_set(&state, KB_BLE_OFF);
    k_msgq_purge(&tx_queue);
    LOG_INF("disconnected (0x%02x)", reason);
}

static void recycled(void)
{
    /* A connection object is free again: advertise for the selected profile. */
    k_work_submit(&advertise_work);
}

static void security_changed(struct bt_conn *conn, bt_security_t level, enum bt_security_err err)
{
    if (err || level < BT_SECURITY_L2) {
        LOG_WRN("encryption failed (%d)", err);
        bt_conn_disconnect(conn, BT_HCI_ERR_AUTH_FAIL);
        return;
    }
    if (conn != current) {
        return;
    }
    atomic_set(&state, KB_BLE_SECURE);
    atomic_set(&resync, 1);
    refresh_bonded();

    mtu_params.func = mtu_done;
    if (bt_gatt_exchange_mtu(conn, &mtu_params) != 0) {
        /* Already exchanged by the host, or in progress. */
        mtu_done(conn, 0, &mtu_params);
    }
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
    .recycled = recycled,
    .security_changed = security_changed,
};

/* Pairing: Just Works, and only on a profile without a host. */

static enum bt_security_err pairing_accept(struct bt_conn *conn,
                                           const struct bt_conn_pairing_feat *const feat)
{
    ARG_UNUSED(feat);
    struct bt_conn_info info;
    bt_addr_le_t bond;
    if (bt_conn_get_info(conn, &info) != 0) {
        return BT_SECURITY_ERR_UNSPECIFIED;
    }
    /* The paired host itself may pair again (it lost its keys). */
    if (profile_bond(info.id, &bond) && !bt_addr_le_eq(&bond, bt_conn_get_dst(conn))) {
        LOG_INF("host %u already has a computer; BT_CLR forgets it", info.id + 1);
        return BT_SECURITY_ERR_PAIR_NOT_ALLOWED;
    }
    return BT_SECURITY_ERR_SUCCESS;
}

static void pairing_cancel(struct bt_conn *conn)
{
    ARG_UNUSED(conn);
    LOG_INF("pairing cancelled");
}

static struct bt_conn_auth_cb auth_callbacks = {
    .pairing_accept = pairing_accept,
    .cancel = pairing_cancel,
};

static void pairing_complete(struct bt_conn *conn, bool bond)
{
    ARG_UNUSED(conn);
    LOG_INF("paired%s", bond ? " and bonded" : "");
    refresh_bonded();
}

static void pairing_failed(struct bt_conn *conn, enum bt_security_err reason)
{
    ARG_UNUSED(conn);
    LOG_WRN("pairing failed (%d)", reason);
}

static struct bt_conn_auth_info_cb auth_info_callbacks = {
    .pairing_complete = pairing_complete,
    .pairing_failed = pairing_failed,
};

/* Battery */

void kb_ble_set_battery(uint8_t percent)
{
    if (atomic_get(&up)) {
        (void)bt_bas_set_battery_level(MIN(percent, 100));
    }
}

/* Setup */

int kb_ble_init(void)
{
    int err = bt_conn_auth_cb_register(&auth_callbacks);
    err = err ? err : bt_conn_auth_info_cb_register(&auth_info_callbacks);
    if (err) {
        LOG_ERR("registering pairing callbacks failed (%d)", err);
        return err;
    }

    err = hids_init();
    if (err) {
        LOG_ERR("HID service init failed (%d)", err);
        return err;
    }

    err = bt_enable(NULL);
    if (err) {
        LOG_ERR("Bluetooth init failed (%d)", err);
        return err;
    }

    /* Bonds, identities, the profile and the preferred output (output.c). */
    err = settings_load();
    if (err) {
        LOG_WRN("loading settings failed (%d)", err);
    }

    /* One identity per profile; new ones are kept in settings. */
    size_t ids = CONFIG_BT_ID_MAX;
    bt_id_get(NULL, &ids);
    while (ids < PROFILES) {
        int id = bt_id_create(NULL, NULL);
        if (id < 0) {
            LOG_ERR("creating identity %u failed (%d)", (unsigned)ids, id);
            return id;
        }
        ids++;
    }
    refresh_bonded();

    k_thread_create(&tx_thread_data, tx_stack, K_THREAD_STACK_SIZEOF(tx_stack), tx_thread, NULL,
                    NULL, NULL, TX_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(&tx_thread_data, "kb_ble_tx");

    atomic_set(&up, 1);
    advertise();
    return 0;
}
