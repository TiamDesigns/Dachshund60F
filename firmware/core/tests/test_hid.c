#include "kb/hid.h"
#include "kb/protocol_gen.h"
#include "kb_test.h"

#include <string.h>

/*
 * A small HID report descriptor parser: input and output bits per report
 * ID, collection balance, and whether any report ID is used.
 */
struct desc_info {
    unsigned input_bits[256];
    unsigned output_bits[256];
    int depth;
    int max_depth_errors;
    bool uses_ids;
    bool ok;
};

static void parse(const uint8_t *d, size_t len, struct desc_info *info)
{
    unsigned size = 0, count = 0, id = 0;

    memset(info, 0, sizeof(*info));
    info->ok = true;
    for (size_t i = 0; i < len;) {
        uint8_t prefix = d[i];
        unsigned n = prefix & 3u;
        if (n == 3u) {
            n = 4u;
        }
        if (prefix == 0xFEu || i + 1u + n > len) {
            info->ok = false; /* long items are not used; truncated item */
            return;
        }
        uint32_t v = 0;
        for (unsigned b = 0; b < n; b++) {
            v |= (uint32_t)d[i + 1u + b] << (8u * b);
        }
        switch (prefix & 0xFCu) {
        case 0x74: size = v; break;               /* Report Size */
        case 0x94: count = v; break;              /* Report Count */
        case 0x84: id = v; info->uses_ids = true; break; /* Report ID */
        case 0x80: info->input_bits[id] += size * count; break;
        case 0x90: info->output_bits[id] += size * count; break;
        case 0xA0: info->depth++; break;          /* Collection */
        case 0xC0:                                /* End Collection */
            if (--info->depth < 0) {
                info->max_depth_errors++;
            }
            break;
        default: break;
        }
        i += 1u + n;
    }
}

static void test_keyboard_descriptor(void)
{
    struct desc_info info;
    parse(kb_hid_keyboard_desc, kb_hid_keyboard_desc_len, &info);

    KB_CHECK(info.ok);
    KB_CHECK_EQ(info.depth, 0);
    KB_CHECK_EQ(info.max_depth_errors, 0);
    /* Boot interface: no report IDs, 8-byte input, 1-byte LED output. */
    KB_CHECK(!info.uses_ids);
    KB_CHECK_EQ(info.input_bits[0], KB_HID_BOOT_REPORT_SIZE * 8u);
    KB_CHECK_EQ(info.output_bits[0], 8);
}

static void test_extra_descriptor(void)
{
    struct desc_info info;
    parse(kb_hid_extra_desc, kb_hid_extra_desc_len, &info);

    KB_CHECK(info.ok);
    KB_CHECK_EQ(info.depth, 0);
    KB_CHECK_EQ(info.max_depth_errors, 0);
    KB_CHECK(info.uses_ids);
    KB_CHECK_EQ(info.input_bits[0], 0);
    /* Each report's bits plus its ID byte equal the packed size. */
    KB_CHECK_EQ(info.input_bits[KB_HID_ID_NKRO] / 8u + 1u, KB_HID_NKRO_REPORT_SIZE);
    KB_CHECK_EQ(info.input_bits[KB_HID_ID_NKRO] % 8u, 0);
    KB_CHECK_EQ(info.input_bits[KB_HID_ID_CONSUMER] / 8u + 1u, KB_HID_CONSUMER_REPORT_SIZE);
    KB_CHECK_EQ(info.input_bits[KB_HID_ID_SYSTEM] / 8u + 1u, KB_HID_SYSTEM_REPORT_SIZE);
    KB_CHECK_EQ(info.input_bits[KB_HID_ID_MOUSE] / 8u + 1u, KB_HID_MOUSE_REPORT_SIZE);
    KB_CHECK_EQ(info.input_bits[KB_HID_ID_MOUSE] % 8u, 0);
    for (unsigned i = 0; i < 256; i++) {
        KB_CHECK_EQ(info.output_bits[i], 0);
    }
    KB_CHECK(KB_HID_EXTRA_REPORT_MAX <= 64u); /* one full-speed packet */
}

static void test_mouse_report(void)
{
    struct kb_mouse_report m = {.buttons = 0x21, .x = -5, .y = 127, .wheel = -1, .pan = 1};
    uint8_t out[KB_HID_MOUSE_REPORT_SIZE];
    kb_hid_pack_mouse(&m, out);
    KB_CHECK_EQ(out[0], KB_HID_ID_MOUSE);
    KB_CHECK_EQ(out[1], 0x01); /* five buttons only */
    KB_CHECK_EQ(out[2], 0xFB);
    KB_CHECK_EQ(out[3], 0x7F);
    KB_CHECK_EQ(out[4], 0xFF);
    KB_CHECK_EQ(out[5], 0x01);
}

