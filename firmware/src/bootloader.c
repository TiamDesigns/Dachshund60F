#include "bootloader.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

#include <soc.h>

LOG_MODULE_REGISTER(bootloader, LOG_LEVEL_INF);

/*
 * GPREGRET value the bootloader checks to stay in update mode: 0x57 for the
 * Adafruit UF2 bootloader (Kconfig KB_BOOTLOADER_GPREGRET).
 */

void bootloader_reboot_to_uf2(void)
{
    LOG_INF("rebooting into UF2 bootloader");
    LOG_PANIC();

    NRF_POWER->GPREGRET = CONFIG_KB_BOOTLOADER_GPREGRET;
    NVIC_SystemReset();
}
