#include <string.h>

#include "kb/link.h"
#include "kb_test.h"

static void test_report_round_trip(void)
{
    uint8_t buf[KB_LINK_PAYLOAD_MAX];
    uint8_t nkro[KB_HID_NKRO_REPORT_SIZE];
    for (size_t i = 0; i < sizeof(nkro); i++) {
        nkro[i] = (uint8_t)(i * 7);
    }
    size_t n = kb_link_encode_report(KB_HID_REPORT_NKRO, nkro, sizeof(nkro), buf);
    KB_CHECK_EQ(n, 2 + sizeof(nkro));

    struct kb_link_packet p;
    KB_CHECK_EQ(kb_link_decode(buf, n, &p), 0);
    KB_CHECK_EQ(p.type, KB_LINK_REPORT);
    KB_CHECK_EQ(p.report.kind, KB_HID_REPORT_NKRO);
    KB_CHECK_EQ(p.report.len, sizeof(nkro));
    KB_CHECK(memcmp(p.report.data, nkro, sizeof(nkro)) == 0);

    /* Every kind fits a packet. */
    for (uint8_t kind = KB_HID_REPORT_BOOT; kind <= KB_HID_REPORT_MOUSE; kind++) {
        KB_CHECK(kb_link_report_size(kind) > 0);
        KB_CHECK(2 + kb_link_report_size(kind) <= KB_LINK_PAYLOAD_MAX);
    }
}

static void test_report_rejects(void)
{
    uint8_t buf[KB_LINK_PAYLOAD_MAX];
    uint8_t boot[KB_HID_BOOT_REPORT_SIZE] = {0};
    KB_CHECK_EQ(kb_link_encode_report(KB_HID_REPORT_BOOT, boot, 7, buf), 0);
    KB_CHECK_EQ(kb_link_encode_report((enum kb_hid_report)9, boot, 8, buf), 0);

    struct kb_link_packet p;
    size_t n = kb_link_encode_report(KB_HID_REPORT_BOOT, boot, sizeof(boot), buf);
    KB_CHECK_EQ(kb_link_decode(buf, n - 1, &p), -1); /* cut short */
    buf[1] = 9;                                       /* unknown kind */
    KB_CHECK_EQ(kb_link_decode(buf, n, &p), -1);
    buf[1] = KB_HID_REPORT_MOUSE; /* the wrong size for that kind */
    KB_CHECK_EQ(kb_link_decode(buf, n, &p), -1);
    KB_CHECK_EQ(kb_link_decode(buf, 1, &p), -1);
}

static void test_other_packets(void)
{
    uint8_t buf[KB_LINK_PAYLOAD_MAX];
    struct kb_link_packet p;

    size_t n = kb_link_encode_poll(buf);
    KB_CHECK_EQ(kb_link_decode(buf, n, &p), 0);
    KB_CHECK_EQ(p.type, KB_LINK_POLL);
    KB_CHECK_EQ(kb_link_decode(buf, 2, &p), -1);

    uint8_t frame[KB_LINK_CONFIG_SIZE];
    memset(frame, 0xA5, sizeof(frame));
    n = kb_link_encode_config(frame, buf);
    KB_CHECK_EQ(n, 65u);
    KB_CHECK(n <= KB_LINK_PAYLOAD_MAX);
    KB_CHECK_EQ(kb_link_decode(buf, n, &p), 0);
    KB_CHECK(memcmp(p.config, frame, sizeof(frame)) == 0);
    KB_CHECK_EQ(kb_link_decode(buf, n - 1, &p), -1);

    struct kb_link_status st = {.locks = 0x02, .flags = KB_LINK_USB_READY | KB_LINK_RESYNC};
    n = kb_link_encode_status(&st, buf);
    KB_CHECK_EQ(kb_link_decode(buf, n, &p), 0);
    KB_CHECK_EQ(p.status.locks, 0x02);
    KB_CHECK_EQ(p.status.flags, KB_LINK_USB_READY | KB_LINK_RESYNC);

    const uint8_t id[KB_LINK_ID_SIZE] = {1, 2, 3, 4, 5, 6, 7, 8};
    n = kb_link_encode_pair(id, buf);
    KB_CHECK_EQ(kb_link_decode(buf, n, &p), 0);
    KB_CHECK(memcmp(p.id, id, sizeof(id)) == 0);

    buf[0] = 0;
    KB_CHECK_EQ(kb_link_decode(buf, 1, &p), -1);
    buf[0] = 200;
    KB_CHECK_EQ(kb_link_decode(buf, 1, &p), -1);
    KB_CHECK_EQ(kb_link_decode(buf, 0, &p), -1);
}

static void test_addresses(void)
{
    uint8_t buf[KB_LINK_PAYLOAD_MAX];
    struct kb_link_packet p;

    KB_CHECK(!kb_link_address_valid(&kb_link_pairing_address));
    struct kb_link_address bad = {.base = {0x12, 0x00, 0x34, 0x56}, .prefix = 0x78};
    KB_CHECK(!kb_link_address_valid(&bad));
    bad.base[1] = 0x55;
    KB_CHECK(!kb_link_address_valid(&bad));
    bad.base[1] = 0x9A;
    KB_CHECK(kb_link_address_valid(&bad));
    bad.prefix = 0xFF;
    KB_CHECK(!kb_link_address_valid(&bad));
    KB_CHECK_EQ(kb_link_encode_pair_offer(&bad, buf), 0);

    /* Derived addresses are stable, valid and differ between devices. */
    uint8_t id[8] = {0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04};
    struct kb_link_address a, b, c;
    kb_link_address_from_id(id, sizeof(id), &a);
    kb_link_address_from_id(id, sizeof(id), &b);
    KB_CHECK(memcmp(&a, &b, sizeof(a)) == 0);
    KB_CHECK(kb_link_address_valid(&a));
    id[7] ^= 1;
    kb_link_address_from_id(id, sizeof(id), &c);
    KB_CHECK(memcmp(&a, &c, sizeof(a)) != 0);

    for (unsigned seed = 0; seed < 5000; seed++) {
        uint8_t sid[4] = {(uint8_t)seed, (uint8_t)(seed >> 8), 0x55, 0xFF};
        struct kb_link_address d;
        kb_link_address_from_id(sid, sizeof(sid), &d);
        KB_CHECK(kb_link_address_valid(&d));
    }

    size_t n = kb_link_encode_pair_offer(&a, buf);
    KB_CHECK_EQ(n, 6u);
    KB_CHECK_EQ(kb_link_decode(buf, n, &p), 0);
    KB_CHECK(memcmp(&p.address, &a, sizeof(a)) == 0);
    /* An offer of an unusable address is refused. */
    buf[5] = 0x00;
    KB_CHECK_EQ(kb_link_decode(buf, n, &p), -1);
}

int main(void)
{
    KB_RUN(test_report_round_trip);
    KB_RUN(test_report_rejects);
    KB_RUN(test_other_packets);
    KB_RUN(test_addresses);
    return KB_TEST_RESULT();
}
