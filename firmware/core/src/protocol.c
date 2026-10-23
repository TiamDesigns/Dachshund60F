#include "kb/protocol.h"

#include <string.h>

#include "kb/keycodes.h"

/* Response payload goes here; returns its length and sets *status. */
typedef size_t (*handler_fn)(const struct kb_protocol *p, const uint8_t *in, size_t len,
                             uint8_t *out, uint8_t *status);

static void lock(const struct kb_protocol *p)
{
    if (p->ops && p->ops->lock) {
        p->ops->lock(p->ctx);
    }
}

static void unlock(const struct kb_protocol *p)
{
    if (p->ops && p->ops->unlock) {
        p->ops->unlock(p->ctx);
    }
}

static void copy_text(char *dst, size_t size, const char *src)
{
    memset(dst, 0, size);
    if (src) {
        size_t n = 0;
        while (n + 1 < size && src[n] != '\0') {
            dst[n] = src[n];
            n++;
        }
    }
}

static size_t keymap_total(const struct kb_keymap *km)
{
    return kb_keymap_size(km->layers, km->rows, km->cols);
}

static size_t h_handshake(const struct kb_protocol *p, const uint8_t *in, size_t len,
                          uint8_t *out, uint8_t *status)
{
    (void)in;
    if (len != 0) {
        *status = KB_PROTO_BAD_LENGTH;
        return 0;
    }

    struct kb_proto_handshake_resp r;
    memset(&r, 0, sizeof(r));
    r.protocol_version = KB_PROTO_VERSION;
    r.firmware_major = p->firmware_major;
    r.firmware_minor = p->firmware_minor;
    r.firmware_patch = p->firmware_patch;
    copy_text(r.build, sizeof(r.build), p->build);
    r.capabilities = p->capabilities;
    r.rows = p->keymap->rows;
    r.cols = p->keymap->cols;
    r.layers = p->keymap->layers;
    r.encoders = p->encoders;
    r.macro_buffer_size = p->macro_buffer_size;
    copy_text(r.model, sizeof(r.model), p->model);
    return kb_proto_handshake_resp_encode(&r, out);
}

static size_t h_reboot(const struct kb_protocol *p, const uint8_t *in, size_t len, uint8_t *out,
                       uint8_t *status)
{
    (void)out;
    struct kb_proto_reboot_req r;
    if (kb_proto_reboot_req_decode(in, len, &r) != 0) {
        *status = KB_PROTO_BAD_LENGTH;
    } else if (r.mode > 1) {
        *status = KB_PROTO_BAD_VALUE;
    } else if (!p->ops || !p->ops->reboot) {
        *status = KB_PROTO_NOT_SUPPORTED;
    } else {
        p->ops->reboot(r.mode == 1, p->ctx);
    }
    return 0;
}

static size_t h_identify(const struct kb_protocol *p, const uint8_t *in, size_t len,
                         uint8_t *out, uint8_t *status)
{
    (void)out;
    struct kb_proto_identify_req r;
    if (kb_proto_identify_req_decode(in, len, &r) != 0) {
        *status = KB_PROTO_BAD_LENGTH;
    } else if (r.seconds < 1 || r.seconds > 30) {
        *status = KB_PROTO_BAD_VALUE;
    } else if (!p->ops || !p->ops->identify) {
        *status = KB_PROTO_NOT_SUPPORTED;
    } else {
        p->ops->identify(r.seconds, p->ctx);
    }
    return 0;
}

static size_t h_get_serial(const struct kb_protocol *p, const uint8_t *in, size_t len,
                           uint8_t *out, uint8_t *status)
{
    (void)in;
    if (len != 0) {
        *status = KB_PROTO_BAD_LENGTH;
        return 0;
    }
    struct kb_proto_get_serial_resp r;
    copy_text(r.serial, sizeof(r.serial), p->serial);
    return kb_proto_get_serial_resp_encode(&r, out);
}

static bool key_in_range(const struct kb_keymap *km, uint8_t layer, uint8_t row, uint8_t col)
{
    return layer < km->layers && row < km->rows && col < km->cols;
}

