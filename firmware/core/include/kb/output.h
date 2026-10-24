/*
 * Output selection: which transport the keys go to (USB, Bluetooth or the
 * 2.4 GHz dongle).
 *
 * Hardware-independent. The firmware tells it which transports have a host
 * ready; the preferred one (OUT_USB, OUT_BLE, OUT_2G4 or the config
 * protocol) wins while it is ready. While it isn't, the first ready one of
 * USB, the dongle and Bluetooth is used, so unplugging the cable carries
 * on wirelessly and plugging it in moves back.
 */
#ifndef KB_OUTPUT_H
#define KB_OUTPUT_H

#include <stdbool.h>
#include <stdint.h>

/* Same values as get_connection's outputs and enum kb_transport (kb/screen.h). */
enum kb_output {
    KB_OUTPUT_NONE = 0,
    KB_OUTPUT_USB = 1,
    KB_OUTPUT_BLE = 2,
    KB_OUTPUT_DONGLE = 3,
};

/* Bluetooth link states, as get_connection reports them. */
enum kb_ble_state {
    KB_BLE_OFF = 0,
    KB_BLE_ADVERTISING = 1,
    KB_BLE_CONNECTED = 2, /* not yet encrypted: no keys go out */
    KB_BLE_SECURE = 3,
};

struct kb_output_status {
    uint8_t preferred; /* KB_OUTPUT_USB, KB_OUTPUT_BLE or KB_OUTPUT_DONGLE */
    bool usb_ready;    /* a host configured the board (suspended still counts) */
    uint8_t ble;       /* enum kb_ble_state */
    bool dongle_ready; /* the dongle is in range and a host configured it */
};

/* True for an output that set_output and the OUT_ keys may prefer (builds without a dongle refuse it first). */
bool kb_output_preferable(uint8_t output);

/* The preferred output if its host is ready, else the first ready of USB, dongle, Bluetooth, else none. */
enum kb_output kb_output_select(const struct kb_output_status *s);

#endif /* KB_OUTPUT_H */
