/*
 * Keyboard firmware entry point.
 *
 * Phase 1 scaffold: boots and reports its version. Board bring-up (LED, USB
 * enumeration, bootloader jump) is phase 2.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <app_version.h>

#include "kb/core.h"

LOG_MODULE_REGISTER(kb_main, LOG_LEVEL_INF);

int main(void)
{
    LOG_INF("keyboard firmware %s, core api %u", APP_VERSION_STRING,
            (unsigned)kb_core_api_version());

    while (1) {
        k_sleep(K_FOREVER);
    }

    return 0;
}
