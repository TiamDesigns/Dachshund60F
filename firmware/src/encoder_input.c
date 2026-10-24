#include "encoder_input.h"

#include <errno.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/spinlock.h>

#include "kb/encoder.h"

LOG_MODULE_REGISTER(encoder, LOG_LEVEL_INF);

#define USER_NODE DT_PATH(zephyr_user)

#if DT_NODE_HAS_PROP(USER_NODE, encoder_a_gpios) && DT_NODE_HAS_PROP(USER_NODE, encoder_b_gpios)

static const struct gpio_dt_spec pin_a = GPIO_DT_SPEC_GET(USER_NODE, encoder_a_gpios);
static const struct gpio_dt_spec pin_b = GPIO_DT_SPEC_GET(USER_NODE, encoder_b_gpios);
static struct kb_encoder decoder;
static bool ready;

static struct gpio_callback pin_cb[2];
static void (*wake_fn)(void);
static struct k_spinlock wake_lock;
static bool armed;

void encoder_input_wake_disarm(void)
{
    K_SPINLOCK(&wake_lock) {
        if (!armed) {
            K_SPINLOCK_BREAK;
        }
        armed = false;
        (void)gpio_pin_interrupt_configure_dt(&pin_a, GPIO_INT_DISABLE);
        (void)gpio_pin_interrupt_configure_dt(&pin_b, GPIO_INT_DISABLE);
    }
}

static void pin_isr(const struct device *port, struct gpio_callback *cb, gpio_port_pins_t pins)
{
    ARG_UNUSED(port);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);
    encoder_input_wake_disarm();
    if (wake_fn) {
        wake_fn();
    }
}

void encoder_input_set_wake_handler(void (*fn)(void))
{
    wake_fn = fn;
}

/* Either pin leaving the level it rests at now. */
static gpio_flags_t away_from(const struct gpio_dt_spec *pin)
{
    return gpio_pin_get_dt(pin) > 0 ? GPIO_INT_LEVEL_INACTIVE : GPIO_INT_LEVEL_ACTIVE;
}

void encoder_input_wake_arm(void)
{
    if (!ready) {
        return;
    }
    K_SPINLOCK(&wake_lock) {
        if (armed) {
            K_SPINLOCK_BREAK;
        }
        armed = true;
        (void)gpio_pin_interrupt_configure_dt(&pin_a, away_from(&pin_a));
        (void)gpio_pin_interrupt_configure_dt(&pin_b, away_from(&pin_b));
    }
}

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
        gpio_init_callback(&pin_cb[i], pin_isr, BIT(pins[i]->pin));
        err = gpio_add_callback_dt(pins[i], &pin_cb[i]);
        if (err) {
            LOG_ERR("encoder interrupt setup failed (%d)", err);
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

void encoder_input_set_wake_handler(void (*fn)(void))
{
    ARG_UNUSED(fn);
}

void encoder_input_wake_arm(void)
{
}

void encoder_input_wake_disarm(void)
{
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
