#include "kb/battery.h"
#include "kb_test.h"

static void test_curve(void)
{
    KB_CHECK_EQ(kb_battery_percent_for(4300), 100);
    KB_CHECK_EQ(kb_battery_percent_for(4200), 100);
    KB_CHECK_EQ(kb_battery_percent_for(3840), 50);
    KB_CHECK_EQ(kb_battery_percent_for(3845), 53); /* between 3840 (50) and 3850 (55) */
    KB_CHECK_EQ(kb_battery_percent_for(3400), 0);
    KB_CHECK_EQ(kb_battery_percent_for(3000), 0);

    /* Never rises as the voltage falls. */
    uint8_t last = 100;
    for (uint16_t mv = 4300; mv >= 3000; mv -= 5) {
        uint8_t pct = kb_battery_percent_for(mv);
        KB_CHECK(pct <= last);
        last = pct;
    }
}

static void test_unknown_until_read(void)
{
    struct kb_battery b;
    kb_battery_init(&b);
    KB_CHECK_EQ(b.percent, KB_BATTERY_PERCENT_UNKNOWN);

    /* On USB, VDDH is VBUS through the power path, not the cell. */
    KB_CHECK(kb_battery_update(&b, true, true, 4900));
    KB_CHECK_EQ(b.percent, KB_BATTERY_PERCENT_UNKNOWN);
    KB_CHECK(b.charging);
    /* Charge done, but no cell read yet: still unknown. */
    kb_battery_update(&b, true, false, 4900);
    KB_CHECK_EQ(b.percent, KB_BATTERY_PERCENT_UNKNOWN);

    /* Nothing that looks like a cell. */
    kb_battery_update(&b, false, false, 1200);
    KB_CHECK_EQ(b.percent, KB_BATTERY_PERCENT_UNKNOWN);
    KB_CHECK_EQ(b.mv, 0);
}

static void test_only_goes_down_on_battery(void)
{
    struct kb_battery b;
    kb_battery_init(&b);

    KB_CHECK(kb_battery_update(&b, false, false, 3840));
    KB_CHECK_EQ(b.percent, 50);
    KB_CHECK_EQ(b.mv, 3840);

    /* The cell recovers a little at rest: the gauge holds. */
    KB_CHECK(!kb_battery_update(&b, false, false, 3900));
    KB_CHECK_EQ(b.percent, 50);
    KB_CHECK_EQ(b.mv, 3855);

    /* Smoothed: one low reading moves it a quarter of the way. */
    for (int i = 0; i < 20; i++) {
        kb_battery_update(&b, false, false, 3770);
    }
    KB_CHECK_EQ(b.percent, 30);
    KB_CHECK(!kb_battery_critical(&b));
}

static void test_charge_and_unplug(void)
{
    struct kb_battery b;
    kb_battery_init(&b);
    kb_battery_update(&b, false, false, 3750);
    KB_CHECK_EQ(b.percent, 25);

    /* Plugged in and charging: keeps the last level. */
    KB_CHECK(kb_battery_update(&b, true, true, 4950));
    KB_CHECK_EQ(b.percent, 25);
    KB_CHECK(b.usb_power);
    KB_CHECK(b.charging);
    KB_CHECK(!kb_battery_update(&b, true, true, 4950));

    /* Charger done: full. */
    KB_CHECK(kb_battery_update(&b, true, false, 4950));
    KB_CHECK_EQ(b.percent, 100);

    /* Unplugged: the first reading on battery stands on its own. */
    KB_CHECK(kb_battery_update(&b, false, true, 4150));
    KB_CHECK_EQ(b.percent, 95);
    KB_CHECK(!b.charging); /* charging only counts on USB */
}

static void test_critical(void)
{
    struct kb_battery b;
    kb_battery_init(&b);
    KB_CHECK(!kb_battery_critical(&b));
    kb_battery_update(&b, false, false, 3250);
    KB_CHECK(kb_battery_critical(&b));
    KB_CHECK_EQ(b.percent, 0);
    kb_battery_update(&b, true, true, 4900);
    KB_CHECK(!kb_battery_critical(&b));
}

int main(void)
{
    KB_RUN(test_curve);
    KB_RUN(test_unknown_until_read);
    KB_RUN(test_only_goes_down_on_battery);
    KB_RUN(test_charge_and_unplug);
    KB_RUN(test_critical);
    return KB_TEST_RESULT();
}
