/*
 * USB device and HID keyboard interface.
 */
#ifndef USB_HID_H
#define USB_HID_H

#include <stdint.h>

/* Boot-protocol keyboard report: modifiers, reserved, six keycodes. */
#define USB_HID_KBD_REPORT_SIZE 8

int usb_hid_init(void);

/* Queue an 8-byte boot keyboard report. Returns -EAGAIN if not configured. */
int usb_hid_send_keyboard(const uint8_t report[USB_HID_KBD_REPORT_SIZE]);

#endif /* USB_HID_H */
