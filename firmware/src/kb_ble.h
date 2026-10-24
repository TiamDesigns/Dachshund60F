/*
 * Bluetooth LE: HID over GATT with the same reports as USB (kb/hid.h), on
 * one host profile at a time. Each profile has its own identity (address)
 * and holds at most one bond, so up to CONFIG_BT_ID_MAX computers can be
 * paired and BT_SEL picks which one the board talks to.
 *
 * The config protocol runs over vendor reports on the same HID service
 * (report ID KB_PROTO_HID_BLE_REPORT_ID), so the configurator reaches the
 * board wirelessly once it is paired.
 */
#ifndef KB_BLE_H
#define KB_BLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "kb/hid.h"
#include "kb/protocol_gen.h"
#include "kb_usb.h"

/* Start Bluetooth, load the bonds and the selected profile, and advertise. */
int kb_ble_init(void);

/* Profiles: 0 to kb_ble_profile_count() - 1. */
uint8_t kb_ble_profile_count(void);
uint8_t kb_ble_profile(void);
/* Bit n set: profile n has a bonded host. */
uint8_t kb_ble_bonded(void);
/* enum kb_ble_state (kb/output.h). */
uint8_t kb_ble_state(void);

/*
 * Switch to a profile (BT_SEL), or forget a profile's host (BT_CLR). They
 * return at once and finish on a work queue; -EINVAL for a profile that
 * doesn't exist, -EAGAIN before Bluetooth is up.
 */
int kb_ble_select_profile(uint8_t profile);
int kb_ble_clear_profile(uint8_t profile);

/*
 * Queue one report from kb/hid.h for the connected host. Safe from any
 * thread. -EAGAIN while no host is connected with encryption.
 */
int kb_ble_send(enum kb_hid_report report, const uint8_t *data, size_t len);

/* The host asked for boot protocol. */
bool kb_ble_boot_protocol(void);

/* True once after the host may have missed reports (a new connection, a queue overflow). */
bool kb_ble_take_resync(void);

/* The host's lock LEDs (KB_HID_LED_* bits). */
uint8_t kb_ble_locks(void);

/* Config requests from the host, on the Bluetooth RX thread. Keep it short. */
void kb_ble_set_config_rx(kb_usb_config_rx_fn rx);

/* Queue one config response frame. -EAGAIN if no host is connected. */
int kb_ble_send_config(const uint8_t frame[KB_PROTO_REPORT_SIZE]);

/*
 * Advertise for the selected profile, or stop (power management, on
 * battery when the board has been left alone). A connected host stays
 * connected either way; with advertising off, a lost one can't come back
 * until it is on again.
 */
void kb_ble_set_advertising(bool on);

/* Before deep sleep: stop advertising and disconnect the host (waits up to 100 ms). */
void kb_ble_shutdown(void);

/* The Battery Service level, 0-100 (from power management). */
void kb_ble_set_battery(uint8_t percent);

#endif /* KB_BLE_H */
