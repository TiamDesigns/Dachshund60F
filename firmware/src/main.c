/*
 * Keyboard firmware entry point.
 *
 * Matrix scanning and the keymap (phase 3) typing over a USB boot keyboard,
 * an LED walk along the SK6812 chain (phase 2 bring-up), and the status
 * screen when a panel overlay is built in.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <app_version.h>

#include "kb/core.h"
#include "kb_usb.h"
#include "keyboard.h"
#include "led_bringup.h"
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

    if (led_bringup_start() != 0) {
        LOG_ERR("LED bring-up failed");
    }

    return 0;
}
