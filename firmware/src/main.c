/*
 * Keyboard firmware entry point.
 *
 * Matrix scanning and the keymap (phase 3) typing over USB with NKRO,
 * media and system keys (phase 4), indicator LEDs for layer and Caps Lock
 * after a power-on walk along the chain, and the status screen when a
 * panel overlay is built in.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <app_version.h>

#include "kb/core.h"
#include "indicators.h"
#include "kb_usb.h"
#include "keyboard.h"
#include "status_screen.h"

LOG_MODULE_REGISTER(kb_main, LOG_LEVEL_INF);

int main(void)
{
    LOG_INF("keyboard firmware %s, core api %u", APP_VERSION_STRING,
            (unsigned)kb_core_api_version());

    /* Start the keyboard first: its hold-Esc recovery key works even if USB fails. */
    if (keyboard_start() != 0) {
        LOG_ERR("keyboard unavailable");
    }

    /* Before USB, so the screen sees the first lock-state report. */
    if (IS_ENABLED(CONFIG_KB_SCREEN) && status_screen_init() != 0) {
        LOG_ERR("status screen unavailable");
    }

    if (kb_usb_init() != 0) {
        LOG_ERR("USB init failed");
    }

    if (indicators_start() != 0) {
        LOG_ERR("indicator LEDs unavailable");
    }

    return 0;
}
