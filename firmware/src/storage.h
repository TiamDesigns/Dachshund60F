/*
 * Settings storage on the storage_partition in flash, through the
 * wear-levelled, CRC-checked log in kb/store.h.
 */
#ifndef STORAGE_H
#define STORAGE_H

#include <stddef.h>
#include <stdint.h>

#include "kb/store.h"

/* The partition's sectors are 4 KB; a snapshot fits in one, after its header. */
#define STORAGE_SECTOR_SIZE 4096u
#define STORAGE_RECORD_MAX (STORAGE_SECTOR_SIZE - KB_STORE_HEADER_SIZE)

/* Open the partition and find the newest snapshot. */
int storage_init(void);

/* The newest snapshot. -ENOENT if nothing is saved, -EIO if unreadable. */
int storage_load(uint16_t *schema, uint8_t *buf, size_t cap, size_t *len);

/* Append a snapshot. -ENOSPC if too large, -EIO if the flash failed. */
int storage_save(uint16_t schema, const uint8_t *buf, size_t len);

/* Erase everything saved. */
int storage_wipe(void);

#endif /* STORAGE_H */
