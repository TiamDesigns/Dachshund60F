/*
 * One fuzz input through the protocol handler: a sequence of frames, each
 * prefixed by its length byte (taken mod 65, so a frame can be cut short).
 * Checks the invariants that must hold for any input; returns false and
 * prints why if one breaks. Shared by the libFuzzer target
 * (fuzz/fuzz_protocol.c) and the ctest driver (test_protocol_fuzz.c).
 */
#ifndef FUZZ_PROTOCOL_H
#define FUZZ_PROTOCOL_H

#include <stdio.h>

#include "protocol_fixture.h"

static bool fuzz_check_response(const uint8_t *req, size_t req_len, const uint8_t *resp)
{
    uint8_t status = resp[KB_PROTO_OFF_STATUS];
    uint8_t len = resp[KB_PROTO_OFF_LENGTH];

    if (resp[KB_PROTO_OFF_COMMAND] != req[KB_PROTO_OFF_COMMAND] ||
        resp[KB_PROTO_OFF_SEQUENCE] != req[KB_PROTO_OFF_SEQUENCE]) {
        fprintf(stderr, "response doesn't echo command and sequence\n");
        return false;
    }
    if (len > KB_PROTO_MAX_PAYLOAD || status > KB_PROTO_NOT_SUPPORTED) {
        fprintf(stderr, "bad response length %u or status %u\n", len, status);
        return false;
    }
    if (status != KB_PROTO_OK && len != 0) {
        fprintf(stderr, "error response with a payload\n");
        return false;
    }
    for (size_t i = KB_PROTO_HEADER_SIZE + len; i < KB_PROTO_REPORT_SIZE; i++) {
        if (resp[i] != 0) {
            fprintf(stderr, "padding not zero at %zu\n", i);
            return false;
        }
    }
    (void)req_len;
    return true;
}

static bool fuzz_protocol_one(const uint8_t *data, size_t size)
{
    fx_init();
    while (size > 0) {
        size_t n = data[0] % (KB_PROTO_REPORT_SIZE + 1u);
        data++;
        size--;
        if (n > size) {
            n = size;
        }
        /* The firmware copies each report into a full-size buffer. */
        uint8_t req[KB_PROTO_REPORT_SIZE] = {0};
        uint8_t resp[KB_PROTO_REPORT_SIZE];
        memcpy(req, data, n);
        bool answered = kb_protocol_handle(&fx_proto, req, n, resp);
        if (answered != (n >= KB_PROTO_HEADER_SIZE)) {
            fprintf(stderr, "answered a %zu-byte frame: %d\n", n, answered);
            return false;
        }
        if (answered && !fuzz_check_response(req, n, resp)) {
            return false;
        }
        if (!fx_keymap_valid()) {
            fprintf(stderr, "the keymap holds an invalid keycode\n");
            return false;
        }
        if (!fx_features_valid()) {
            fprintf(stderr, "the advanced-key tables are invalid\n");
            return false;
        }
        if (!kb_settings_valid(&fx.settings) || fx.indicators.battery_low_pct > 100) {
            fprintf(stderr, "the settings are invalid\n");
            return false;
        }
        if (fx.locks != fx.unlocks) {
            fprintf(stderr, "lock and unlock calls don't pair\n");
            return false;
        }
        data += n;
        size -= n;
    }
    return true;
}

#endif /* FUZZ_PROTOCOL_H */