static size_t h_get_key(const struct kb_protocol *p, const uint8_t *in, size_t len,
                        uint8_t *out, uint8_t *status)
{
    struct kb_proto_get_key_req r;
    if (kb_proto_get_key_req_decode(in, len, &r) != 0) {
        *status = KB_PROTO_BAD_LENGTH;
        return 0;
    }
    if (!key_in_range(p->keymap, r.layer, r.row, r.col)) {
        *status = KB_PROTO_OUT_OF_RANGE;
        return 0;
    }

    struct kb_proto_get_key_resp resp;
    lock(p);
    resp.keycode = kb_keymap_get(p->keymap, r.layer, r.row, r.col);
    unlock(p);
    return kb_proto_get_key_resp_encode(&resp, out);
}

static size_t h_set_key(const struct kb_protocol *p, const uint8_t *in, size_t len,
                        uint8_t *out, uint8_t *status)
{
    (void)out;
    struct kb_proto_set_key_req r;
    if (kb_proto_set_key_req_decode(in, len, &r) != 0) {
        *status = KB_PROTO_BAD_LENGTH;
        return 0;
    }
    if (!key_in_range(p->keymap, r.layer, r.row, r.col)) {
        *status = KB_PROTO_OUT_OF_RANGE;
        return 0;
    }
    if (!kb_keycode_is_valid(r.keycode, p->keymap->layers)) {
        *status = KB_PROTO_BAD_VALUE;
        return 0;
    }

    lock(p);
    int err = kb_keymap_set(p->keymap, r.layer, r.row, r.col, r.keycode);
    unlock(p);
    if (err) {
        *status = KB_PROTO_BAD_VALUE;
    }
    return 0;
}

static size_t h_get_keymap_chunk(const struct kb_protocol *p, const uint8_t *in, size_t len,
                                 uint8_t *out, uint8_t *status)
{
    struct kb_proto_get_keymap_chunk_req r;
    if (kb_proto_get_keymap_chunk_req_decode(in, len, &r) != 0) {
        *status = KB_PROTO_BAD_LENGTH;
        return 0;
    }
    if (r.count > KB_PROTO_GET_KEYMAP_CHUNK_RESP_MAX_KEYCODES ||
        (size_t)r.offset + r.count > keymap_total(p->keymap)) {
        *status = KB_PROTO_OUT_OF_RANGE;
        return 0;
    }

    struct kb_proto_get_keymap_chunk_resp resp;
    resp.offset = r.offset;
    resp.keycodes_count = r.count;
    lock(p);
    memcpy(resp.keycodes, &p->keymap->keys[r.offset], r.count * sizeof(kb_keycode_t));
    unlock(p);
    return kb_proto_get_keymap_chunk_resp_encode(&resp, out);
}

static size_t h_set_keymap_chunk(const struct kb_protocol *p, const uint8_t *in, size_t len,
                                 uint8_t *out, uint8_t *status)
{
    (void)out;
    struct kb_proto_set_keymap_chunk_req r;
    if (kb_proto_set_keymap_chunk_req_decode(in, len, &r) != 0) {
        *status = KB_PROTO_BAD_LENGTH;
        return 0;
    }
    if ((size_t)r.offset + r.keycodes_count > keymap_total(p->keymap)) {
        *status = KB_PROTO_OUT_OF_RANGE;
        return 0;
    }
    /* All or nothing: check every keycode before writing any. */
    for (size_t i = 0; i < r.keycodes_count; i++) {
        if (!kb_keycode_is_valid(r.keycodes[i], p->keymap->layers)) {
            *status = KB_PROTO_BAD_VALUE;
            return 0;
        }
    }

    lock(p);
    memcpy(&p->keymap->keys[r.offset], r.keycodes, r.keycodes_count * sizeof(kb_keycode_t));
    unlock(p);
    return 0;
}

