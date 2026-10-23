#include "kb_test.h"
#include "protocol_fixture.h"

static uint8_t req[KB_PROTO_REPORT_SIZE];
static uint8_t resp[KB_PROTO_REPORT_SIZE];
static uint8_t seq;

/* Send a command with a payload and return the response status. */
static uint8_t call(uint8_t command, const uint8_t *payload, uint8_t len)
{
    memset(req, 0, sizeof(req));
    req[KB_PROTO_OFF_COMMAND] = command;
    req[KB_PROTO_OFF_SEQUENCE] = ++seq;
    req[KB_PROTO_OFF_LENGTH] = len;
    req[KB_PROTO_OFF_VERSION] = KB_PROTO_VERSION;
    if (len) {
        memcpy(&req[KB_PROTO_HEADER_SIZE], payload, len);
    }
    memset(resp, 0xAA, sizeof(resp));
    KB_CHECK(kb_protocol_handle(&fx_proto, req, sizeof(req), resp));
    KB_CHECK_EQ(resp[KB_PROTO_OFF_COMMAND], command);
    KB_CHECK_EQ(resp[KB_PROTO_OFF_SEQUENCE], seq);
    return resp[KB_PROTO_OFF_STATUS];
}

static const uint8_t *payload(void)
{
    return &resp[KB_PROTO_HEADER_SIZE];
}

static uint8_t payload_len(void)
{
    return resp[KB_PROTO_OFF_LENGTH];
}

static void test_handshake(void)
{
    fx_init();
    KB_CHECK_EQ(call(KB_PROTO_CMD_HANDSHAKE, NULL, 0), KB_PROTO_OK);
    struct kb_proto_handshake_resp h;
    KB_CHECK_EQ(kb_proto_handshake_resp_decode(payload(), payload_len(), &h), 0);
    KB_CHECK_EQ(h.protocol_version, KB_PROTO_VERSION);
    KB_CHECK_EQ(h.firmware_minor, 4);
    KB_CHECK_EQ(h.rows, FX_ROWS);
    KB_CHECK_EQ(h.cols, FX_COLS);
    KB_CHECK_EQ(h.layers, FX_LAYERS);
    KB_CHECK_EQ(h.encoders, 1);
    KB_CHECK_EQ(h.capabilities, KB_PROTO_CAP_KEYMAP | KB_PROTO_CAP_IDENTIFY);
    KB_CHECK(strcmp(h.model, "Dachshund60F") == 0);
    KB_CHECK(strcmp(h.build, "abc1234") == 0);
    /* The padding after the payload is zero. */
    for (size_t i = KB_PROTO_HEADER_SIZE + payload_len(); i < KB_PROTO_REPORT_SIZE; i++) {
        KB_CHECK_EQ(resp[i], 0);
    }
}

static void test_get_serial(void)
{
    fx_init();
    KB_CHECK_EQ(call(KB_PROTO_CMD_GET_SERIAL, NULL, 0), KB_PROTO_OK);
    struct kb_proto_get_serial_resp r;
    KB_CHECK_EQ(kb_proto_get_serial_resp_decode(payload(), payload_len(), &r), 0);
    KB_CHECK(strcmp(r.serial, "0123456789ABCDEF") == 0);
    uint8_t one = 0;
    KB_CHECK_EQ(call(KB_PROTO_CMD_GET_SERIAL, &one, 1), KB_PROTO_BAD_LENGTH);
}

static void test_version_mismatch(void)
{
    fx_init();
    uint8_t p[5] = {0, 0, 0, (uint8_t)KC_Z, 0};
    memset(req, 0, sizeof(req));
    req[KB_PROTO_OFF_COMMAND] = KB_PROTO_CMD_SET_KEY;
    req[KB_PROTO_OFF_LENGTH] = sizeof(p);
    req[KB_PROTO_OFF_VERSION] = KB_PROTO_VERSION + 1;
    memcpy(&req[KB_PROTO_HEADER_SIZE], p, sizeof(p));
    KB_CHECK(kb_protocol_handle(&fx_proto, req, sizeof(req), resp));
    KB_CHECK_EQ(resp[KB_PROTO_OFF_STATUS], KB_PROTO_VERSION_MISMATCH);
    KB_CHECK_EQ(fx.live[0], KC_A);

    /* Handshake still answers, so the app can say which versions differ. */
    req[KB_PROTO_OFF_COMMAND] = KB_PROTO_CMD_HANDSHAKE;
    req[KB_PROTO_OFF_LENGTH] = 0;
    KB_CHECK(kb_protocol_handle(&fx_proto, req, sizeof(req), resp));
    KB_CHECK_EQ(resp[KB_PROTO_OFF_STATUS], KB_PROTO_OK);
}

