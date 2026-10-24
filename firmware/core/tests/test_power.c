#include <stdint.h>

#include "kb/power.h"
#include "kb_test.h"

static const struct kb_power_config cfg = {
    .idle_ms = 1000,
    .lights_off_ms = 60000,
    .radio_off_ms = 300000,
    .sleep_timeout_min = 15,
};

static struct kb_power_status step(struct kb_power *p, bool quiet, bool usb, uint32_t now)
{
    struct kb_power_inputs in = {.quiet = quiet, .usb_power = usb};
    return kb_power_update(p, &in, now);
}

static void test_idle_after_quiet(void)
{
    struct kb_power p;
    kb_power_init(&p, &cfg, 0);

    struct kb_power_status st = step(&p, false, false, 10);
    KB_CHECK_EQ(st.state, KB_POWER_ACTIVE);
    st = step(&p, true, false, 500);
    KB_CHECK_EQ(st.state, KB_POWER_ACTIVE);
    KB_CHECK_EQ(st.next_ms, 510u); /* idle at 1010 */
    st = step(&p, true, false, 1010);
    KB_CHECK_EQ(st.state, KB_POWER_IDLE);
    KB_CHECK(!st.lights_off);
    KB_CHECK_EQ(st.next_ms, 59000u); /* lights out at 60010 */

    /* A key: active again at once, and the quiet time starts over. */
    st = step(&p, false, false, 2000);
    KB_CHECK_EQ(st.state, KB_POWER_ACTIVE);
    st = step(&p, true, false, 2500);
    KB_CHECK_EQ(st.state, KB_POWER_ACTIVE);
}

static void test_battery_stages(void)
{
    struct kb_power p;
    kb_power_init(&p, &cfg, 0);

    struct kb_power_status st = step(&p, true, false, 60000);
    KB_CHECK_EQ(st.state, KB_POWER_IDLE);
    KB_CHECK(st.lights_off);
    KB_CHECK(!st.radio_off);
    KB_CHECK_EQ(st.next_ms, 240000u);

    st = step(&p, true, false, 300000);
    KB_CHECK(st.radio_off);
    KB_CHECK_EQ(st.state, KB_POWER_IDLE);
    KB_CHECK_EQ(st.next_ms, 600000u); /* deep sleep at 15 min */

    st = step(&p, true, false, 899999);
    KB_CHECK_EQ(st.state, KB_POWER_IDLE);
    KB_CHECK_EQ(st.next_ms, 1u);
    st = step(&p, true, false, 900000);
    KB_CHECK_EQ(st.state, KB_POWER_OFF);
}

static void test_usb_never_sleeps(void)
{
    struct kb_power p;
    kb_power_init(&p, &cfg, 0);

    struct kb_power_status st = step(&p, true, true, 3600000);
    KB_CHECK_EQ(st.state, KB_POWER_IDLE);
    KB_CHECK(!st.lights_off);
    KB_CHECK(!st.radio_off);
    KB_CHECK_EQ(st.next_ms, UINT32_MAX);

    /* Unplugged after an hour of quiet: everything that was due happens now. */
    st = step(&p, true, false, 3600001);
    KB_CHECK_EQ(st.state, KB_POWER_OFF);
}

static void test_never(void)
{
    struct kb_power p;
    struct kb_power_config none = {0};
    kb_power_init(&p, &none, 0);

    struct kb_power_status st = step(&p, true, false, 100000000);
    KB_CHECK_EQ(st.state, KB_POWER_ACTIVE);
    KB_CHECK(!st.lights_off);
    KB_CHECK(!st.radio_off);
    KB_CHECK_EQ(st.next_ms, UINT32_MAX);
}

static void test_activity_and_configure(void)
{
    struct kb_power p;
    kb_power_init(&p, &cfg, 0);

    /* A host request keeps the board awake like a key does. */
    kb_power_activity(&p, 899500);
    struct kb_power_status st = step(&p, true, false, 900000);
    KB_CHECK_EQ(st.state, KB_POWER_ACTIVE);

    /* A shorter sleep timeout counts the quiet time already spent. */
    struct kb_power_config shorter = cfg;
    shorter.sleep_timeout_min = 1;
    kb_power_configure(&p, &shorter);
    st = step(&p, true, false, 959499);
    KB_CHECK_EQ(st.state, KB_POWER_IDLE);
    st = step(&p, true, false, 959500);
    KB_CHECK_EQ(st.state, KB_POWER_OFF);
}

static void test_critical_battery(void)
{
    struct kb_power p;
    kb_power_init(&p, &cfg, 0);

    struct kb_power_inputs in = {.quiet = false, .battery_critical = true};
    struct kb_power_status st = kb_power_update(&p, &in, 5);
    KB_CHECK_EQ(st.state, KB_POWER_OFF);

    /* Not while USB powers the board (and charges the cell). */
    in.usb_power = true;
    st = kb_power_update(&p, &in, 6);
    KB_CHECK_EQ(st.state, KB_POWER_ACTIVE);
}

static void test_wrap(void)
{
    struct kb_power p;
    kb_power_init(&p, &cfg, UINT32_MAX - 100);

    struct kb_power_status st = step(&p, true, false, UINT32_MAX - 50);
    KB_CHECK_EQ(st.state, KB_POWER_ACTIVE);
    st = step(&p, true, false, 1000);
    KB_CHECK_EQ(st.state, KB_POWER_IDLE);
}

int main(void)
{
    KB_RUN(test_idle_after_quiet);
    KB_RUN(test_battery_stages);
    KB_RUN(test_usb_never_sleeps);
    KB_RUN(test_never);
    KB_RUN(test_activity_and_configure);
    KB_RUN(test_critical_battery);
    KB_RUN(test_wrap);
    return KB_TEST_RESULT();
}
