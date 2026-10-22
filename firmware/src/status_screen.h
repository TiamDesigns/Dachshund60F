/*
 * Status screen on the J4 display header (OLED or e-paper).
 *
 * Built only when a panel overlay from firmware/displays/ sets the
 * zephyr,display chosen node (CONFIG_KB_SCREEN). Otherwise every call is a
 * no-op, so callers need no #ifdefs.
 */
#ifndef STATUS_SCREEN_H
#define STATUS_SCREEN_H

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include "kb/screen.h"

#if defined(CONFIG_KB_SCREEN)

int status_screen_init(void);

void status_screen_set_layer(uint8_t layer);
void status_screen_set_locks(uint8_t locks);
void status_screen_set_battery(int8_t pct, bool charging);
void status_screen_set_transport(uint8_t transport, uint8_t ble_profile);

/* Replace the layout. Rejected with -EINVAL unless it validates. */
int status_screen_set_layout(const struct kb_screen_layout *layout);

#else

static inline int status_screen_init(void)
{
    return -ENOTSUP;
}

static inline void status_screen_set_layer(uint8_t layer)
{
    (void)layer;
}

static inline void status_screen_set_locks(uint8_t locks)
{
    (void)locks;
}

static inline void status_screen_set_battery(int8_t pct, bool charging)
{
    (void)pct;
    (void)charging;
}

static inline void status_screen_set_transport(uint8_t transport, uint8_t ble_profile)
{
    (void)transport;
    (void)ble_profile;
}

static inline int status_screen_set_layout(const struct kb_screen_layout *layout)
{
    (void)layout;
    return -ENOTSUP;
}

#endif /* CONFIG_KB_SCREEN */

#endif /* STATUS_SCREEN_H */