static void test_framing_errors(void)
{
    fx_init();
    /* Too short to have a header: no answer. */
    KB_CHECK(!kb_protocol_handle(&fx_proto, req, 3, resp));

    KB_CHECK_EQ(call(0xEE, NULL, 0), KB_PROTO_UNKNOWN_COMMAND);
    KB_CHECK_EQ(payload_len(), 0);

    /* Length beyond the payload area. */
    memset(req, 0, sizeof(req));
    req[KB_PROTO_OFF_COMMAND] = KB_PROTO_CMD_GET_KEY;
    req[KB_PROTO_OFF_LENGTH] = KB_PROTO_MAX_PAYLOAD + 1;
    req[KB_PROTO_OFF_VERSION] = KB_PROTO_VERSION;
    KB_CHECK(kb_protocol_handle(&fx_proto, req, sizeof(req), resp));
    KB_CHECK_EQ(resp[KB_PROTO_OFF_STATUS], KB_PROTO_BAD_LENGTH);

    /* Length beyond what was received (a short report). */
    req[KB_PROTO_OFF_LENGTH] = 3;
    KB_CHECK(kb_protocol_handle(&fx_proto, req, KB_PROTO_HEADER_SIZE + 2, resp));
    KB_CHECK_EQ(resp[KB_PROTO_OFF_STATUS], KB_PROTO_BAD_LENGTH);

    /* Wrong payload length for the command. */
    uint8_t two[2] = {0, 0};
    KB_CHECK_EQ(call(KB_PROTO_CMD_GET_KEY, two, 2), KB_PROTO_BAD_LENGTH);
    KB_CHECK_EQ(call(KB_PROTO_CMD_SAVE, two, 1), KB_PROTO_BAD_LENGTH);
    KB_CHECK_EQ(fx.saves, 0);
}

static void test_get_and_set_key(void)
{
    fx_init();
    uint8_t get[3] = {1, 1, 3};
    KB_CHECK_EQ(call(KB_PROTO_CMD_GET_KEY, get, 3), KB_PROTO_OK);
    KB_CHECK_EQ(payload_len(), 2);
    KB_CHECK_EQ(payload()[0] | (payload()[1] << 8), QK_BOOT);

    uint8_t set[5] = {0, 0, 1, (uint8_t)(LSFT(KC_1) & 0xFF), (uint8_t)(LSFT(KC_1) >> 8)};
    KB_CHECK_EQ(call(KB_PROTO_CMD_SET_KEY, set, 5), KB_PROTO_OK);
    KB_CHECK_EQ(kb_keymap_get(&fx.keymap, 0, 0, 1), LSFT(KC_1));
    KB_CHECK_EQ(fx.locks, fx.unlocks);
    KB_CHECK(fx.locks >= 2);

    /* Out of range indices */
    uint8_t bad_layer[3] = {FX_LAYERS, 0, 0};
    uint8_t bad_row[3] = {0, FX_ROWS, 0};
    uint8_t bad_col[3] = {0, 0, FX_COLS};
    KB_CHECK_EQ(call(KB_PROTO_CMD_GET_KEY, bad_layer, 3), KB_PROTO_OUT_OF_RANGE);
    KB_CHECK_EQ(call(KB_PROTO_CMD_GET_KEY, bad_row, 3), KB_PROTO_OUT_OF_RANGE);
    KB_CHECK_EQ(call(KB_PROTO_CMD_GET_KEY, bad_col, 3), KB_PROTO_OUT_OF_RANGE);
    uint8_t set_bad[5] = {0, 0, 255, KC_A, 0};
    KB_CHECK_EQ(call(KB_PROTO_CMD_SET_KEY, set_bad, 5), KB_PROTO_OUT_OF_RANGE);

    /* Invalid keycodes: undefined, and a layer key past the layer count. */
    uint8_t undefined[5] = {0, 0, 0, 0xFF, 0xFF};
    KB_CHECK_EQ(call(KB_PROTO_CMD_SET_KEY, undefined, 5), KB_PROTO_BAD_VALUE);
    uint8_t mo9[5] = {0, 0, 0, (uint8_t)(MO(9) & 0xFF), (uint8_t)(MO(9) >> 8)};
    KB_CHECK_EQ(call(KB_PROTO_CMD_SET_KEY, mo9, 5), KB_PROTO_BAD_VALUE);
    KB_CHECK_EQ(fx.live[0], KC_A);
}