static void test_config_descriptor(void)
{
    struct desc_info info;
    parse(kb_hid_config_desc, kb_hid_config_desc_len, &info);

    KB_CHECK(info.ok);
    KB_CHECK_EQ(info.depth, 0);
    KB_CHECK(!info.uses_ids);
    KB_CHECK_EQ(info.input_bits[0], KB_PROTO_REPORT_SIZE * 8u);
    KB_CHECK_EQ(info.output_bits[0], KB_PROTO_REPORT_SIZE * 8u);
    /* Vendor-defined usage page, as protocol.yaml says. */
    KB_CHECK_EQ(kb_hid_config_desc[0], 0x06);
    KB_CHECK_EQ(kb_hid_config_desc[1] | (kb_hid_config_desc[2] << 8), KB_PROTO_HID_USAGE_PAGE);
    KB_CHECK(KB_PROTO_HID_USAGE_PAGE >= 0xFF00u);
}

static void test_pack(void)
{
    struct kb_hid_state hid;
    memset(&hid, 0, sizeof(hid));
    hid.mods = 0x22; /* LShift + RShift */
    hid.keys[KC_A >> 3] |= 1u << (KC_A & 7u);
    hid.keys[0xA4 >> 3] |= 1u << (0xA4 & 7u);
    hid.consumer = 0x0E9;
    hid.system = 0x82;

    uint8_t nkro[KB_HID_NKRO_REPORT_SIZE];
    kb_hid_pack_nkro(&hid, nkro);
    KB_CHECK_EQ(nkro[0], KB_HID_ID_NKRO);
    KB_CHECK_EQ(nkro[1], 0x22);
    KB_CHECK_EQ(nkro[2 + (KC_A >> 3)], 1u << (KC_A & 7u));
    KB_CHECK_EQ(nkro[2 + (0xA4 >> 3)], 1u << (0xA4 & 7u));

    uint8_t c[KB_HID_CONSUMER_REPORT_SIZE];
    kb_hid_pack_consumer(&hid, c);
    KB_CHECK_EQ(c[0], KB_HID_ID_CONSUMER);
    KB_CHECK_EQ(c[1], 0xE9);
    KB_CHECK_EQ(c[2], 0x00);

    uint8_t s[KB_HID_SYSTEM_REPORT_SIZE];
    kb_hid_pack_system(&hid, s);
    KB_CHECK_EQ(s[0], KB_HID_ID_SYSTEM);
    KB_CHECK_EQ(s[1], 0x82);
    KB_CHECK_EQ(s[2], 0x00);
}

static void ignore_report(const struct kb_hid_state *hid, void *ctx)
{
    (void)hid;
    (void)ctx;
}

/* Reporter: record what it sends. */

#define MAX_SENT 16

struct sent {
    enum kb_hid_report which;
    uint8_t data[KB_HID_EXTRA_REPORT_MAX];
    size_t len;
};

static struct sent sent[MAX_SENT];
static int n_sent;

static void on_send(enum kb_hid_report which, const uint8_t *data, size_t len, void *ctx)
{
    (void)ctx;
    if (n_sent < MAX_SENT) {
        sent[n_sent].which = which;
        memcpy(sent[n_sent].data, data, len);
        sent[n_sent].len = len;
    }
    n_sent++;
}

static void reset_sent(void)
{
    n_sent = 0;
    memset(sent, 0, sizeof(sent));
}

static void press(struct kb_hid_state *hid, uint8_t usage)
{
    hid->keys[usage >> 3] |= (uint8_t)(1u << (usage & 7u));
    for (unsigned i = 0; i < KB_BOOT_KEYS; i++) {
        if (hid->boot_keys[i] == 0) {
            hid->boot_keys[i] = usage;
            break;
        }
    }
}

static bool nkro_has(const struct sent *s, uint8_t usage)
{
    return (s->data[2 + (usage >> 3)] >> (usage & 7u)) & 1u;
}

