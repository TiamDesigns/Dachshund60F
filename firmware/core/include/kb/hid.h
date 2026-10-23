/*
 * HID report descriptors, report packing and the report selector.
 *
 * Hardware- and transport-independent: USB uses these now, BLE and the
 * dongle reuse them later.
 *
 * Three HID interfaces (the third, for config, is in protocol/protocol.yaml):
 * - Keyboard: boot subclass, the 8-byte 6KRO report (mods, reserved, six
 *   keys) with no report ID, and the lock-LED output report. This is all a
 *   BIOS or KVM sees.
 * - Extra: report protocol only, with report IDs: an NKRO bitmap keyboard,
 *   consumer control and system control.
 *
 * Keys go out on exactly one path at a time. In NKRO mode the keyboard
 * interface stays at all zeros and every key and modifier goes in the NKRO
 * report; in 6KRO mode the NKRO report stays empty. NKRO mode is used only
 * while NKRO is enabled and the host has not asked for boot protocol, so a
 * BIOS always gets the boot report.
 */
#ifndef KB_HID_H
#define KB_HID_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "kb/engine.h"

/* Report IDs on the extra interface */
#define KB_HID_ID_NKRO 1u
#define KB_HID_ID_CONSUMER 2u
#define KB_HID_ID_SYSTEM 3u

/* The NKRO bitmap covers keyboard usages 0x00-0xDF; modifiers have their own byte. */
#define KB_HID_NKRO_USAGES 0xE0u
#define KB_HID_NKRO_BITMAP_SIZE (KB_HID_NKRO_USAGES / 8u)

/* Report sizes in bytes, including the report ID where there is one */
#define KB_HID_BOOT_REPORT_SIZE 8u
#define KB_HID_NKRO_REPORT_SIZE (2u + KB_HID_NKRO_BITMAP_SIZE)
#define KB_HID_CONSUMER_REPORT_SIZE 3u
#define KB_HID_SYSTEM_REPORT_SIZE 3u
#define KB_HID_EXTRA_REPORT_MAX KB_HID_NKRO_REPORT_SIZE

/* Lock-LED output report bits (keyboard interface) */
#define KB_HID_LED_NUM 0x01u
#define KB_HID_LED_CAPS 0x02u
#define KB_HID_LED_SCROLL 0x04u
#define KB_HID_LED_COMPOSE 0x08u
#define KB_HID_LED_KANA 0x10u

extern const uint8_t kb_hid_keyboard_desc[];
extern const size_t kb_hid_keyboard_desc_len;
extern const uint8_t kb_hid_extra_desc[];
extern const size_t kb_hid_extra_desc_len;

/*
 * Config interface: the vendor-defined page from protocol/protocol.yaml,
 * one KB_PROTO_REPORT_SIZE input and output report, no report ID.
 */
extern const uint8_t kb_hid_config_desc[];
extern const size_t kb_hid_config_desc_len;

/* Report packers. Each writes exactly its report size. */
void kb_hid_pack_nkro(const struct kb_hid_state *hid, uint8_t out[KB_HID_NKRO_REPORT_SIZE]);
void kb_hid_pack_consumer(const struct kb_hid_state *hid,
                          uint8_t out[KB_HID_CONSUMER_REPORT_SIZE]);
void kb_hid_pack_system(const struct kb_hid_state *hid, uint8_t out[KB_HID_SYSTEM_REPORT_SIZE]);

enum kb_hid_report {
    KB_HID_REPORT_BOOT,     /* keyboard interface */
    KB_HID_REPORT_NKRO,     /* extra interface, ID 1 */
    KB_HID_REPORT_CONSUMER, /* extra interface, ID 2 */
    KB_HID_REPORT_SYSTEM,   /* extra interface, ID 3 */
};

/* Hands one report to the transport. */
typedef void (*kb_hid_send_fn)(enum kb_hid_report report, const uint8_t *data, size_t len,
                               void *ctx);

/*
 * Turns HID state into the reports to send: only those that changed, on
 * the path the current mode selects, and a release on the old path when
 * the mode switches so no key sticks down.
 */
struct kb_hid_reporter {
    kb_hid_send_fn send;
    void *ctx;
    bool nkro_enabled;
    bool boot_protocol;
    bool nkro_active;
    uint8_t last_boot[KB_HID_BOOT_REPORT_SIZE];
    uint8_t last_nkro[KB_HID_NKRO_REPORT_SIZE];
    uint8_t last_consumer[KB_HID_CONSUMER_REPORT_SIZE];
    uint8_t last_system[KB_HID_SYSTEM_REPORT_SIZE];
};

/* Starts in report protocol, with nothing held and nothing sent. */
void kb_hid_reporter_init(struct kb_hid_reporter *r, bool nkro_enabled, kb_hid_send_fn send,
                          void *ctx);

/* Send whatever changed since the last call. */
void kb_hid_reporter_update(struct kb_hid_reporter *r, const struct kb_hid_state *hid);

/* NKRO on or off (setting or NK_TOGG). Moves held keys to the new path. */
void kb_hid_reporter_set_nkro(struct kb_hid_reporter *r, bool enabled,
                              const struct kb_hid_state *hid);

/* The host picked boot (true) or report protocol. Moves held keys to the new path. */
void kb_hid_reporter_set_boot_protocol(struct kb_hid_reporter *r, bool boot,
                                       const struct kb_hid_state *hid);

/*
 * Forget what was sent and send every report again, for a host that may
 * have missed them (after a resume, or a new connection).
 */
void kb_hid_reporter_resync(struct kb_hid_reporter *r, const struct kb_hid_state *hid);

static inline bool kb_hid_reporter_nkro_active(const struct kb_hid_reporter *r)
{
    return r->nkro_active;
}

#endif /* KB_HID_H */