static size_t run_op(const struct kb_protocol *p, size_t len, uint8_t *status,
                     uint8_t (*op)(void *ctx))
{
    if (len != 0) {
        *status = KB_PROTO_BAD_LENGTH;
    } else if (!op) {
        *status = KB_PROTO_NOT_SUPPORTED;
    } else {
        *status = op(p->ctx);
    }
    return 0;
}

static size_t h_save(const struct kb_protocol *p, const uint8_t *in, size_t len, uint8_t *out,
                     uint8_t *status)
{
    (void)in;
    (void)out;
    return run_op(p, len, status, p->ops ? p->ops->save : NULL);
}

static size_t h_revert(const struct kb_protocol *p, const uint8_t *in, size_t len,
                       uint8_t *out, uint8_t *status)
{
    (void)in;
    (void)out;
    return run_op(p, len, status, p->ops ? p->ops->revert : NULL);
}

static size_t h_factory_reset(const struct kb_protocol *p, const uint8_t *in, size_t len,
                              uint8_t *out, uint8_t *status)
{
    (void)in;
    (void)out;
    return run_op(p, len, status, p->ops ? p->ops->factory_reset : NULL);
}

static handler_fn handler_for(uint8_t command)
{
    switch (command) {
    case KB_PROTO_CMD_HANDSHAKE: return h_handshake;
    case KB_PROTO_CMD_REBOOT: return h_reboot;
    case KB_PROTO_CMD_IDENTIFY: return h_identify;
    case KB_PROTO_CMD_GET_SERIAL: return h_get_serial;
    case KB_PROTO_CMD_GET_KEY: return h_get_key;
    case KB_PROTO_CMD_SET_KEY: return h_set_key;
    case KB_PROTO_CMD_GET_KEYMAP_CHUNK: return h_get_keymap_chunk;
    case KB_PROTO_CMD_SET_KEYMAP_CHUNK: return h_set_keymap_chunk;
    case KB_PROTO_CMD_SAVE: return h_save;
    case KB_PROTO_CMD_REVERT: return h_revert;
    case KB_PROTO_CMD_FACTORY_RESET: return h_factory_reset;
    default: return NULL;
    }
}

bool kb_protocol_handle(const struct kb_protocol *p, const uint8_t *req, size_t req_len,
                        uint8_t resp[KB_PROTO_REPORT_SIZE])
{
    memset(resp, 0, KB_PROTO_REPORT_SIZE);
    if (req_len < KB_PROTO_HEADER_SIZE) {
        return false;
    }
    if (req_len > KB_PROTO_REPORT_SIZE) {
        req_len = KB_PROTO_REPORT_SIZE;
    }

    uint8_t command = req[KB_PROTO_OFF_COMMAND];
    uint8_t len = req[KB_PROTO_OFF_LENGTH];
    uint8_t status = KB_PROTO_OK;
    size_t out_len = 0;
    resp[KB_PROTO_OFF_COMMAND] = command;
    resp[KB_PROTO_OFF_SEQUENCE] = req[KB_PROTO_OFF_SEQUENCE];

    handler_fn handler = handler_for(command);
    if (len > KB_PROTO_MAX_PAYLOAD || KB_PROTO_HEADER_SIZE + (size_t)len > req_len) {
        status = KB_PROTO_BAD_LENGTH;
    } else if (command != KB_PROTO_CMD_HANDSHAKE && req[KB_PROTO_OFF_VERSION] != KB_PROTO_VERSION) {
        status = KB_PROTO_VERSION_MISMATCH;
    } else if (!handler) {
        status = KB_PROTO_UNKNOWN_COMMAND;
    } else {
        out_len = handler(p, &req[KB_PROTO_HEADER_SIZE], len, &resp[KB_PROTO_HEADER_SIZE], &status);
    }

    if (status != KB_PROTO_OK) {
        /* Error responses carry no payload. */
        out_len = 0;
        memset(&resp[KB_PROTO_HEADER_SIZE], 0, KB_PROTO_MAX_PAYLOAD);
    }
    resp[KB_PROTO_OFF_LENGTH] = (uint8_t)out_len;
    resp[KB_PROTO_OFF_STATUS] = status;
    return true;
}