static void test_reporter_nkro_path(void)
{
    struct kb_hid_reporter r;
    struct kb_hid_state hid;
    memset(&hid, 0, sizeof(hid));
    reset_sent();
    kb_hid_reporter_init(&r, true, on_send, NULL);
    KB_CHECK(kb_hid_reporter_nkro_active(&r));

    /* Nothing changed: nothing sent. */
    kb_hid_reporter_update(&r, &hid);
    KB_CHECK_EQ(n_sent, 0);

    /* Seven keys and a modifier all reach NKRO; the boot report stays empty. */
    for (uint8_t k = KC_A; k < KC_A + 7; k++) {
        press(&hid, k);
    }
    hid.mods = 0x01;
    kb_hid_reporter_update(&r, &hid);
    KB_CHECK_EQ(n_sent, 1);
    KB_CHECK_EQ(sent[0].which, KB_HID_REPORT_NKRO);
    KB_CHECK_EQ(sent[0].data[1], 0x01);
    for (uint8_t k = KC_A; k < KC_A + 7; k++) {
        KB_CHECK(nkro_has(&sent[0], k));
    }

    /* A media key changes only the consumer report. */
    reset_sent();
    hid.consumer = 0x0CD;
    kb_hid_reporter_update(&r, &hid);
    KB_CHECK_EQ(n_sent, 1);
    KB_CHECK_EQ(sent[0].which, KB_HID_REPORT_CONSUMER);
    KB_CHECK_EQ(sent[0].data[1], 0xCD);

    /* A system key changes only the system report. */
    reset_sent();
    hid.system = 0x82;
    kb_hid_reporter_update(&r, &hid);
    KB_CHECK_EQ(n_sent, 1);
    KB_CHECK_EQ(sent[0].which, KB_HID_REPORT_SYSTEM);
}

static void test_reporter_6kro_path(void)
{
    struct kb_hid_reporter r;
    struct kb_hid_state hid;
    memset(&hid, 0, sizeof(hid));
    reset_sent();
    kb_hid_reporter_init(&r, false, on_send, NULL);
    KB_CHECK(!kb_hid_reporter_nkro_active(&r));

    press(&hid, KC_B);
    hid.mods = 0x02;
    kb_hid_reporter_update(&r, &hid);
    KB_CHECK_EQ(n_sent, 1);
    KB_CHECK_EQ(sent[0].which, KB_HID_REPORT_BOOT);
    KB_CHECK_EQ(sent[0].len, KB_HID_BOOT_REPORT_SIZE);
    KB_CHECK_EQ(sent[0].data[0], 0x02);
    KB_CHECK_EQ(sent[0].data[2], KC_B);
}

static void test_reporter_boot_protocol_fallback(void)
{
    struct kb_hid_reporter r;
    struct kb_hid_state hid;
    memset(&hid, 0, sizeof(hid));
    reset_sent();
    kb_hid_reporter_init(&r, true, on_send, NULL);

    press(&hid, KC_C);
    kb_hid_reporter_update(&r, &hid);
    KB_CHECK_EQ(n_sent, 1);
    KB_CHECK_EQ(sent[0].which, KB_HID_REPORT_NKRO);

    /* The host (a BIOS) asks for boot protocol while C is held: release it
     * on NKRO first, then press it on the boot report.
     */
    reset_sent();
    kb_hid_reporter_set_boot_protocol(&r, true, &hid);
    KB_CHECK(!kb_hid_reporter_nkro_active(&r));
    KB_CHECK_EQ(n_sent, 2);
    KB_CHECK_EQ(sent[0].which, KB_HID_REPORT_NKRO);
    KB_CHECK(!nkro_has(&sent[0], KC_C));
    KB_CHECK_EQ(sent[1].which, KB_HID_REPORT_BOOT);
    KB_CHECK_EQ(sent[1].data[2], KC_C);

    /* NK_TOGG while in boot protocol changes nothing on the wire. */
    reset_sent();
    kb_hid_reporter_set_nkro(&r, false, &hid);
    kb_hid_reporter_set_nkro(&r, true, &hid);
    KB_CHECK_EQ(n_sent, 0);
    KB_CHECK(!kb_hid_reporter_nkro_active(&r));

    /* Back to report protocol: release on boot, press on NKRO. */
    kb_hid_reporter_set_boot_protocol(&r, false, &hid);
    KB_CHECK(kb_hid_reporter_nkro_active(&r));
    KB_CHECK_EQ(n_sent, 2);
    KB_CHECK_EQ(sent[0].which, KB_HID_REPORT_BOOT);
    KB_CHECK_EQ(sent[0].data[2], 0);
    KB_CHECK_EQ(sent[1].which, KB_HID_REPORT_NKRO);
    KB_CHECK(nkro_has(&sent[1], KC_C));
}