static void test_keymap_chunks(void)
{
    fx_init();
    /* Read the whole keymap in chunks of up to 28. */
    kb_keycode_t all[FX_KEYS];
    for (uint16_t off = 0; off < FX_KEYS;) {
        uint8_t n = (uint8_t)(FX_KEYS - off > 28 ? 28 : FX_KEYS - off);
        uint8_t p[3] = {(uint8_t)off, (uint8_t)(off >> 8), n};
        KB_CHECK_EQ(call(KB_PROTO_CMD_GET_KEYMAP_CHUNK, p, 3), KB_PROTO_OK);
        struct kb_proto_get_keymap_chunk_resp r;
        KB_CHECK_EQ(kb_proto_get_keymap_chunk_resp_decode(payload(), payload_len(), &r), 0);
        KB_CHECK_EQ(r.offset, off);
        KB_CHECK_EQ(r.keycodes_count, n);
        memcpy(&all[off], r.keycodes, n * sizeof(kb_keycode_t));
        off = (uint16_t)(off + n);
    }
    KB_CHECK(memcmp(all, fx_default, sizeof(all)) == 0);

    /* Past the end, or more than fits */
    uint8_t past[3] = {(uint8_t)(FX_KEYS - 1), 0, 2};
    KB_CHECK_EQ(call(KB_PROTO_CMD_GET_KEYMAP_CHUNK, past, 3), KB_PROTO_OUT_OF_RANGE);
    uint8_t too_many[3] = {0, 0, 29};
    KB_CHECK_EQ(call(KB_PROTO_CMD_GET_KEYMAP_CHUNK, too_many, 3), KB_PROTO_OUT_OF_RANGE);
    uint8_t huge[3] = {0xFF, 0xFF, 1};
    KB_CHECK_EQ(call(KB_PROTO_CMD_GET_KEYMAP_CHUNK, huge, 3), KB_PROTO_OUT_OF_RANGE);

    /* Write keys 10-12 (layer 1, row 0, cols 0-2). */
    struct kb_proto_set_keymap_chunk_req w = {.offset = 10, .keycodes_count = 3,
                                              .keycodes = {KC_X, KC_Y, TG(2)}};
    uint8_t buf[KB_PROTO_MAX_PAYLOAD];
    size_t n = kb_proto_set_keymap_chunk_req_encode(&w, buf);
    KB_CHECK_EQ(call(KB_PROTO_CMD_SET_KEYMAP_CHUNK, buf, (uint8_t)n), KB_PROTO_OK);
    KB_CHECK_EQ(kb_keymap_get(&fx.keymap, 1, 0, 0), KC_X);
    KB_CHECK_EQ(kb_keymap_get(&fx.keymap, 1, 0, 2), TG(2));

    /* One bad keycode: nothing is written. */
    w.keycodes[0] = KC_Z;
    w.keycodes[2] = 0xFFFF;
    n = kb_proto_set_keymap_chunk_req_encode(&w, buf);
    KB_CHECK_EQ(call(KB_PROTO_CMD_SET_KEYMAP_CHUNK, buf, (uint8_t)n), KB_PROTO_BAD_VALUE);
    KB_CHECK_EQ(kb_keymap_get(&fx.keymap, 1, 0, 0), KC_X);

    /* Running past the end: nothing is written. */
    w.offset = FX_KEYS - 2;
    w.keycodes[2] = KC_Z;
    n = kb_proto_set_keymap_chunk_req_encode(&w, buf);
    KB_CHECK_EQ(call(KB_PROTO_CMD_SET_KEYMAP_CHUNK, buf, (uint8_t)n), KB_PROTO_OUT_OF_RANGE);
    KB_CHECK_EQ(fx.live[FX_KEYS - 1], fx_default[FX_LAYERS - 1][FX_ROWS - 1][FX_COLS - 1]);

    /* An odd payload length can't be a list of keycodes. */
    KB_CHECK_EQ(call(KB_PROTO_CMD_SET_KEYMAP_CHUNK, buf, 5), KB_PROTO_BAD_LENGTH);
}

static void test_save_revert_reset(void)
{
    fx_init();
    uint8_t set[5] = {0, 0, 0, KC_Q, 0};
    KB_CHECK_EQ(call(KB_PROTO_CMD_SET_KEY, set, 5), KB_PROTO_OK);
    KB_CHECK_EQ(call(KB_PROTO_CMD_REVERT, NULL, 0), KB_PROTO_OK);
    KB_CHECK_EQ(fx.live[0], KC_A);

    KB_CHECK_EQ(call(KB_PROTO_CMD_SET_KEY, set, 5), KB_PROTO_OK);
    KB_CHECK_EQ(call(KB_PROTO_CMD_SAVE, NULL, 0), KB_PROTO_OK);
    KB_CHECK_EQ(fx.saved[0], KC_Q);

    /* A failing save reports its status. */
    fx.save_status = KB_PROTO_STORAGE_FULL;
    KB_CHECK_EQ(call(KB_PROTO_CMD_SAVE, NULL, 0), KB_PROTO_STORAGE_FULL);
    KB_CHECK_EQ(payload_len(), 0);

    KB_CHECK_EQ(call(KB_PROTO_CMD_FACTORY_RESET, NULL, 0), KB_PROTO_OK);
    KB_CHECK_EQ(fx.live[0], KC_A);
    KB_CHECK_EQ(fx.saved[0], KC_A);
}

