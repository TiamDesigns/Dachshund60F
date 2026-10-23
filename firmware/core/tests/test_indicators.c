#include "kb/hid.h"
#include "kb/indicators.h"
#include "kb_test.h"

#include <string.h>

/* The Dachshund60F chain: layer, a four-step battery gauge, Caps Lock. */
static const struct kb_led_def chain[] = {
    {KB_LED_ROLE_LAYER, 0},   {KB_LED_ROLE_BATTERY, 1}, {KB_LED_ROLE_BATTERY, 2},
    {KB_LED_ROLE_BATTERY, 3}, {KB_LED_ROLE_BATTERY, 4}, {KB_LED_ROLE_CAPS, 0},
};
#define N (sizeof(chain) / sizeof(chain[0]))

static bool is_off(struct kb_rgb c)
{
    return c.r == 0 && c.g == 0 && c.b == 0;
}

static struct kb_indicator_config full_brightness(void)
{
    struct kb_indicator_config cfg;
    kb_indicators_default_config(&cfg);
    cfg.brightness = 255;
    return cfg;
}

static void test_all_off_at_rest(void)
{
    struct kb_indicator_config cfg;
    kb_indicators_default_config(&cfg);
    struct kb_indicator_state st = {0, 0, KB_BATTERY_UNKNOWN, false};
    struct kb_rgb out[N];
    memset(out, 0xAA, sizeof(out));

    KB_CHECK(!kb_indicators_render(chain, N, &cfg, &st, out));
    for (size_t i = 0; i < N; i++) {
        KB_CHECK(is_off(out[i]));
    }
}

static void test_caps_lock(void)
{
    struct kb_indicator_config cfg = full_brightness();
    struct kb_indicator_state st = {KB_HID_LED_CAPS, 0, KB_BATTERY_UNKNOWN, false};
    struct kb_rgb out[N];

    KB_CHECK(kb_indicators_render(chain, N, &cfg, &st, out));
    KB_CHECK_EQ(out[5].r, cfg.caps.r);
    KB_CHECK_EQ(out[5].g, cfg.caps.g);
    KB_CHECK_EQ(out[5].b, cfg.caps.b);
    KB_CHECK(is_off(out[0]));

    /* Num and Scroll have no LED on this board: nothing lights. */
    st.locks = KB_HID_LED_NUM | KB_HID_LED_SCROLL;
    KB_CHECK(!kb_indicators_render(chain, N, &cfg, &st, out));
}

static void test_num_and_scroll_roles(void)
{
    static const struct kb_led_def three[] = {
        {KB_LED_ROLE_NUM, 0}, {KB_LED_ROLE_CAPS, 0}, {KB_LED_ROLE_SCROLL, 0}};
    struct kb_indicator_config cfg = full_brightness();
    struct kb_indicator_state st = {KB_HID_LED_NUM | KB_HID_LED_SCROLL, 0, KB_BATTERY_UNKNOWN,
                                    false};
    struct kb_rgb out[3];

    KB_CHECK(kb_indicators_render(three, 3, &cfg, &st, out));
    KB_CHECK(!is_off(out[0]));
    KB_CHECK(is_off(out[1]));
    KB_CHECK(!is_off(out[2]));
}

static void test_layer_colour(void)
{
    struct kb_indicator_config cfg = full_brightness();
    struct kb_indicator_state st = {0, 1, KB_BATTERY_UNKNOWN, false};
    struct kb_rgb out[N];

    KB_CHECK(kb_indicators_render(chain, N, &cfg, &st, out));
    KB_CHECK_EQ(out[0].r, cfg.layer[1].r);
    KB_CHECK_EQ(out[0].g, cfg.layer[1].g);
    KB_CHECK_EQ(out[0].b, cfg.layer[1].b);

    /* A layer past the table stays dark rather than reading out of bounds. */
    st.layer = KB_MAX_LAYERS;
    KB_CHECK(!kb_indicators_render(chain, N, &cfg, &st, out));
}

static void test_brightness_scales(void)
{
    struct kb_indicator_config cfg;
    kb_indicators_default_config(&cfg);
    cfg.caps = (struct kb_rgb){255, 128, 0};
    cfg.brightness = 128;
    struct kb_indicator_state st = {KB_HID_LED_CAPS, 0, KB_BATTERY_UNKNOWN, false};
    struct kb_rgb out[N];

    kb_indicators_render(chain, N, &cfg, &st, out);
    KB_CHECK_EQ(out[5].r, 128);
    KB_CHECK_EQ(out[5].g, 64);
    KB_CHECK_EQ(out[5].b, 0);

    cfg.brightness = 0;
    KB_CHECK(!kb_indicators_render(chain, N, &cfg, &st, out));
}

static void test_asleep_is_dark(void)
{
    struct kb_indicator_config cfg = full_brightness();
    struct kb_indicator_state st = {KB_HID_LED_CAPS, 2, 80, true};
    struct kb_rgb out[N];

    KB_CHECK(!kb_indicators_render(chain, N, &cfg, &st, out));
}

static int lit_battery(const struct kb_rgb *out)
{
    int n = 0;
    for (size_t i = 1; i <= 4; i++) {
        n += !is_off(out[i]);
    }
    return n;
}

static void test_battery_gauge(void)
{
    struct kb_indicator_config cfg = full_brightness();
    struct kb_indicator_state st = {0, 0, KB_BATTERY_UNKNOWN, false};
    struct kb_rgb out[N];

    kb_indicators_render(chain, N, &cfg, &st, out);
    KB_CHECK_EQ(lit_battery(out), 0);

    st.battery = 100;
    kb_indicators_render(chain, N, &cfg, &st, out);
    KB_CHECK_EQ(lit_battery(out), 4);
    KB_CHECK_EQ(out[1].g, cfg.battery.g);

    st.battery = 75;
    kb_indicators_render(chain, N, &cfg, &st, out);
    KB_CHECK_EQ(lit_battery(out), 3);

    st.battery = 76;
    kb_indicators_render(chain, N, &cfg, &st, out);
    KB_CHECK_EQ(lit_battery(out), 4);

    st.battery = 26;
    kb_indicators_render(chain, N, &cfg, &st, out);
    KB_CHECK_EQ(lit_battery(out), 2);

    /* Low: one LED, in the low colour. */
    st.battery = 10;
    kb_indicators_render(chain, N, &cfg, &st, out);
    KB_CHECK_EQ(lit_battery(out), 1);
    KB_CHECK_EQ(out[1].r, cfg.battery_low.r);

    st.battery = 0;
    kb_indicators_render(chain, N, &cfg, &st, out);
    KB_CHECK_EQ(lit_battery(out), 1);

    /* Out of range reads as unknown. */
    st.battery = 101;
    kb_indicators_render(chain, N, &cfg, &st, out);
    KB_CHECK_EQ(lit_battery(out), 0);
}

int main(void)
{
    KB_RUN(test_all_off_at_rest);
    KB_RUN(test_caps_lock);
    KB_RUN(test_num_and_scroll_roles);
    KB_RUN(test_layer_colour);
    KB_RUN(test_brightness_scales);
    KB_RUN(test_asleep_is_dark);
    KB_RUN(test_battery_gauge);
    return KB_TEST_RESULT();
}
