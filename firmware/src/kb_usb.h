/*
 * USB device: the boot keyboard interface and the extra HID interface
 * (NKRO, consumer and system control). Report formats are in kb/hid.h.
 */
#ifndef KB_USB_H
#define KB_USB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "kb/hid.h"

int kb_usb_init(void);

/*
 * Queue one report from kb/hid.h; reports on each interface go out in
 * order, one per poll. Safe from any thread. Returns -EAGAIN while the
 * interface isn't configured or the host is suspended. While suspended,
 * it asks the host to wake up (if the host allowed remote wakeup).
 */
int kb_usb_send(enum kb_hid_report report, const uint8_t *data, size_t len);

/* The host asked for boot protocol on the keyboard interface. */
bool kb_usb_boot_protocol(void);

/*
 * True once after the host may have missed reports: a new configuration,
 * a resume, or a queue overflow. The caller then sends every report again.
 */
bool kb_usb_take_resync(void);

#endif /* KB_USB_H */