static void test_device_control(void)
{
    fx_init();
    uint8_t mode = 1;
    KB_CHECK_EQ(call(KB_PROTO_CMD_REBOOT, &mode, 1), KB_PROTO_OK);
    KB_CHECK_EQ(fx.reboots, 1);
    KB_CHECK(fx.reboot_bootloader);
    mode = 2;
    KB_CHECK_EQ(call(KB_PROTO_CMD_REBOOT, &mode, 1), KB_PROTO_BAD_VALUE);
    KB_CHECK_EQ(fx.reboots, 1);

    uint8_t secs = 5;
    KB_CHECK_EQ(call(KB_PROTO_CMD_IDENTIFY, &secs, 1), KB_PROTO_OK);
    KB_CHECK_EQ(fx.identify_seconds, 5);
    secs = 0;
    KB_CHECK_EQ(call(KB_PROTO_CMD_IDENTIFY, &secs, 1), KB_PROTO_BAD_VALUE);
    secs = 31;
    KB_CHECK_EQ(call(KB_PROTO_CMD_IDENTIFY, &secs, 1), KB_PROTO_BAD_VALUE);

    /* Without ops, the commands say so instead of crashing. */
    static const struct kb_protocol_ops none = {0};
    fx_proto.ops = &none;
    secs = 5;
    KB_CHECK_EQ(call(KB_PROTO_CMD_IDENTIFY, &secs, 1), KB_PROTO_NOT_SUPPORTED);
    KB_CHECK_EQ(call(KB_PROTO_CMD_SAVE, NULL, 0), KB_PROTO_NOT_SUPPORTED);
    mode = 0;
    KB_CHECK_EQ(call(KB_PROTO_CMD_REBOOT, &mode, 1), KB_PROTO_NOT_SUPPORTED);
}

static void test_codec_round_trip(void)
{
    /* Every generated encoder and decoder agree, including text padding. */
    struct kb_proto_handshake_resp h = {0}, h2;
    h.protocol_version = 1;
    h.capabilities = 0x89ABCDEFu;
    h.macro_buffer_size = 0x1234;
    memcpy(h.model, "0123456789abcdef", 16); /* exactly 16: no terminator on the wire */
    uint8_t buf[KB_PROTO_MAX_PAYLOAD];
    size_t n = kb_proto_handshake_resp_encode(&h, buf);
    KB_CHECK_EQ(n, KB_PROTO_HANDSHAKE_RESP_SIZE);
    KB_CHECK_EQ(kb_proto_handshake_resp_decode(buf, n, &h2), 0);
    KB_CHECK_EQ(h2.capabilities, 0x89ABCDEFu);
    KB_CHECK_EQ(h2.macro_buffer_size, 0x1234);
    KB_CHECK(strcmp(h2.model, "0123456789abcdef") == 0);
    KB_CHECK_EQ(kb_proto_handshake_resp_decode(buf, n - 1, &h2), -1);

    struct kb_proto_set_keymap_chunk_req c = {.offset = 300, .keycodes_count = 28};
    for (int i = 0; i < 28; i++) {
        c.keycodes[i] = (uint16_t)(0x100 * i + 7);
    }
    n = kb_proto_set_keymap_chunk_req_encode(&c, buf);
    KB_CHECK_EQ(n, 58);
    struct kb_proto_set_keymap_chunk_req c2;
    KB_CHECK_EQ(kb_proto_set_keymap_chunk_req_decode(buf, n, &c2), 0);
    KB_CHECK_EQ(c2.offset, 300);
    KB_CHECK_EQ(c2.keycodes_count, 28);
    KB_CHECK_EQ(c2.keycodes[27], 0x100 * 27 + 7);
    /* 29 keycodes don't fit the message. */
    KB_CHECK_EQ(kb_proto_set_keymap_chunk_req_decode(buf, 60, &c2), -1);
}

int main(void)
{
    KB_RUN(test_handshake);
    KB_RUN(test_get_serial);
    KB_RUN(test_version_mismatch);
    KB_RUN(test_framing_errors);
    KB_RUN(test_get_and_set_key);
    KB_RUN(test_keymap_chunks);
    KB_RUN(test_save_revert_reset);
    KB_RUN(test_device_control);
    KB_RUN(test_codec_round_trip);
    return KB_TEST_RESULT();
}
