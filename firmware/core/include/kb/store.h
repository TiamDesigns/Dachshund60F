/*
 * Settings store: a wear-levelled, CRC-checked log of whole-settings
 * snapshots in a flash area, hardware-independent (the firmware passes
 * flash read/write/erase functions; the host tests pass a simulated flash).
 *
 * Layout: the area is split into sectors (erase units). Each save appends
 * one record to the current sector:
 *   magic u32, seq u32, schema u16, len u16, crc32 u32, data, 0xFF padding
 * When a record doesn't fit, the next sector (round robin) is erased and
 * the record goes there, so every sector is erased equally often and the
 * newest record is never in the sector being erased.
 *
 * Power loss: the magic word is written last, and mount checks that the
 * free space after the last record is really blank, so a torn save is
 * ignored and the previous snapshot stays current.
 */
#ifndef KB_STORE_H
#define KB_STORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define KB_STORE_MAGIC 0x31485344u /* "DSH1" */
#define KB_STORE_HEADER_SIZE 16u
/* Flash writes go in whole words: the nRF52 programs 32 bits at a time. */
#define KB_STORE_WRITE_ALIGN 4u

/* Errors (negative returns) */
#define KB_STORE_ENOENT 1  /* nothing saved yet */
#define KB_STORE_ENOSPC 2  /* record larger than a sector */
#define KB_STORE_EIO 3     /* flash failed, or read back wrong */
#define KB_STORE_EINVAL 4  /* bad geometry or arguments */

struct kb_flash {
    /* Offsets are relative to the start of the area. Return 0 or negative. */
    int (*read)(void *ctx, uint32_t off, void *buf, size_t len);
    /* len and off are multiples of KB_STORE_WRITE_ALIGN; only clears bits. */
    int (*write)(void *ctx, uint32_t off, const void *buf, size_t len);
    /* off and len are whole sectors; erased flash reads 0xFF. */
    int (*erase)(void *ctx, uint32_t off, size_t len);
    void *ctx;
    uint32_t size;
    uint32_t sector_size;
};

struct kb_store {
    const struct kb_flash *flash;
    bool have_latest;
    uint32_t latest_off; /* the newest valid record */
    uint32_t latest_seq;
    uint32_t write_off;  /* where the next record goes */
    bool write_ok;       /* false: the next save starts a fresh sector */
};

/* CRC-32 (IEEE 802.3, as zlib), continuing from crc (start with 0). */
uint32_t kb_crc32(uint32_t crc, const void *data, size_t len);

/* Scan the area: find the newest valid record and the next write position. */
int kb_store_mount(struct kb_store *s, const struct kb_flash *flash);

/*
 * Read the newest record into buf (cap bytes). Sets *schema and *len.
 * Returns 0, -KB_STORE_ENOENT, -KB_STORE_ENOSPC (buf too small) or -KB_STORE_EIO.
 */
int kb_store_load(const struct kb_store *s, uint16_t *schema, void *buf, size_t cap,
                  size_t *len);

/* Append a new snapshot. Returns 0 or a negative KB_STORE_* error. */
int kb_store_save(struct kb_store *s, uint16_t schema, const void *data, size_t len);

/* Erase every sector: the next load finds nothing. */
int kb_store_wipe(struct kb_store *s);

/* Largest record data that fits a sector. */
static inline size_t kb_store_max_data(const struct kb_flash *flash)
{
    return flash->sector_size - KB_STORE_HEADER_SIZE;
}

#endif /* KB_STORE_H */
