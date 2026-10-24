/*
 * The 2.4 GHz dongle link, keyboard end: Enhanced ShockBurst as PTX, on
 * the MPSL timeslot API so it shares the radio with Bluetooth. Packets are
 * kb/link.h's.
 *
 * Reports go out the moment they are queued. Whatever the dongle has for
 * the keyboard (its host's lock LEDs and state, config requests) rides
 * back in acknowledgements, so the keyboard polls it every few
 * milliseconds while in use, more slowly while idle, and not at all once
 * the radio is off (power management) or nothing is paired.
 *
 * Pairing (RF_PAIR, pair_dongle): for 30 s the keyboard asks on the fixed
 * pairing address; a dongle in pairing mode offers its own address, and
 * the keyboard keeps it (Zephyr settings, next to the Bluetooth bonds).
 * The link isn't encrypted yet.
 */
#ifndef KB_ESB_H
#define KB_ESB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "kb/hid.h"
#include "kb/protocol.h"
#include "kb/protocol_gen.h"
#include "kb_usb.h"

/* After settings_load (kb_ble_init): picks up the paired dongle. */
int kb_esb_init(void);

/* The dongle is in range and its host has configured it: keys can go there. */
bool kb_esb_ready(void);

/*
 * Queue one report from kb/hid.h. Safe from any thread. -EAGAIN while the
 * dongle isn't ready, -ENOBUFS when the radio queue is full (resync then
 * fires).
 */
int kb_esb_send(enum kb_hid_report report, const uint8_t *data, size_t len);

/* The dongle's host asked for boot protocol. */
bool kb_esb_boot_protocol(void);

/* True once after the dongle's host may have missed reports. */
bool kb_esb_take_resync(void);

/* The dongle's host's lock LEDs (KB_HID_LED_* bits). */
uint8_t kb_esb_locks(void);

/* Config requests relayed by the dongle, on a work queue. Keep it short. */
void kb_esb_set_config_rx(kb_usb_config_rx_fn rx);

/* Send one config response frame to the dongle. -EAGAIN if it isn't in range. */
int kb_esb_send_config(const uint8_t frame[KB_PROTO_REPORT_SIZE]);

/* get_dongle */
void kb_esb_get(struct kb_proto_get_dongle_resp *out);

/* pair_dongle, RF_PAIR: KB_PROTO_PAIR_STOP, _START or _FORGET. -EINVAL for anything else. */
int kb_esb_pair(uint8_t mode);

/*
 * Power management, from the scan thread: idle polls slowly; radio off
 * stops polling until the next report (a key) or radio on.
 */
void kb_esb_set_power(bool idle, bool radio_off);

/* Before deep sleep. */
void kb_esb_shutdown(void);

#endif /* KB_ESB_H */
