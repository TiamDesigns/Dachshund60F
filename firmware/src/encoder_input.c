#include "encoder_input.h"

#include <errno.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include "kb/encoder.h"

LOG_MODULE_REGISTER(encoder, LOG_LEVEL_INF);

#define USER_NODE DT_PATH(zephyr_user)

#if DT_NODE_HAS_PROP(USER_NODE, encoder_a_gpios) && DT_NODE_HAS_PROP(USER_NODE, encoder_b_gpios)

static const struct gpio_dt_spec pin_a = GPIO_DT_SPEC_GET(USER_NODE, encoder_a_gpios);
static const struct gpio_dt_spec pin_b = GPIO_DT_SPEC_GET(USER_NODE, encoder_b_gpios);
static struct kb_encoder decoder;
static bool ready;

int encoder_input_init(void)
{
    const struct gpio_dt_spec *pins[] = {&pin_a, &pin_b};
    for (size_t i = 0; i < ARRAY_SIZE(pins); i++) {
        if (!gpio_is_ready_dt(pins[i])) {
            return -ENODEV;
        }
        int err = gpio_pin_configure_dt(pins[i], GPIO_INPUT);
        if (err) {
            LOG_ERR("encoder pin setup failed (%d)", err);
            return err;
        }
    }
    kb_encoder_init(&decoder, gpio_pin_get_dt(&pin_a) > 0, gpio_pin_get_dt(&pin_b) > 0,
                    CONFIG_KB_ENCODER_STEPS_PER_DETENT);
    ready = true;
    return 0;
}

int encoder_input_count(void)
{
    return 1;
}

int encoder_input_poll(int i)
{
    if (!ready || i != 0) {
        return 0;
    }
    return kb_encoder_update(&decoder, gpio_pin_get_dt(&pin_a) > 0, gpio_pin_get_dt(&pin_b) > 0);
}

#else

int encoder_input_init(void)
{
    return 0;
}

int encoder_input_count(void)
{
    return 0;
}

int encoder_input_poll(int i)
{
    ARG_UNUSED(i);
    return 0;
}

#endif
