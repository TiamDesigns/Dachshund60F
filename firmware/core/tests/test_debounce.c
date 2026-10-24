#include "kb/debounce.h"
#include "kb_test.h"

#include <stddef.h>
#include <stdint.h>

/* Feed one key, (0, 0), of a 1x1 matrix and return its debounced state. */
static int feed(struct kb_debounce *db, int pressed, uint32_t t)
{
    kb_matrix_row_t raw[1] = {pressed ? 1u : 0u};
    kb_debounce_update(db, raw, t);
    return (int)(kb_debounce_state(db)[0] & 1u);
}

static void test_init_validates(void)
{
    struct kb_debounce db;
    KB_CHECK_EQ(kb_debounce_init(&db, 6, 15, KB_DEBOUNCE_EAGER_PRESS, 5), 0);
    KB_CHECK_EQ(kb_debounce_init(&db, 0, 15, KB_DEBOUNCE_EAGER_PRESS, 5), -1);
    KB_CHECK_EQ(kb_debounce_init(&db, 9, 15, KB_DEBOUNCE_EAGER_PRESS, 5), -1);
    KB_CHECK_EQ(kb_debounce_init(&db, 6, 33, KB_DEBOUNCE_EAGER_PRESS, 5), -1);
    KB_CHECK_EQ(kb_debounce_init(&db, 6, 15, KB_DEBOUNCE_ALGO_COUNT, 5), -1);
    KB_CHECK_EQ(kb_debounce_init(&db, 6, 15, KB_DEBOUNCE_DEFER_PER_KEY, KB_DEBOUNCE_MAX_MS + 1), -1);
}

static void test_defer_per_key(void)
{
    struct kb_debounce db;
    kb_debounce_init(&db, 1, 1, KB_DEBOUNCE_DEFER_PER_KEY, 5);

    KB_CHECK_EQ(feed(&db, 1, 100), 0);
    KB_CHECK_EQ(feed(&db, 0, 101), 0); /* bounce restarts the wait */
    KB_CHECK_EQ(feed(&db, 1, 102), 0);
    KB_CHECK_EQ(feed(&db, 1, 106), 0);
    KB_CHECK_EQ(feed(&db, 1, 107), 1); /* 5 ms after the last change */
    KB_CHECK_EQ(feed(&db, 0, 200), 1);
    KB_CHECK_EQ(feed(&db, 0, 204), 1);
    KB_CHECK_EQ(feed(&db, 0, 205), 0);
}

static void test_defer_per_key_rejects_a_glitch(void)
{
    struct kb_debounce db;
    kb_debounce_init(&db, 1, 1, KB_DEBOUNCE_DEFER_PER_KEY, 5);

    for (uint32_t t = 0; t < 20; t++) {
        KB_CHECK_EQ(feed(&db, t == 7, t), 0);
    }
}

static void test_defer_is_per_key(void)
{
    struct kb_debounce db;
    kb_debounce_init(&db, 2, 2, KB_DEBOUNCE_DEFER_PER_KEY, 5);

    kb_matrix_row_t a[2] = {0x1, 0x0};
    kb_matrix_row_t ab[2] = {0x1, 0x2};
    kb_debounce_update(&db, a, 0);
    kb_debounce_update(&db, ab, 3);
    KB_CHECK(!kb_debounce_update(&db, ab, 4));
    KB_CHECK(kb_debounce_update(&db, ab, 5)); /* key (0,0) only */
    KB_CHECK_EQ(kb_debounce_state(&db)[0], 0x1);
    KB_CHECK_EQ(kb_debounce_state(&db)[1], 0x0);
    KB_CHECK(kb_debounce_update(&db, ab, 8));
    KB_CHECK_EQ(kb_debounce_state(&db)[1], 0x2);
}

static void test_defer_global_waits_for_whole_matrix(void)
{
    struct kb_debounce db;
    kb_debounce_init(&db, 2, 2, KB_DEBOUNCE_DEFER_GLOBAL, 5);

    kb_matrix_row_t a[2] = {0x1, 0x0};
    kb_matrix_row_t ab[2] = {0x1, 0x2};
    kb_debounce_update(&db, a, 0);
    kb_debounce_update(&db, ab, 3); /* restarts the global timer */
    KB_CHECK(!kb_debounce_update(&db, ab, 5));
    KB_CHECK(!kb_debounce_update(&db, ab, 7));
    KB_CHECK(kb_debounce_update(&db, ab, 8));
    KB_CHECK_EQ(kb_debounce_state(&db)[0], 0x1);
    KB_CHECK_EQ(kb_debounce_state(&db)[1], 0x2);
}

