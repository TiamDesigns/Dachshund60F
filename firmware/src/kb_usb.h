/*
 * USB device: the boot keyboard interface, the extra HID interface (NKRO,
 * consumer and system control) and the config interface. Report formats
 * are in kb/hid.h and protocol/protocol.yaml.
 */
#ifndef KB_USB_H
#define KB_USB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "kb/hid.h"
#include "kb/protocol_gen.h"

int kb_usb_init(void);

/*
 * Queue one report from kb/hid.h; reports on each interface go out in
 * order, one per poll. Safe from any thread. Returns -EAGAIN while the
 * interface isn't configured or the host is suspended. While suspended,
 * it asks the host to wake up (if the host allowed remote wakeup).
 */
int kb_usb_send(enum kb_hid_report report, const uint8_t *data, size_t len);

/*
 * A host has configured the board (it still counts while the host is
 * suspended, so a key press can wake it).
 */
bool kb_usb_ready(void);

/* The host's lock LEDs (KB_HID_LED_* bits). */
uint8_t kb_usb_locks(void);

/* The host asked for boot protocol on the keyboard interface. */
bool kb_usb_boot_protocol(void);

/*
 * True once after the host may have missed reports: a new configuration,
 * a resume, or a queue overflow. The caller then sends every report again.
 */
bool kb_usb_take_resync(void);

/*
 * Called on the USB thread with each config request (an output report on
 * the config interface, up to KB_PROTO_REPORT_SIZE bytes). Keep it short:
 * copy the bytes and return.
 */
typedef void (*kb_usb_config_rx_fn)(const uint8_t *data, size_t len);
void kb_usb_set_config_rx(kb_usb_config_rx_fn rx);

/* Queue one config response frame. -EAGAIN if not configured or suspended. */
int kb_usb_send_config(const uint8_t frame[KB_PROTO_REPORT_SIZE]);

#endif /* KB_USB_H */
