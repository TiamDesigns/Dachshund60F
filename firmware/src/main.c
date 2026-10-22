/*
 * Keyboard firmware entry point.
 *
 * Phase 2 bring-up: USB boot keyboard enumeration, an LED walk along the
 * SK6812 chain, hold-Esc to reboot into the UF2 bootloader, and the status
 * screen when a panel overlay is built in. Matrix scanning and the keymap
 * arrive in phase 3.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <app_version.h>

#include "bootloader.h"
#include "kb/core.h"
#include "kb_usb.h"
#include "led_bringup.h"
#include "status_screen.h"

LOG_MODULE_REGISTER(kb_main, LOG_LEVEL_INF);

int main(void)
{
    LOG_INF("keyboard firmware %s, core api %u", APP_VERSION_STRING,
            (unsigned)kb_core_api_version());

    /* Start the bootloader key first so a recovery path exists even if USB fails. */
    if (bootloader_key_start() != 0) {
        LOG_ERR("bootloader key unavailable");
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