static void test_eager_per_key(void)
{
    struct kb_debounce db;
    kb_debounce_init(&db, 1, 1, KB_DEBOUNCE_EAGER_PER_KEY, 5);

    KB_CHECK_EQ(feed(&db, 1, 10), 1); /* at once */
    KB_CHECK_EQ(feed(&db, 0, 11), 1); /* chatter ignored */
    KB_CHECK_EQ(feed(&db, 1, 12), 1);
    KB_CHECK_EQ(feed(&db, 0, 14), 1);
    KB_CHECK_EQ(feed(&db, 0, 15), 0); /* lockout over */
    KB_CHECK_EQ(feed(&db, 1, 16), 0); /* release lockout */
    KB_CHECK_EQ(feed(&db, 1, 20), 1);
}

static void test_eager_global(void)
{
    struct kb_debounce db;
    kb_debounce_init(&db, 1, 2, KB_DEBOUNCE_EAGER_GLOBAL, 5);

    kb_matrix_row_t none[1] = {0x0};
    kb_matrix_row_t a[1] = {0x1};
    kb_matrix_row_t ab[1] = {0x3};
    KB_CHECK(kb_debounce_update(&db, a, 0));
    KB_CHECK(!kb_debounce_update(&db, ab, 2)); /* whole matrix locked out */
    KB_CHECK(!kb_debounce_update(&db, none, 3));
    KB_CHECK(kb_debounce_update(&db, ab, 5));
    KB_CHECK_EQ(kb_debounce_state(&db)[0], 0x3);
}

static void test_eager_press_defer_release(void)
{
    struct kb_debounce db;
    kb_debounce_init(&db, 1, 1, KB_DEBOUNCE_EAGER_PRESS, 5);

    KB_CHECK_EQ(feed(&db, 1, 0), 1); /* press at once */
    KB_CHECK_EQ(feed(&db, 0, 1), 1); /* press bounce */
    KB_CHECK_EQ(feed(&db, 1, 2), 1);
    KB_CHECK_EQ(feed(&db, 1, 50), 1);
    KB_CHECK_EQ(feed(&db, 0, 51), 1); /* release waits */
    KB_CHECK_EQ(feed(&db, 1, 52), 1); /* release bounce restarts */
    KB_CHECK_EQ(feed(&db, 0, 53), 1);
    KB_CHECK_EQ(feed(&db, 0, 57), 1);
    KB_CHECK_EQ(feed(&db, 0, 58), 0);
    KB_CHECK_EQ(feed(&db, 1, 59), 1); /* next press at once */
}

static void test_zero_ms_passes_through(void)
{
    for (int algo = 0; algo < KB_DEBOUNCE_ALGO_COUNT; algo++) {
        struct kb_debounce db;
        kb_debounce_init(&db, 1, 1, (enum kb_debounce_algo)algo, 0);
        KB_CHECK_EQ(feed(&db, 1, 0), 1);
        KB_CHECK_EQ(feed(&db, 0, 1), 0);
        KB_CHECK_EQ(feed(&db, 1, 2), 1);
    }
}

