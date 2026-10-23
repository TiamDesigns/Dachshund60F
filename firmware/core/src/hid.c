#include "kb/hid.h"

#include <string.h>

/* Keyboard interface: the standard boot keyboard (HID 1.11, appendix B.1). */
const uint8_t kb_hid_keyboard_desc[] = {
    0x05, 0x01,       /* Usage Page (Generic Desktop) */
    0x09, 0x06,       /* Usage (Keyboard) */
    0xA1, 0x01,       /* Collection (Application) */
    0x05, 0x07,       /*   Usage Page (Keyboard/Keypad) */
    0x19, 0xE0,       /*   Usage Minimum (Left Control) */
    0x29, 0xE7,       /*   Usage Maximum (Right GUI) */
    0x15, 0x00,       /*   Logical Minimum (0) */
    0x25, 0x01,       /*   Logical Maximum (1) */
    0x75, 0x01,       /*   Report Size (1) */
    0x95, 0x08,       /*   Report Count (8) */
    0x81, 0x02,       /*   Input (Data, Variable, Absolute): modifiers */
    0x95, 0x01,       /*   Report Count (1) */
    0x75, 0x08,       /*   Report Size (8) */
    0x81, 0x01,       /*   Input (Constant): reserved */
    0x95, 0x05,       /*   Report Count (5) */
    0x75, 0x01,       /*   Report Size (1) */
    0x05, 0x08,       /*   Usage Page (LEDs) */
    0x19, 0x01,       /*   Usage Minimum (Num Lock) */
    0x29, 0x05,       /*   Usage Maximum (Kana) */
    0x91, 0x02,       /*   Output (Data, Variable, Absolute): lock LEDs */
    0x95, 0x01,       /*   Report Count (1) */
    0x75, 0x03,       /*   Report Size (3) */
    0x91, 0x01,       /*   Output (Constant): padding */
    0x95, 0x06,       /*   Report Count (6) */
    0x75, 0x08,       /*   Report Size (8) */
    0x15, 0x00,       /*   Logical Minimum (0) */
    0x26, 0xFF, 0x00, /*   Logical Maximum (255) */
    0x05, 0x07,       /*   Usage Page (Keyboard/Keypad) */
    0x19, 0x00,       /*   Usage Minimum (0) */
    0x2A, 0xFF, 0x00, /*   Usage Maximum (255) */
    0x81, 0x00,       /*   Input (Data, Array): six keys */
    0xC0,             /* End Collection */
};
const size_t kb_hid_keyboard_desc_len = sizeof(kb_hid_keyboard_desc);

/* Extra interface: NKRO keyboard, consumer control and system control. */
const uint8_t kb_hid_extra_desc[] = {
    0x05, 0x01,             /* Usage Page (Generic Desktop) */
    0x09, 0x06,             /* Usage (Keyboard) */
    0xA1, 0x01,             /* Collection (Application) */
    0x85, KB_HID_ID_NKRO,   /*   Report ID */
    0x05, 0x07,             /*   Usage Page (Keyboard/Keypad) */
    0x19, 0xE0,             /*   Usage Minimum (Left Control) */
    0x29, 0xE7,             /*   Usage Maximum (Right GUI) */
    0x15, 0x00,             /*   Logical Minimum (0) */
    0x25, 0x01,             /*   Logical Maximum (1) */
    0x75, 0x01,             /*   Report Size (1) */
    0x95, 0x08,             /*   Report Count (8) */
    0x81, 0x02,             /*   Input (Data, Variable, Absolute): modifiers */
    0x19, 0x00,             /*   Usage Minimum (0) */
    0x29, KB_HID_NKRO_USAGES - 1u, /* Usage Maximum (0xDF) */
    0x95, KB_HID_NKRO_USAGES, /* Report Count (224) */
    0x81, 0x02,             /*   Input (Data, Variable, Absolute): key bitmap */
    0xC0,                   /* End Collection */

    0x05, 0x0C,             /* Usage Page (Consumer) */
    0x09, 0x01,             /* Usage (Consumer Control) */
    0xA1, 0x01,             /* Collection (Application) */
    0x85, KB_HID_ID_CONSUMER, /* Report ID */
    0x15, 0x01,             /*   Logical Minimum (1) */
    0x26, 0xFF, 0x03,       /*   Logical Maximum (0x3FF) */
    0x19, 0x01,             /*   Usage Minimum (1) */
    0x2A, 0xFF, 0x03,       /*   Usage Maximum (0x3FF) */
    0x75, 0x10,             /*   Report Size (16) */
    0x95, 0x01,             /*   Report Count (1) */
    0x81, 0x00,             /*   Input (Data, Array) */
    0xC0,                   /* End Collection */

    0x05, 0x01,             /* Usage Page (Generic Desktop) */
    0x09, 0x80,             /* Usage (System Control) */
    0xA1, 0x01,             /* Collection (Application) */
    0x85, KB_HID_ID_SYSTEM, /*   Report ID */
    0x15, 0x01,             /*   Logical Minimum (1) */
    0x26, 0xB7, 0x00,       /*   Logical Maximum (0xB7) */
    0x19, 0x01,             /*   Usage Minimum (1) */
    0x2A, 0xB7, 0x00,       /*   Usage Maximum (0xB7) */
    0x75, 0x10,             /*   Report Size (16) */
    0x95, 0x01,             /*   Report Count (1) */
    0x81, 0x00,             /*   Input (Data, Array) */
    0xC0,                   /* End Collection */
};
const size_t kb_hid_extra_desc_len = sizeof(kb_hid_extra_desc);

