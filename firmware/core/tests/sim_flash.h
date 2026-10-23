/*
 * Simulated NOR flash for the store tests: erased bytes read 0xFF, writes
 * can only clear bits and must be word-aligned, erases are whole sectors.
 * It can cut the power after a number of write calls, and counts erases.
 */
#ifndef SIM_FLASH_H
#define SIM_FLASH_H

#include <string.h>

#include "kb/store.h"

/* A user may define its own geometry first (kb_host_device: the firmware's). */
#ifndef SIM_SIZE
#define SIM_SIZE 8192u
#define SIM_SECTOR 1024u
#endif
#define SIM_SECTORS (SIM_SIZE / SIM_SECTOR)

struct sim_flash {
    uint8_t mem[SIM_SIZE];
    unsigned erases[SIM_SECTORS];
    int writes_left; /* < 0: no power cut */
    int violations;  /* writes that tried to set bits, misaligned access */
    struct kb_flash flash;
};

static int sim_read(void *ctx, uint32_t off, void *buf, size_t len)
{
    struct sim_flash *s = ctx;
    if (off > SIM_SIZE || len > SIM_SIZE - off) {
        s->violations++;
        return -1;
    }
    memcpy(buf, &s->mem[off], len);
    return 0;
}

static int sim_write(void *ctx, uint32_t off, const void *buf, size_t len)
{
    struct sim_flash *s = ctx;
    const uint8_t *b = buf;
    if (off % 4 || len % 4 || off > SIM_SIZE || len > SIM_SIZE - off) {
        s->violations++;
        return -1;
    }
    if (s->writes_left == 0) {
        return -1; /* power is gone */
    }
    for (size_t i = 0; i < len; i++) {
        if (b[i] & ~s->mem[off + i]) {
            s->violations++;
        }
        /* With the power cut mid-write, only the first half lands. */
        if (s->writes_left == 1 && i >= len / 2) {
            break;
        }
        s->mem[off + i] &= b[i];
    }
    if (s->writes_left > 0 && --s->writes_left == 0) {
        return -1;
    }
    return 0;
}

static int sim_erase(void *ctx, uint32_t off, size_t len)
{
    struct sim_flash *s = ctx;
    if (off % SIM_SECTOR || len % SIM_SECTOR || off + len > SIM_SIZE) {
        s->violations++;
        return -1;
    }
    if (s->writes_left == 0) {
        return -1;
    }
    memset(&s->mem[off], 0xFF, len);
    for (uint32_t k = off / SIM_SECTOR; k < (off + len) / SIM_SECTOR; k++) {
        s->erases[k]++;
    }
    return 0;
}

static void sim_init(struct sim_flash *s)
{
    memset(s, 0, sizeof(*s));
    memset(s->mem, 0xFF, sizeof(s->mem));
    s->writes_left = -1;
    s->flash = (struct kb_flash){
        .read = sim_read,
        .write = sim_write,
        .erase = sim_erase,
        .ctx = s,
        .size = SIM_SIZE,
        .sector_size = SIM_SECTOR,
    };
}

#endif /* SIM_FLASH_H */