static void test_every_algo_settles(void)
{
    /* Bounces shorter than the debounce time give exactly one press. */
    static const int bounce[] = {1, 0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                                 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    for (int algo = 0; algo < KB_DEBOUNCE_ALGO_COUNT; algo++) {
        struct kb_debounce db;
        kb_debounce_init(&db, 1, 1, (enum kb_debounce_algo)algo, 5);
        int presses = 0;
        int state = 0;
        for (uint32_t t = 0; t < sizeof(bounce) / sizeof(bounce[0]); t++) {
            int now = feed(&db, bounce[t], t);
            if (now && !state) {
                presses++;
            }
            state = now;
            if (t == 14) {
                KB_CHECK_EQ(state, 1);
            }
        }
        KB_CHECK_EQ(state, 0);
        KB_CHECK_EQ(presses, 1);
    }
}

static void test_timestamp_wraparound(void)
{
    struct kb_debounce db;
    kb_debounce_init(&db, 1, 1, KB_DEBOUNCE_DEFER_PER_KEY, 5);

    KB_CHECK_EQ(feed(&db, 1, UINT32_MAX - 2), 0);
    KB_CHECK_EQ(feed(&db, 1, 1), 0);
    KB_CHECK_EQ(feed(&db, 1, 2), 1);
}

static void test_ignores_bits_past_cols(void)
{
    struct kb_debounce db;
    kb_debounce_init(&db, 1, 15, KB_DEBOUNCE_EAGER_PRESS, 5);

    kb_matrix_row_t raw[1] = {0xFFFF8000u};
    KB_CHECK(!kb_debounce_update(&db, raw, 0));
    KB_CHECK_EQ(kb_debounce_state(&db)[0], 0);
}

static void test_full_width_rows(void)
{
    struct kb_debounce db;
    kb_debounce_init(&db, 1, 32, KB_DEBOUNCE_EAGER_PRESS, 5);

    kb_matrix_row_t raw[1] = {0x80000001u};
    KB_CHECK(kb_debounce_update(&db, raw, 0));
    KB_CHECK_EQ(kb_debounce_state(&db)[0], 0x80000001u);
}

static void test_configure_keeps_state(void)
{
    struct kb_debounce db;
    kb_debounce_init(&db, 1, 1, KB_DEBOUNCE_EAGER_PRESS, 5);

    KB_CHECK_EQ(feed(&db, 1, 0), 1);
    KB_CHECK_EQ(kb_debounce_configure(&db, KB_DEBOUNCE_DEFER_PER_KEY, 10), 0);
    KB_CHECK_EQ(feed(&db, 1, 1), 1);
    KB_CHECK_EQ(feed(&db, 0, 2), 1);
    KB_CHECK_EQ(feed(&db, 0, 11), 1);
    KB_CHECK_EQ(feed(&db, 0, 12), 0);

    KB_CHECK_EQ(kb_debounce_configure(&db, KB_DEBOUNCE_ALGO_COUNT, 5), -1);
    KB_CHECK_EQ(kb_debounce_configure(&db, KB_DEBOUNCE_EAGER_PRESS, 101), -1);
    KB_CHECK_EQ(db.algo, KB_DEBOUNCE_DEFER_PER_KEY);
    KB_CHECK_EQ(db.ms, 10);
}

static void test_idle(void)
{
    static const enum kb_debounce_algo algos[] = {
        KB_DEBOUNCE_DEFER_GLOBAL, KB_DEBOUNCE_DEFER_PER_KEY, KB_DEBOUNCE_EAGER_GLOBAL,
        KB_DEBOUNCE_EAGER_PER_KEY, KB_DEBOUNCE_EAGER_PRESS};
    for (size_t i = 0; i < sizeof(algos) / sizeof(algos[0]); i++) {
        struct kb_debounce db;
        kb_debounce_init(&db, 1, 1, algos[i], 5);
        KB_CHECK(kb_debounce_idle(&db));
        feed(&db, 1, 100);
        KB_CHECK(!kb_debounce_idle(&db)); /* reads pressed */
        feed(&db, 1, 110);
        feed(&db, 0, 120);
        /* Released but maybe not yet debounced released. */
        uint32_t t = 120;
        while (kb_debounce_state(&db)[0]) {
            KB_CHECK(!kb_debounce_idle(&db));
            feed(&db, 0, ++t);
        }
        KB_CHECK(kb_debounce_idle(&db));
    }
}

int main(void)
{
    printf("test_debounce\n");
    KB_RUN(test_init_validates);
    KB_RUN(test_idle);
    KB_RUN(test_defer_per_key);
    KB_RUN(test_defer_per_key_rejects_a_glitch);
    KB_RUN(test_defer_is_per_key);
    KB_RUN(test_defer_global_waits_for_whole_matrix);
    KB_RUN(test_eager_per_key);
    KB_RUN(test_eager_global);
    KB_RUN(test_eager_press_defer_release);
    KB_RUN(test_zero_ms_passes_through);
    KB_RUN(test_every_algo_settles);
    KB_RUN(test_timestamp_wraparound);
    KB_RUN(test_ignores_bits_past_cols);
    KB_RUN(test_full_width_rows);
    KB_RUN(test_configure_keeps_state);
    return KB_TEST_RESULT();
}