/* Packers */

void kb_hid_pack_nkro(const struct kb_hid_state *hid, uint8_t out[KB_HID_NKRO_REPORT_SIZE])
{
    out[0] = KB_HID_ID_NKRO;
    out[1] = hid->mods;
    memcpy(&out[2], hid->keys, KB_HID_NKRO_BITMAP_SIZE);
}

static void pack_u16(uint8_t id, uint16_t usage, uint8_t out[3])
{
    out[0] = id;
    out[1] = (uint8_t)(usage & 0xFFu);
    out[2] = (uint8_t)(usage >> 8);
}

void kb_hid_pack_consumer(const struct kb_hid_state *hid,
                          uint8_t out[KB_HID_CONSUMER_REPORT_SIZE])
{
    pack_u16(KB_HID_ID_CONSUMER, hid->consumer, out);
}

void kb_hid_pack_system(const struct kb_hid_state *hid, uint8_t out[KB_HID_SYSTEM_REPORT_SIZE])
{
    pack_u16(KB_HID_ID_SYSTEM, hid->system, out);
}

/* Reporter */

static const struct kb_hid_state empty_state;

/* Sends the report if it differs from last (or always, when forced), and remembers it. */
static void emit(struct kb_hid_reporter *r, enum kb_hid_report which, const uint8_t *report,
                 uint8_t *last, size_t len, bool force)
{
    if (!force && memcmp(report, last, len) == 0) {
        return;
    }
    memcpy(last, report, len);
    r->send(which, report, len, r->ctx);
}

static void emit_all(struct kb_hid_reporter *r, const struct kb_hid_state *hid, bool force)
{
    /* Keys on the active path; the other path carries nothing. */
    const struct kb_hid_state *boot_src = r->nkro_active ? &empty_state : hid;
    const struct kb_hid_state *nkro_src = r->nkro_active ? hid : &empty_state;
    uint8_t boot[KB_HID_BOOT_REPORT_SIZE];
    uint8_t nkro[KB_HID_NKRO_REPORT_SIZE];
    uint8_t consumer[KB_HID_CONSUMER_REPORT_SIZE];
    uint8_t system[KB_HID_SYSTEM_REPORT_SIZE];

    kb_hid_boot_report(boot_src, boot);
    kb_hid_pack_nkro(nkro_src, nkro);
    kb_hid_pack_consumer(hid, consumer);
    kb_hid_pack_system(hid, system);

    /* Release on the old path before pressing on the new one. */
    if (r->nkro_active) {
        emit(r, KB_HID_REPORT_BOOT, boot, r->last_boot, sizeof(boot), force);
        emit(r, KB_HID_REPORT_NKRO, nkro, r->last_nkro, sizeof(nkro), force);
    } else {
        emit(r, KB_HID_REPORT_NKRO, nkro, r->last_nkro, sizeof(nkro), force);
        emit(r, KB_HID_REPORT_BOOT, boot, r->last_boot, sizeof(boot), force);
    }
    emit(r, KB_HID_REPORT_CONSUMER, consumer, r->last_consumer, sizeof(consumer), force);
    emit(r, KB_HID_REPORT_SYSTEM, system, r->last_system, sizeof(system), force);
}

static void select_path(struct kb_hid_reporter *r, const struct kb_hid_state *hid)
{
    r->nkro_active = r->nkro_enabled && !r->boot_protocol;
    emit_all(r, hid, false);
}

void kb_hid_reporter_init(struct kb_hid_reporter *r, bool nkro_enabled, kb_hid_send_fn send,
                          void *ctx)
{
    memset(r, 0, sizeof(*r));
    r->send = send;
    r->ctx = ctx;
    r->nkro_enabled = nkro_enabled;
    r->nkro_active = nkro_enabled;
    /* What the host assumes before the first report. */
    kb_hid_boot_report(&empty_state, r->last_boot);
    kb_hid_pack_nkro(&empty_state, r->last_nkro);
    kb_hid_pack_consumer(&empty_state, r->last_consumer);
    kb_hid_pack_system(&empty_state, r->last_system);
}

void kb_hid_reporter_update(struct kb_hid_reporter *r, const struct kb_hid_state *hid)
{
    emit_all(r, hid, false);
}

void kb_hid_reporter_set_nkro(struct kb_hid_reporter *r, bool enabled,
                              const struct kb_hid_state *hid)
{
    r->nkro_enabled = enabled;
    select_path(r, hid);
}

void kb_hid_reporter_set_boot_protocol(struct kb_hid_reporter *r, bool boot,
                                       const struct kb_hid_state *hid)
{
    r->boot_protocol = boot;
    select_path(r, hid);
}

void kb_hid_reporter_resync(struct kb_hid_reporter *r, const struct kb_hid_state *hid)
{
    emit_all(r, hid, true);
}
