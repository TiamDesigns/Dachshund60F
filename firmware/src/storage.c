/*
 * Settings storage: kb/store.h over the storage_partition (32 KB at
 * 0xEC000, inside the area the UF2 bootloader never erases or writes), as
 * eight 4 KB sectors.
 */

#include "storage.h"

#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>

#include "kb/store.h"

LOG_MODULE_REGISTER(storage, LOG_LEVEL_INF);

#define STORAGE_ID PARTITION_ID(storage_partition)
#define SECTOR_SIZE 4096u

static const struct flash_area *area;
static struct kb_store store;
static struct kb_flash flash;
static K_MUTEX_DEFINE(storage_mutex);

static int fa_read(void *ctx, uint32_t off, void *buf, size_t len)
{
    ARG_UNUSED(ctx);
    return flash_area_read(area, off, buf, len);
}

static int fa_write(void *ctx, uint32_t off, const void *buf, size_t len)
{
    ARG_UNUSED(ctx);
    return flash_area_write(area, off, buf, len);
}

static int fa_erase(void *ctx, uint32_t off, size_t len)
{
    ARG_UNUSED(ctx);
    return flash_area_erase(area, off, len);
}

static int to_errno(int err)
{
    switch (err) {
    case 0:
        return 0;
    case -KB_STORE_ENOENT:
        return -ENOENT;
    case -KB_STORE_ENOSPC:
        return -ENOSPC;
    case -KB_STORE_EINVAL:
        return -EINVAL;
    default:
        return -EIO;
    }
}

int storage_init(void)
{
    int err = flash_area_open(STORAGE_ID, &area);
    if (err) {
        LOG_ERR("opening the storage partition failed (%d)", err);
        return err;
    }

    flash = (struct kb_flash){
        .read = fa_read,
        .write = fa_write,
        .erase = fa_erase,
        .size = area->fa_size,
        .sector_size = SECTOR_SIZE,
    };

    k_mutex_lock(&storage_mutex, K_FOREVER);
    err = to_errno(kb_store_mount(&store, &flash));
    k_mutex_unlock(&storage_mutex);
    if (err) {
        LOG_ERR("mounting storage failed (%d)", err);
        return err;
    }
    LOG_INF("storage: %u KB, %s", (unsigned)(area->fa_size / 1024),
            store.have_latest ? "snapshot found" : "empty");
    return 0;
}

int storage_load(uint16_t *schema, uint8_t *buf, size_t cap, size_t *len)
{
    if (!area) {
        return -ENODEV;
    }
    k_mutex_lock(&storage_mutex, K_FOREVER);
    int err = to_errno(kb_store_load(&store, schema, buf, cap, len));
    k_mutex_unlock(&storage_mutex);
    return err;
}

int storage_save(uint16_t schema, const uint8_t *buf, size_t len)
{
    if (!area) {
        return -ENODEV;
    }
    k_mutex_lock(&storage_mutex, K_FOREVER);
    int err = to_errno(kb_store_save(&store, schema, buf, len));
    k_mutex_unlock(&storage_mutex);
    if (err) {
        LOG_ERR("save failed (%d)", err);
    }
    return err;
}

int storage_wipe(void)
{
    if (!area) {
        return -ENODEV;
    }
    k_mutex_lock(&storage_mutex, K_FOREVER);
    int err = to_errno(kb_store_wipe(&store));
    k_mutex_unlock(&storage_mutex);
    return err;
}
