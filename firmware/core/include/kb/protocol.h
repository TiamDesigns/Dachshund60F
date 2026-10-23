/*
 * Config protocol: handles one request frame from the host and builds the
 * response frame. Frame layout and commands are defined in
 * protocol/protocol.yaml (bindings in kb/protocol_gen.h).
 *
 * Hardware- and transport-independent: the firmware hands in the bytes of
 * one output report (USB now, BLE and the dongle later) and sends back the
 * response. Every index and length from the host is checked here; a
 * malformed frame gets an error status and changes nothing.
 */
#ifndef KB_PROTOCOL_H
#define KB_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "kb/keymap.h"
#include "kb/protocol_gen.h"

/* What the firmware does for the commands that reach beyond the keymap. */
struct kb_protocol_ops {
    /*
     * Save, revert, factory reset: return a KB_PROTO_* status. Revert and
     * factory reset replace the live keymap themselves.
     */
    uint8_t (*save)(void *ctx);
    uint8_t (*revert)(void *ctx);
    uint8_t (*factory_reset)(void *ctx);
    /* Reboot once the response is on its way (bootloader: into UF2). */
    void (*reboot)(bool bootloader, void *ctx);
    /* Flash the indicators for this many seconds. NULL: not supported. */
    void (*identify)(uint8_t seconds, void *ctx);
    /* Guard keymap changes against the scan thread. Optional. */
    void (*lock)(void *ctx);
    void (*unlock)(void *ctx);
};

struct kb_protocol {
    struct kb_keymap *keymap;
    const struct kb_protocol_ops *ops;
    void *ctx;
    /* Handshake fields that don't come from the keymap. */
    uint8_t firmware_major;
    uint8_t firmware_minor;
    uint8_t firmware_patch;
    const char *build;
    const char *model;
    const char *serial; /* the USB serial number string */
    uint32_t capabilities;
    uint8_t encoders;
    uint16_t macro_buffer_size;
};

/*
 * Handle one request of req_len bytes (a whole report, or less) and write
 * a KB_PROTO_REPORT_SIZE response into resp, zero-padded. Returns false if
 * the request is too short to answer (no header).
 */
bool kb_protocol_handle(const struct kb_protocol *p, const uint8_t *req, size_t req_len,
                        uint8_t resp[KB_PROTO_REPORT_SIZE]);

#endif /* KB_PROTOCOL_H */
