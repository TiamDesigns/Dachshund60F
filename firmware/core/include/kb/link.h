/*
 * The 2.4 GHz link between the keyboard and its dongle: packets carried by
 * Nordic's Enhanced ShockBurst (ESB).
 *
 * The keyboard transmits (ESB PTX) and the dongle listens (PRX). Whatever
 * the dongle has for the keyboard rides back in ESB acknowledgements, so
 * the keyboard sends a poll every so often when it has nothing else. Every
 * packet starts with its type:
 *
 *   keyboard to dongle
 *     POLL        nothing: just collecting what the dongle has
 *     REPORT      report kind (enum kb_hid_report), then the report as
 *                 kb/hid.h formats it for USB
 *     CONFIG      one config protocol response frame (64 bytes)
 *     PAIR        on the pairing address: the keyboard's device ID
 *   dongle to keyboard (in acknowledgements)
 *     STATUS      the USB host's lock LEDs and state
 *     CONFIG      one config protocol request frame (64 bytes)
 *     PAIR_OFFER  on the pairing address: the dongle's link address
 *
 * Pairing: the dongle's link address comes from its device ID, so it never
 * changes. In pairing mode the dongle also listens on a fixed pairing
 * address and offers its link address there; a keyboard in pairing mode
 * takes the offer, keeps it and moves to it. The link isn't encrypted yet.
 *
 * Hardware-independent: the firmware on both ends encodes and decodes
 * here, and the host tests cover it.
 */
#ifndef KB_LINK_H
#define KB_LINK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "kb/hid.h"

/* Largest packet: a config frame and its type byte, with room to spare. */
#define KB_LINK_PAYLOAD_MAX 72u
#define KB_LINK_CONFIG_SIZE 64u
#define KB_LINK_ID_SIZE 8u
/* 2475 MHz: above Wi-Fi channel 11 and below Bluetooth advertising channel 39 (2480 MHz). */
#define KB_LINK_CHANNEL 75u

enum kb_link_type {
    KB_LINK_POLL = 1,
    KB_LINK_REPORT = 2,
    KB_LINK_CONFIG = 3,
    KB_LINK_STATUS = 4,
    KB_LINK_PAIR = 5,
    KB_LINK_PAIR_OFFER = 6,
};

/* STATUS flags */
#define KB_LINK_USB_READY 0x01u     /* a host has configured the dongle */
#define KB_LINK_BOOT_PROTOCOL 0x02u /* the host asked for boot protocol */
#define KB_LINK_RESYNC 0x04u        /* the host may have missed reports: send them all again */
#define KB_LINK_SUSPENDED 0x08u     /* the host is asleep (a key wakes it) */

struct kb_link_status {
    uint8_t locks; /* KB_HID_LED_* bits */
    uint8_t flags; /* KB_LINK_* */
};

/* An ESB address: four base bytes and a one-byte prefix. */
struct kb_link_address {
    uint8_t base[4];
    uint8_t prefix;
};

/* The fixed address dongles in pairing mode listen on. */
extern const struct kb_link_address kb_link_pairing_address;

struct kb_link_packet {
    uint8_t type; /* enum kb_link_type */
    union {
        struct {
            uint8_t kind; /* enum kb_hid_report */
            uint8_t len;
            const uint8_t *data;
        } report;
        const uint8_t *config; /* KB_LINK_CONFIG_SIZE bytes */
        struct kb_link_status status;
        const uint8_t *id; /* KB_LINK_ID_SIZE bytes */
        struct kb_link_address address;
    };
};

/* The size of a report of this kind, or 0 for none. */
size_t kb_link_report_size(uint8_t kind);

/*
 * Encoders write into buf (KB_LINK_PAYLOAD_MAX bytes) and return the
 * packet length, or 0 if the input is invalid (a report of the wrong size,
 * an unknown kind, an unusable address).
 */
size_t kb_link_encode_poll(uint8_t *buf);
size_t kb_link_encode_report(enum kb_hid_report kind, const uint8_t *data, size_t len,
                             uint8_t *buf);
size_t kb_link_encode_config(const uint8_t frame[KB_LINK_CONFIG_SIZE], uint8_t *buf);
size_t kb_link_encode_status(const struct kb_link_status *s, uint8_t *buf);
size_t kb_link_encode_pair(const uint8_t id[KB_LINK_ID_SIZE], uint8_t *buf);
size_t kb_link_encode_pair_offer(const struct kb_link_address *a, uint8_t *buf);

/*
 * Decode one received packet. Returns 0, or -1 for anything malformed (an
 * unknown type, a wrong length, a report of the wrong size, an unusable
 * address); out then holds nothing useful. Pointers in out point into buf.
 */
int kb_link_decode(const uint8_t *buf, size_t len, struct kb_link_packet *out);

/*
 * An address ESB can use reliably: not the pairing address, and no base
 * or prefix byte that is all zeros, all ones or a 0x55/0xAA preamble
 * lookalike.
 */
bool kb_link_address_valid(const struct kb_link_address *a);

/* The dongle's link address, from its device ID: stable, and always valid. */
void kb_link_address_from_id(const uint8_t *id, size_t len, struct kb_link_address *out);

#endif /* KB_LINK_H */
