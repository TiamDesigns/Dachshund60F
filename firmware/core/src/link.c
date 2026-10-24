#include "kb/link.h"

#include <string.h>

const struct kb_link_address kb_link_pairing_address = {
    .base = {0x4B, 0x9D, 0x36, 0xE1},
    .prefix = 0xC6,
};

size_t kb_link_report_size(uint8_t kind)
{
    switch (kind) {
    case KB_HID_REPORT_BOOT:
        return KB_HID_BOOT_REPORT_SIZE;
    case KB_HID_REPORT_NKRO:
        return KB_HID_NKRO_REPORT_SIZE;
    case KB_HID_REPORT_CONSUMER:
        return KB_HID_CONSUMER_REPORT_SIZE;
    case KB_HID_REPORT_SYSTEM:
        return KB_HID_SYSTEM_REPORT_SIZE;
    case KB_HID_REPORT_MOUSE:
        return KB_HID_MOUSE_REPORT_SIZE;
    default:
        return 0;
    }
}

size_t kb_link_encode_poll(uint8_t *buf)
{
    buf[0] = KB_LINK_POLL;
    return 1;
}

size_t kb_link_encode_report(enum kb_hid_report kind, const uint8_t *data, size_t len,
                             uint8_t *buf)
{
    size_t size = kb_link_report_size((uint8_t)kind);
    if (size == 0 || len != size) {
        return 0;
    }
    buf[0] = KB_LINK_REPORT;
    buf[1] = (uint8_t)kind;
    memcpy(&buf[2], data, len);
    return 2 + len;
}

size_t kb_link_encode_config(const uint8_t frame[KB_LINK_CONFIG_SIZE], uint8_t *buf)
{
    buf[0] = KB_LINK_CONFIG;
    memcpy(&buf[1], frame, KB_LINK_CONFIG_SIZE);
    return 1 + KB_LINK_CONFIG_SIZE;
}

size_t kb_link_encode_status(const struct kb_link_status *s, uint8_t *buf)
{
    buf[0] = KB_LINK_STATUS;
    buf[1] = s->locks;
    buf[2] = s->flags;
    return 3;
}

size_t kb_link_encode_pair(const uint8_t id[KB_LINK_ID_SIZE], uint8_t *buf)
{
    buf[0] = KB_LINK_PAIR;
    memcpy(&buf[1], id, KB_LINK_ID_SIZE);
    return 1 + KB_LINK_ID_SIZE;
}

size_t kb_link_encode_pair_offer(const struct kb_link_address *a, uint8_t *buf)
{
    if (!kb_link_address_valid(a)) {
        return 0;
    }
    buf[0] = KB_LINK_PAIR_OFFER;
    memcpy(&buf[1], a->base, sizeof(a->base));
    buf[5] = a->prefix;
    return 6;
}

int kb_link_decode(const uint8_t *buf, size_t len, struct kb_link_packet *out)
{
    memset(out, 0, sizeof(*out));
    if (len < 1) {
        return -1;
    }
    out->type = buf[0];

    switch (buf[0]) {
    case KB_LINK_POLL:
        return len == 1 ? 0 : -1;
    case KB_LINK_REPORT: {
        if (len < 2) {
            return -1;
        }
        size_t size = kb_link_report_size(buf[1]);
        if (size == 0 || len != 2 + size) {
            return -1;
        }
        out->report.kind = buf[1];
        out->report.len = (uint8_t)size;
        out->report.data = &buf[2];
        return 0;
    }
    case KB_LINK_CONFIG:
        if (len != 1 + KB_LINK_CONFIG_SIZE) {
            return -1;
        }
        out->config = &buf[1];
        return 0;
    case KB_LINK_STATUS:
        if (len != 3) {
            return -1;
        }
        out->status.locks = buf[1];
        out->status.flags = buf[2];
        return 0;
    case KB_LINK_PAIR:
        if (len != 1 + KB_LINK_ID_SIZE) {
            return -1;
        }
        out->id = &buf[1];
        return 0;
    case KB_LINK_PAIR_OFFER:
        if (len != 6) {
            return -1;
        }
        memcpy(out->address.base, &buf[1], sizeof(out->address.base));
        out->address.prefix = buf[5];
        return kb_link_address_valid(&out->address) ? 0 : -1;
    default:
        return -1;
    }
}

static bool usable_byte(uint8_t b)
{
    return b != 0x00 && b != 0xFF && b != 0x55 && b != 0xAA;
}

bool kb_link_address_valid(const struct kb_link_address *a)
{
    if (memcmp(a->base, kb_link_pairing_address.base, sizeof(a->base)) == 0) {
        return false;
    }
    for (size_t i = 0; i < sizeof(a->base); i++) {
        if (!usable_byte(a->base[i])) {
            return false;
        }
    }
    return usable_byte(a->prefix);
}

/* FNV-1a, enough to spread a device ID over five bytes. */
static uint32_t fnv(const uint8_t *p, size_t len, uint32_t h)
{
    for (size_t i = 0; i < len; i++) {
        h = (h ^ p[i]) * 16777619u;
    }
    return h;
}

static uint8_t fix_byte(uint8_t b, uint8_t salt)
{
    while (!usable_byte(b)) {
        b = (uint8_t)(b + 0x13u + salt);
    }
    return b;
}

void kb_link_address_from_id(const uint8_t *id, size_t len, struct kb_link_address *out)
{
    uint32_t h1 = fnv(id, len, 2166136261u);
    uint32_t h2 = fnv(id, len, h1 ^ 0x9E3779B9u);

    for (size_t i = 0; i < sizeof(out->base); i++) {
        out->base[i] = fix_byte((uint8_t)(h1 >> (8 * i)), (uint8_t)i);
    }
    out->prefix = fix_byte((uint8_t)h2, 7);
    if (!kb_link_address_valid(out)) {
        /* Only the pairing address itself is left: step away from it. */
        out->base[0] = fix_byte((uint8_t)(out->base[0] + 1u), 1);
    }
}
