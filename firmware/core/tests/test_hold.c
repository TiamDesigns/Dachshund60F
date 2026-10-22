#include "kb/hold.h"
#include "kb_test.h"

#include <stdint.h>

static void test_fires_after_hold_time(void)
{
    struct kb_hold h;
    kb_hold_init(&h, 3000);

    KB_CHECK(!kb_hold_update(&h, true, 1000));
    KB_CHECK(!kb_hold_update(&h, true, 2500));
    KB_CHECK(!kb_hold_update(&h, true, 3999));
    KB_CHECK(kb_hold_update(&h, true, 4000));
}

static void test_fires_once_per_press(void)
{
    struct kb_hold h;
    kb_hold_init(&h, 100);

    KB_CHECK(!kb_hold_update(&h, true, 0));
    KB_CHECK(kb_hold_update(&h, true, 150));
    KB_CHECK(!kb_hold_update(&h, true, 300));
    KB_CHECK(!kb_hold_update(&h, true, 10000));
}

static void test_release_rearms(void)
{
    struct kb_hold h;
    kb_hold_init(&h, 100);

    KB_CHECK(!kb_hold_update(&h, true, 0));
    KB_CHECK(kb_hold_update(&h, true, 100));
    KB_CHECK(!kb_hold_update(&h, false, 110));
    KB_CHECK(!kb_hold_update(&h, true, 120));
    KB_CHECK(!kb_hold_update(&h, true, 219));
    KB_CHECK(kb_hold_update(&h, true, 220));
}

static void test_short_press_restarts_timer(void)
{
    struct kb_hold h;
    kb_hold_init(&h, 100);

    KB_CHECK(!kb_hold_update(&h, true, 0));
    KB_CHECK(!kb_hold_update(&h, true, 90));
    KB_CHECK(!kb_hold_update(&h, false, 95));
    KB_CHECK(!kb_hold_update(&h, true, 96));
    KB_CHECK(!kb_hold_update(&h, true, 150));
    KB_CHECK(kb_hold_update(&h, true, 196));
}

static void test_released_never_fires(void)
{
    struct kb_hold h;
    kb_hold_init(&h, 0);

    KB_CHECK(!kb_hold_update(&h, false, 0));
    KB_CHECK(!kb_hold_update(&h, false, 5000));
}

static void test_timestamp_wraparound(void)
{
    struct kb_hold h;
    kb_hold_init(&h, 100);

    KB_CHECK(!kb_hold_update(&h, true, UINT32_MAX - 49));
    KB_CHECK(!kb_hold_update(&h, true, 40));
    KB_CHECK(kb_hold_update(&h, true, 50));
}

int main(void)
{
    printf("test_hold\n");
    KB_RUN(test_fires_after_hold_time);
    KB_RUN(test_fires_once_per_press);
    KB_RUN(test_release_rearms);
    KB_RUN(test_short_press_restarts_timer);
    KB_RUN(test_released_never_fires);
    KB_RUN(test_timestamp_wraparound);
    return KB_TEST_RESULT();
}
