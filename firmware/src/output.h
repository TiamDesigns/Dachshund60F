/*
 * The report layer between the keyboard and its transports: reports go to
 * the output kb/output.h picks (USB, Bluetooth or the dongle), and the keyboard follows
 * that output's boot protocol, lock LEDs and resync requests.
 */
#ifndef OUTPUT_H
#define OUTPUT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "kb/hid.h"
#include "kb/output.h"

/*
 * Scan thread, every scan: follow the transports. When keys move to
 * another output, the old one gets an all-released report of each kind.
 * Returns true when the reporter must send everything again (a new output,
 * or one that may have missed reports).
 */
bool output_poll(void);

/* Send one report on the active output. -EAGAIN when no host is ready. */
int output_send(enum kb_hid_report report, const uint8_t *data, size_t len);

/* The active output's host asked for boot protocol. */
bool output_boot_protocol(void);

enum kb_output output_active(void);
uint8_t output_preferred(void);

/* An output this build has: USB, Bluetooth, and the dongle when built in. */
bool output_available(uint8_t output);

/*
 * Prefer USB, Bluetooth or the dongle (OUT_USB, OUT_BLE, OUT_2G4,
 * set_output); kept across power cycles. -EINVAL for an output this build
 * lacks.
 */
int output_set_preferred(uint8_t output);

#endif /* OUTPUT_H */
