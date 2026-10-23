/*
 * USB device and HID keyboard interface.
 */
#ifndef KB_USB_H
#define KB_USB_H

#include <stdint.h>

/* Boot-protocol keyboard report: modifiers, reserved, six keycodes. */
#define KB_USB_KBD_REPORT_SIZE 8

int kb_usb_init(void);

/*
 * Queue an 8-byte boot keyboard report; reports go out in order, one per
 * poll. Returns -EAGAIN if USB isn't configured. Safe from any thread.
 */
int kb_usb_send_keyboard(const uint8_t report[KB_USB_KBD_REPORT_SIZE]);

#endif /* KB_USB_H */
