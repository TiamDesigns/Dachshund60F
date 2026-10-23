#include "kb/store.h"
#include "kb_test.h"
#include "sim_flash.h"

static struct sim_flash sim;
static struct kb_store store;

static void fill(uint8_t *buf, size_t len, uint32_t seed)
{
    for (size_t i = 0; i < len; i++) {
        buf[i] = (uint8_t)(seed * 31u + i * 7u);
    }
}

/* Load and compare with what fill(seed) wrote. */
static bool holds(size_t len, uint32_t seed, uint16_t schema)
{
    uint8_t want[SIM_SECTOR], got[SIM_SECTOR];
    uint16_t sch = 0;
    size_t n = 0;
    fill(want, len, seed);
    return kb_store_load(&store, &sch, got, sizeof(got), &n) == 0 && n == len && sch == schema &&
           memcmp(want, got, len) == 0;
}

static int save(size_t len, uint32_t seed, uint16_t schema)
{
    uint8_t buf[SIM_SECTOR];
    fill(buf, len, seed);
    return kb_store_save(&store, schema, buf, len);
}

static void test_crc32(void)
{
    /* The standard check value. */
    KB_CHECK_EQ(kb_crc32(0, "123456789", 9), 0xCBF43926u);
    KB_CHECK_EQ(kb_crc32(kb_crc32(0, "1234", 4), "56789", 5), 0xCBF43926u);
}

static void test_geometry(void)
{
    sim_init(&sim);
    struct kb_flash bad = sim.flash;
    bad.size = SIM_SECTOR; /* one sector can't rotate */
    KB_CHECK_EQ(kb_store_mount(&store, &bad), -KB_STORE_EINVAL);
    bad = sim.flash;
    bad.size = SIM_SIZE - 4;
    KB_CHECK_EQ(kb_store_mount(&store, &bad), -KB_STORE_EINVAL);
}

static void test_empty(void)
{
    sim_init(&sim);
    KB_CHECK_EQ(kb_store_mount(&store, &sim.flash), 0);
    uint8_t buf[16];
    uint16_t schema;
    size_t len;
    KB_CHECK_EQ(kb_store_load(&store, &schema, buf, sizeof(buf), &len), -KB_STORE_ENOENT);
}

static void test_save_load_remount(void)
{
    sim_init(&sim);
    kb_store_mount(&store, &sim.flash);
    KB_CHECK_EQ(save(100, 1, 1), 0);
    KB_CHECK(holds(100, 1, 1));
    KB_CHECK_EQ(save(37, 2, 3), 0); /* odd length, other schema */
    KB_CHECK(holds(37, 2, 3));
    KB_CHECK_EQ(save(0, 3, 1), 0); /* empty is a valid snapshot */
    KB_CHECK(holds(0, 3, 1));
    KB_CHECK_EQ(save(1, 4, 1), 0);

    KB_CHECK_EQ(kb_store_mount(&store, &sim.flash), 0);
    KB_CHECK(holds(1, 4, 1));
    /* Appending continues after a remount. */
    KB_CHECK_EQ(save(200, 5, 1), 0);
    KB_CHECK_EQ(kb_store_mount(&store, &sim.flash), 0);
    KB_CHECK(holds(200, 5, 1));
    KB_CHECK_EQ(sim.violations, 0);

    /* A buffer too small for the record. */
    uint8_t small[10];
    uint16_t schema;
    size_t len;
    KB_CHECK_EQ(kb_store_load(&store, &schema, small, sizeof(small), &len), -KB_STORE_ENOSPC);
}

static void test_sizes(void)
{
    sim_init(&sim);
    kb_store_mount(&store, &sim.flash);
    size_t max = kb_store_max_data(&sim.flash);
    KB_CHECK_EQ(save(max + 1, 1, 1), -KB_STORE_ENOSPC);
    /* Exactly a sector, several times: each fills one sector. */
    for (uint32_t i = 0; i < 2 * SIM_SECTORS + 1; i++) {
        KB_CHECK_EQ(save(max, i, 1), 0);
        KB_CHECK(holds(max, i, 1));
        KB_CHECK_EQ(kb_store_mount(&store, &sim.flash), 0);
        KB_CHECK(holds(max, i, 1));
    }
    KB_CHECK_EQ(sim.violations, 0);
}

static void test_wear_levelling(void)
{
    sim_init(&sim);
    kb_store_mount(&store, &sim.flash);
    for (uint32_t i = 0; i < 2000; i++) {
        size_t len = 50 + (i * 37u) % 400u;
        KB_CHECK_EQ(save(len, i, 1), 0);
        if (i % 97 == 0) {
            KB_CHECK_EQ(kb_store_mount(&store, &sim.flash), 0);
        }
        KB_CHECK(holds(len, i, 1));
    }
    unsigned lo = sim.erases[0], hi = sim.erases[0];
    for (unsigned k = 1; k < SIM_SECTORS; k++) {
        lo = sim.erases[k] < lo ? sim.erases[k] : lo;
        hi = sim.erases[k] > hi ? sim.erases[k] : hi;
    }
    KB_CHECK(lo > 0);
    KB_CHECK(hi - lo <= 1);
    KB_CHECK_EQ(sim.violations, 0);
}