static void test_reporter_toggle_nkro(void)
{
    struct kb_hid_reporter r;
    struct kb_hid_state hid;
    memset(&hid, 0, sizeof(hid));
    reset_sent();
    kb_hid_reporter_init(&r, true, on_send, NULL);

    /* Nothing held: switching sends nothing. */
    kb_hid_reporter_set_nkro(&r, false, &hid);
    KB_CHECK_EQ(n_sent, 0);
    KB_CHECK(!kb_hid_reporter_nkro_active(&r));

    press(&hid, KC_D);
    kb_hid_reporter_update(&r, &hid);
    KB_CHECK_EQ(n_sent, 1);
    KB_CHECK_EQ(sent[0].which, KB_HID_REPORT_BOOT);

    /* Turned on with D held: D moves to NKRO without a gap in the wrong order. */
    reset_sent();
    kb_hid_reporter_set_nkro(&r, true, &hid);
    KB_CHECK_EQ(n_sent, 2);
    KB_CHECK_EQ(sent[0].which, KB_HID_REPORT_BOOT);
    KB_CHECK_EQ(sent[0].data[2], 0);
    KB_CHECK_EQ(sent[1].which, KB_HID_REPORT_NKRO);
    KB_CHECK(nkro_has(&sent[1], KC_D));
}

static void test_reporter_resync(void)
{
    struct kb_hid_reporter r;
    struct kb_hid_state hid;
    memset(&hid, 0, sizeof(hid));
    reset_sent();
    kb_hid_reporter_init(&r, true, on_send, NULL);

    press(&hid, KC_E);
    kb_hid_reporter_update(&r, &hid);
    reset_sent();

    /* After a resume every report goes out again, unchanged or not. */
    kb_hid_reporter_resync(&r, &hid);
    KB_CHECK_EQ(n_sent, 4);
    KB_CHECK_EQ(sent[0].which, KB_HID_REPORT_BOOT);
    KB_CHECK_EQ(sent[1].which, KB_HID_REPORT_NKRO);
    KB_CHECK(nkro_has(&sent[1], KC_E));
    KB_CHECK_EQ(sent[2].which, KB_HID_REPORT_CONSUMER);
    KB_CHECK_EQ(sent[3].which, KB_HID_REPORT_SYSTEM);

    reset_sent();
    kb_hid_reporter_update(&r, &hid);
    KB_CHECK_EQ(n_sent, 0);
}

static void test_reporter_with_engine(void)
{
    /* The engine's own state through the reporter: a seventh key reaches NKRO. */
    static const kb_keycode_t def[1][1][8] = {
        {{KC_A, KC_B, KC_C, KC_D, KC_E, KC_F, KC_G, KC_VOLU}},
    };
    static kb_keycode_t storage[8];
    struct kb_keymap km;
    KB_CHECK_EQ(kb_keymap_init(&km, storage, 1, 1, 8, &def[0][0][0]), 0);

    struct kb_engine e;
    struct kb_hid_reporter r;
    static const struct kb_engine_ops ops = {.report = ignore_report};
    reset_sent();
    kb_engine_init(&e, &km, &ops, NULL);
    kb_hid_reporter_init(&r, true, on_send, NULL);

    for (uint8_t c = 0; c < 8; c++) {
        kb_engine_key(&e, 0, c, true, c);
    }
    kb_hid_reporter_update(&r, kb_engine_hid(&e));
    KB_CHECK_EQ(n_sent, 2);
    KB_CHECK_EQ(sent[0].which, KB_HID_REPORT_NKRO);
    KB_CHECK(nkro_has(&sent[0], KC_G));
    KB_CHECK_EQ(sent[1].which, KB_HID_REPORT_CONSUMER);
    KB_CHECK_EQ(sent[1].data[1], 0xE9);

    /* The same keys in 6KRO: the boot report holds the first six. */
    reset_sent();
    kb_hid_reporter_set_nkro(&r, false, kb_engine_hid(&e));
    KB_CHECK_EQ(n_sent, 2);
    KB_CHECK_EQ(sent[1].which, KB_HID_REPORT_BOOT);
    KB_CHECK_EQ(sent[1].data[2], KC_A);
    KB_CHECK_EQ(sent[1].data[7], KC_F);
}

int main(void)
{
    KB_RUN(test_keyboard_descriptor);
    KB_RUN(test_extra_descriptor);
    KB_RUN(test_config_descriptor);
    KB_RUN(test_mouse_report);
    KB_RUN(test_pack);
    KB_RUN(test_reporter_nkro_path);
    KB_RUN(test_reporter_6kro_path);
    KB_RUN(test_reporter_boot_protocol_fallback);
    KB_RUN(test_reporter_toggle_nkro);
    KB_RUN(test_reporter_resync);
    KB_RUN(test_reporter_with_engine);
    return KB_TEST_RESULT();
}
