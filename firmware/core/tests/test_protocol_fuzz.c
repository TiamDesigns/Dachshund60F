/*
 * Randomised protocol frames with a fixed seed, so every ctest run (with
 * any compiler) covers the parser. Frames are mostly well-formed (a real
 * command, the right version, a plausible length) so they reach the
 * handlers, with random corruption on top. CI also runs the libFuzzer
 * target in fuzz/ with clang.
 */
#include "fuzz_protocol.h"
#include "kb_test.h"

#define RUNS 20000
#define MAX_INPUT 512

static uint32_t rng = 0x2545F491u;

static uint32_t next(void)
{
    /* xorshift32 */
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

static const uint8_t commands[] = {
    KB_PROTO_CMD_HANDSHAKE,       KB_PROTO_CMD_REBOOT,          KB_PROTO_CMD_IDENTIFY,
    KB_PROTO_CMD_GET_SERIAL,
    KB_PROTO_CMD_GET_KEY,         KB_PROTO_CMD_SET_KEY,         KB_PROTO_CMD_GET_KEYMAP_CHUNK,
    KB_PROTO_CMD_SET_KEYMAP_CHUNK, KB_PROTO_CMD_SAVE,           KB_PROTO_CMD_REVERT,
    KB_PROTO_CMD_FACTORY_RESET,
};

/* Appends one frame (length byte, then the frame) to buf; returns bytes written. */
static size_t make_frame(uint8_t *buf, size_t room)
{
    uint8_t frame[KB_PROTO_REPORT_SIZE];
    size_t n = KB_PROTO_REPORT_SIZE;

    for (size_t i = 0; i < sizeof(frame); i++) {
        frame[i] = (uint8_t)next();
    }
    frame[KB_PROTO_OFF_COMMAND] = commands[next() % sizeof(commands)];
    frame[KB_PROTO_OFF_VERSION] = KB_PROTO_VERSION;
    frame[KB_PROTO_OFF_LENGTH] = (uint8_t)(next() % 8 == 0 ? next() : next() % 62);
    /* Small indices hit the keymap; keycodes from the real ranges pass validation. */
    for (size_t i = KB_PROTO_HEADER_SIZE; i < KB_PROTO_HEADER_SIZE + 3; i++) {
        if (next() % 2) {
            frame[i] = (uint8_t)(next() % 6);
        }
    }
    if (next() % 4 == 0) {
        frame[next() % sizeof(frame)] = (uint8_t)next(); /* corrupt one byte */
    }
    if (next() % 8 == 0) {
        n = next() % (KB_PROTO_REPORT_SIZE + 1); /* short frame */
    }

    if (n + 1 > room) {
        return 0;
    }
    buf[0] = (uint8_t)n;
    memcpy(&buf[1], frame, n);
    return n + 1;
}

static void test_random_frames(void)
{
    uint8_t input[MAX_INPUT];
    int failures = 0;

    for (int run = 0; run < RUNS && failures < 5; run++) {
        size_t len = 0;
        size_t frames = 1 + next() % 8;
        for (size_t f = 0; f < frames; f++) {
            size_t w = make_frame(&input[len], sizeof(input) - len);
            if (w == 0) {
                break;
            }
            len += w;
        }
        if (!fuzz_protocol_one(input, len)) {
            fprintf(stderr, "failed on run %d\n", run);
            failures++;
        }
    }
    KB_CHECK_EQ(failures, 0);
}

static void test_pure_noise(void)
{
    uint8_t input[MAX_INPUT];
    for (int run = 0; run < RUNS; run++) {
        size_t len = next() % sizeof(input);
        for (size_t i = 0; i < len; i++) {
            input[i] = (uint8_t)next();
        }
        if (!fuzz_protocol_one(input, len)) {
            KB_CHECK(false);
            break;
        }
    }
}

int main(void)
{
    KB_RUN(test_random_frames);
    KB_RUN(test_pure_noise);
    return KB_TEST_RESULT();
}