static void test_power_loss_every_write(void)
{
    /* Cut the power at every write of a save, in a sector with room and at a
     * sector change: the old snapshot or the new one survives, never neither.
     */
    for (int prior = 1; prior <= 9; prior++) {
        for (int cut = 1; cut <= 9; cut++) {
            sim_init(&sim);
            kb_store_mount(&store, &sim.flash);
            for (int i = 0; i < prior; i++) {
                KB_CHECK_EQ(save(300, (uint32_t)i, 1), 0);
            }
            sim.writes_left = cut;
            (void)save(301, 99, 1);
            sim.writes_left = -1;

            KB_CHECK_EQ(kb_store_mount(&store, &sim.flash), 0);
            bool old = holds(300, (uint32_t)prior - 1, 1);
            bool new = holds(301, 99, 1);
            KB_CHECK(old || new);
            /* 301 bytes take 8 writes; the magic word is the last of them. */
            KB_CHECK(cut == 9 ? new : !new);

            /* And the store keeps working. */
            KB_CHECK_EQ(save(10, 7, 1), 0);
            KB_CHECK_EQ(kb_store_mount(&store, &sim.flash), 0);
            KB_CHECK(holds(10, 7, 1));
            KB_CHECK_EQ(sim.violations, 0);
        }
    }
}

static void test_corruption(void)
{
    sim_init(&sim);
    kb_store_mount(&store, &sim.flash);
    KB_CHECK_EQ(save(64, 1, 1), 0);
    uint32_t first = store.latest_off;
    KB_CHECK_EQ(save(64, 2, 1), 0);
    uint32_t second = store.latest_off;

    /* A bit flipped in the newest record's data: the one before is used. */
    sim.mem[second + KB_STORE_HEADER_SIZE + 10] ^= 0x01;
    KB_CHECK_EQ(kb_store_mount(&store, &sim.flash), 0);
    KB_CHECK(holds(64, 1, 1));
    KB_CHECK_EQ(store.latest_off, first);
    /* The next save doesn't append after the damage. */
    KB_CHECK_EQ(save(64, 3, 1), 0);
    KB_CHECK(store.latest_off / SIM_SECTOR != second / SIM_SECTOR);
    KB_CHECK_EQ(kb_store_mount(&store, &sim.flash), 0);
    KB_CHECK(holds(64, 3, 1));

    /* Garbage everywhere: nothing loads, and saving still works. */
    memset(sim.mem, 0x5A, sizeof(sim.mem));
    KB_CHECK_EQ(kb_store_mount(&store, &sim.flash), 0);
    uint8_t buf[8];
    uint16_t schema;
    size_t len;
    KB_CHECK_EQ(kb_store_load(&store, &schema, buf, sizeof(buf), &len), -KB_STORE_ENOENT);
    KB_CHECK_EQ(save(8, 4, 1), 0);
    KB_CHECK_EQ(kb_store_mount(&store, &sim.flash), 0);
    KB_CHECK(holds(8, 4, 1));
}

static void test_wipe(void)
{
    sim_init(&sim);
    kb_store_mount(&store, &sim.flash);
    KB_CHECK_EQ(save(64, 1, 1), 0);
    KB_CHECK_EQ(kb_store_wipe(&store), 0);
    KB_CHECK_EQ(kb_store_mount(&store, &sim.flash), 0);
    uint8_t buf[64];
    uint16_t schema;
    size_t len;
    KB_CHECK_EQ(kb_store_load(&store, &schema, buf, sizeof(buf), &len), -KB_STORE_ENOENT);
}

static void test_sequence_wraps(void)
{
    sim_init(&sim);
    kb_store_mount(&store, &sim.flash);
    store.have_latest = true;
    store.latest_seq = 0xFFFFFFFEu;
    store.write_ok = false;
    KB_CHECK_EQ(save(16, 1, 1), 0); /* seq 0xFFFFFFFF */
    KB_CHECK_EQ(save(16, 2, 1), 0); /* seq 0 */
    KB_CHECK_EQ(kb_store_mount(&store, &sim.flash), 0);
    KB_CHECK(holds(16, 2, 1));
}

int main(void)
{
    KB_RUN(test_crc32);
    KB_RUN(test_geometry);
    KB_RUN(test_empty);
    KB_RUN(test_save_load_remount);
    KB_RUN(test_sizes);
    KB_RUN(test_wear_levelling);
    KB_RUN(test_power_loss_every_write);
    KB_RUN(test_corruption);
    KB_RUN(test_wipe);
    KB_RUN(test_sequence_wraps);
    return KB_TEST_RESULT();
}
