#include "kb/store.h"

#include <string.h>

#define CHUNK 64u

uint32_t kb_crc32(uint32_t crc, const void *data, size_t len)
{
    const uint8_t *p = data;
    crc = ~crc;
    while (len--) {
        crc ^= *p++;
        for (int k = 0; k < 8; k++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static uint32_t pad4(uint32_t n)
{
    return (n + KB_STORE_WRITE_ALIGN - 1u) & ~(KB_STORE_WRITE_ALIGN - 1u);
}

struct header {
    uint32_t magic;
    uint32_t seq;
    uint16_t schema;
    uint16_t len;
    uint32_t crc;
};

static int read_header(const struct kb_flash *f, uint32_t off, struct header *h)
{
    uint8_t b[KB_STORE_HEADER_SIZE];
    if (f->read(f->ctx, off, b, sizeof(b)) != 0) {
        return -KB_STORE_EIO;
    }
    h->magic = get32(&b[0]);
    h->seq = get32(&b[4]);
    h->schema = (uint16_t)(b[8] | (b[9] << 8));
    h->len = (uint16_t)(b[10] | (b[11] << 8));
    h->crc = get32(&b[12]);
    return 0;
}

/* CRC over seq, schema, len and the data, as stored. */
static int record_crc(const struct kb_flash *f, uint32_t off, const struct header *h,
                      uint32_t *crc)
{
    uint8_t b[CHUNK];
    put32(&b[0], h->seq);
    b[4] = (uint8_t)h->schema;
    b[5] = (uint8_t)(h->schema >> 8);
    b[6] = (uint8_t)h->len;
    b[7] = (uint8_t)(h->len >> 8);
    uint32_t c = kb_crc32(0, b, 8);

    for (uint32_t done = 0; done < h->len;) {
        uint32_t n = h->len - done < CHUNK ? h->len - done : CHUNK;
        if (f->read(f->ctx, off + KB_STORE_HEADER_SIZE + done, b, n) != 0) {
            return -KB_STORE_EIO;
        }
        c = kb_crc32(c, b, n);
        done += n;
    }
    *crc = c;
    return 0;
}

static bool is_blank(const struct kb_flash *f, uint32_t off, uint32_t end)
{
    uint8_t b[CHUNK];
    while (off < end) {
        uint32_t n = end - off < CHUNK ? end - off : CHUNK;
        if (f->read(f->ctx, off, b, n) != 0) {
            return false;
        }
        for (uint32_t i = 0; i < n; i++) {
            if (b[i] != 0xFFu) {
                return false;
            }
        }
        off += n;
    }
    return true;
}

static bool seq_newer(uint32_t a, uint32_t b)
{
    return (int32_t)(a - b) > 0;
}

static bool geometry_ok(const struct kb_flash *f)
{
    return f && f->read && f->write && f->erase && f->sector_size >= 2u * KB_STORE_HEADER_SIZE &&
           f->sector_size % KB_STORE_WRITE_ALIGN == 0 && f->size % f->sector_size == 0 &&
           f->size / f->sector_size >= 2u && kb_store_max_data(f) <= 0xFFFFu;
}

int kb_store_mount(struct kb_store *s, const struct kb_flash *flash)
{
    memset(s, 0, sizeof(*s));
    s->flash = flash;
    if (!geometry_ok(flash)) {
        return -KB_STORE_EINVAL;
    }

    uint32_t latest_sector_free = 0;
    bool latest_sector_clean = false;

    for (uint32_t start = 0; start < flash->size; start += flash->sector_size) {
        uint32_t end = start + flash->sector_size;
        uint32_t off = start;
        bool clean = false;
        bool newest_here = false;

        while (off + KB_STORE_HEADER_SIZE <= end) {
            struct header h;
            if (read_header(flash, off, &h) != 0) {
                break;
            }
            if (h.magic == 0xFFFFFFFFu) {
                /* Free space, unless a torn save left bytes behind it. */
                clean = is_blank(flash, off, end);
                break;
            }
            if (h.magic != KB_STORE_MAGIC || h.len > end - off - KB_STORE_HEADER_SIZE) {
                break;
            }
            uint32_t crc;
            if (record_crc(flash, off, &h, &crc) != 0 || crc != h.crc) {
                break; /* torn or damaged: nothing after it can be trusted */
            }
            if (!s->have_latest || seq_newer(h.seq, s->latest_seq)) {
                s->have_latest = true;
                s->latest_seq = h.seq;
                s->latest_off = off;
                newest_here = true;
            }
            off += KB_STORE_HEADER_SIZE + pad4(h.len);
            if (off >= end) {
                clean = true; /* exactly full */
                break;
            }
        }
        if (newest_here) {
            latest_sector_free = off;
            latest_sector_clean = clean;
        }
    }

    /* Keep appending after the newest record if its sector's free space is clean. */
    s->write_ok = s->have_latest && latest_sector_clean;
    s->write_off = s->write_ok ? latest_sector_free : 0;
    return 0;
}

int kb_store_load(const struct kb_store *s, uint16_t *schema, void *buf, size_t cap,
                  size_t *len)
{
    if (!s->flash || !s->have_latest) {
        return -KB_STORE_ENOENT;
    }
    struct header h;
    if (read_header(s->flash, s->latest_off, &h) != 0) {
        return -KB_STORE_EIO;
    }
    if (h.len > cap) {
        return -KB_STORE_ENOSPC;
    }
    if (s->flash->read(s->flash->ctx, s->latest_off + KB_STORE_HEADER_SIZE, buf, h.len) != 0) {
        return -KB_STORE_EIO;
    }
    /* Check again: the flash may have changed since mount. */
    uint32_t crc;
    if (record_crc(s->flash, s->latest_off, &h, &crc) != 0 || crc != h.crc) {
        return -KB_STORE_EIO;
    }
    *schema = h.schema;
    *len = h.len;
    return 0;
}

static int write_record(const struct kb_flash *f, uint32_t off, uint32_t seq, uint16_t schema,
                        const uint8_t *data, size_t len)
{
    uint8_t b[KB_STORE_HEADER_SIZE];
    struct header h = {.seq = seq, .schema = schema, .len = (uint16_t)len};

    /* CRC as record_crc computes it. */
    put32(&b[0], seq);
    b[4] = (uint8_t)schema;
    b[5] = (uint8_t)(schema >> 8);
    b[6] = (uint8_t)len;
    b[7] = (uint8_t)(len >> 8);
    h.crc = kb_crc32(kb_crc32(0, b, 8), data, len);

    /* Everything but the magic word first, then the data, then the magic. */
    put32(&b[4], h.seq);
    b[8] = (uint8_t)h.schema;
    b[9] = (uint8_t)(h.schema >> 8);
    b[10] = (uint8_t)h.len;
    b[11] = (uint8_t)(h.len >> 8);
    put32(&b[12], h.crc);
    if (f->write(f->ctx, off + 4u, &b[4], KB_STORE_HEADER_SIZE - 4u) != 0) {
        return -KB_STORE_EIO;
    }

    size_t whole = len & ~(size_t)(KB_STORE_WRITE_ALIGN - 1u);
    for (size_t done = 0; done < whole;) {
        uint8_t chunk[CHUNK];
        size_t n = whole - done < CHUNK ? whole - done : CHUNK;
        memcpy(chunk, data + done, n); /* data may be unaligned */
        if (f->write(f->ctx, off + KB_STORE_HEADER_SIZE + (uint32_t)done, chunk, n) != 0) {
            return -KB_STORE_EIO;
        }
        done += n;
    }
    if (whole < len) {
        uint8_t tail[KB_STORE_WRITE_ALIGN];
        memset(tail, 0xFF, sizeof(tail));
        memcpy(tail, data + whole, len - whole);
        if (f->write(f->ctx, off + KB_STORE_HEADER_SIZE + (uint32_t)whole, tail, sizeof(tail)) !=
            0) {
            return -KB_STORE_EIO;
        }
    }

    put32(&b[0], KB_STORE_MAGIC);
    if (f->write(f->ctx, off, b, 4u) != 0) {
        return -KB_STORE_EIO;
    }

    /* Read it back. */
    struct header back;
    uint32_t crc;
    if (read_header(f, off, &back) != 0 || back.magic != KB_STORE_MAGIC || back.crc != h.crc ||
        record_crc(f, off, &back, &crc) != 0 || crc != h.crc) {
        return -KB_STORE_EIO;
    }
    return 0;
}

int kb_store_save(struct kb_store *s, uint16_t schema, const void *data, size_t len)
{
    const struct kb_flash *f = s->flash;
    if (!f || (len && !data)) {
        return -KB_STORE_EINVAL;
    }
    if (len > kb_store_max_data(f)) {
        return -KB_STORE_ENOSPC;
    }

    uint32_t size = KB_STORE_HEADER_SIZE + pad4((uint32_t)len);
    uint32_t sector_end = (s->write_off / f->sector_size + 1u) * f->sector_size;
    /*
     * write_off on a sector boundary means the previous record filled its
     * sector exactly: that's never a place to append (and the sector there
     * isn't erased).
     */
    bool fits = s->write_ok && s->write_off % f->sector_size != 0 &&
                s->write_off + size <= sector_end;

    uint32_t off = s->write_off;
    if (!fits) {
        /* Move to the sector after the newest record's, and erase it. */
        uint32_t sectors = f->size / f->sector_size;
        uint32_t next = s->have_latest ? (s->latest_off / f->sector_size + 1u) % sectors : 0u;
        off = next * f->sector_size;
        if (f->erase(f->ctx, off, f->sector_size) != 0) {
            s->write_ok = false;
            return -KB_STORE_EIO;
        }
    }

    uint32_t seq = s->have_latest ? s->latest_seq + 1u : 1u;
    int err = write_record(f, off, seq, schema, data, len);
    if (err) {
        /* Don't append after a bad record; the old snapshot stays current. */
        s->write_ok = false;
        return err;
    }

    s->have_latest = true;
    s->latest_seq = seq;
    s->latest_off = off;
    s->write_off = off + size;
    s->write_ok = true;
    return 0;
}

int kb_store_wipe(struct kb_store *s)
{
    const struct kb_flash *f = s->flash;
    if (!f) {
        return -KB_STORE_EINVAL;
    }
    for (uint32_t off = 0; off < f->size; off += f->sector_size) {
        if (f->erase(f->ctx, off, f->sector_size) != 0) {
            return -KB_STORE_EIO;
        }
    }
    s->have_latest = false;
    s->latest_seq = 0;
    s->latest_off = 0;
    s->write_off = 0;
    s->write_ok = false;
    return 0;
}
