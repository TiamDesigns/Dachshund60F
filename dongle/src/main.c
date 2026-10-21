/*
 * 2.4 GHz dongle firmware entry point.
 *
 * Phase 1 scaffold: boots and reports its version. The ESB link, pairing and
 * USB HID relay are phase 14.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <app_version.h>

LOG_MODULE_REGISTER(kb_dongle, LOG_LEVEL_INF);

int main(void)
{
    LOG_INF("dongle firmware %s", APP_VERSION_STRING);

    while (1) {
        k_sleep(K_FOREVER);
    }

    return 0;
}
