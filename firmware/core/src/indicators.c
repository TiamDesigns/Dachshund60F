#include "kb/indicators.h"

#include <string.h>

#include "kb/hid.h"

/* Low brightness: six SK6812s at full white draw about 360 mA. */
#define DEFAULT_BRIGHTNESS 32u

void kb_indicators_default_config(struct kb_indicator_config *cfg)
{
    static const struct kb_rgb layers[] = {
        {0, 0, 0},       /* Base: off */
        {255, 140, 0},   /* amber */
        {0, 200, 160},   /* teal */
        {170, 60, 255},  /* violet */
    };

    memset(cfg, 0, sizeof(*cfg));
    for (size_t i = 0; i < KB_MAX_LAYERS; i++) {
        /* Layers past the named ones repeat the non-base colours. */
        cfg->layer[i] = i < 4 ? layers[i] : layers[1 + (i - 1) % 3];
    }
    cfg->caps = (struct kb_rgb){255, 220, 180};
    cfg->num = (struct kb_rgb){255, 220, 180};
    cfg->scroll = (struct kb_rgb){255, 220, 180};
    cfg->battery = (struct kb_rgb){0, 255, 60};
    cfg->battery_low = (struct kb_rgb){255, 30, 0};
    cfg->battery_low_pct = 15;
    cfg->brightness = DEFAULT_BRIGHTNESS;
}

static uint8_t scale(uint8_t c, uint8_t brightness)
{
    return (uint8_t)(((unsigned)c * brightness + 127u) / 255u);
}

static uint8_t battery_levels(const struct kb_led_def *defs, size_t n)
{
    uint8_t levels = 0;
    for (size_t i = 0; i < n; i++) {
        if (defs[i].role == KB_LED_ROLE_BATTERY && defs[i].level > levels) {
            levels = defs[i].level;
        }
    }
    return levels;
}

bool kb_indicators_render(const struct kb_led_def *defs, size_t n,
                          const struct kb_indicator_config *cfg,
                          const struct kb_indicator_state *state, struct kb_rgb *out)
{
    static const struct kb_rgb off = {0, 0, 0};
    uint8_t levels = battery_levels(defs, n);
    bool lit = false;

    for (size_t i = 0; i < n; i++) {
        struct kb_rgb c = off;

        switch (state->asleep ? KB_LED_ROLE_NONE : defs[i].role) {
        case KB_LED_ROLE_LAYER:
            if (state->layer < KB_MAX_LAYERS) {
                c = cfg->layer[state->layer];
            }
            break;
        case KB_LED_ROLE_CAPS:
            c = (state->locks & KB_HID_LED_CAPS) ? cfg->caps : off;
            break;
        case KB_LED_ROLE_NUM:
            c = (state->locks & KB_HID_LED_NUM) ? cfg->num : off;
            break;
        case KB_LED_ROLE_SCROLL:
            c = (state->locks & KB_HID_LED_SCROLL) ? cfg->scroll : off;
            break;
        case KB_LED_ROLE_BATTERY:
            if (state->battery <= 100u && levels > 0u && defs[i].level > 0u) {
                /* Step k of n lights above (k - 1) / n, so any charge shows one LED. */
                unsigned threshold = (unsigned)(defs[i].level - 1u) * 100u / levels;
                if (defs[i].level == 1u || state->battery > threshold) {
                    c = state->battery <= cfg->battery_low_pct ? cfg->battery_low : cfg->battery;
                }
            }
            break;
        default:
            break;
        }

        out[i].r = scale(c.r, cfg->brightness);
        out[i].g = scale(c.g, cfg->brightness);
        out[i].b = scale(c.b, cfg->brightness);
        lit = lit || out[i].r || out[i].g || out[i].b;
    }
    return lit;
}
