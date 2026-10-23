#include "bootloader.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

#include <soc.h>

LOG_MODULE_REGISTER(bootloader, LOG_LEVEL_INF);

/* GPREGRET value the Adafruit bootloader checks to stay in UF2 mode. */
#define ADAFRUIT_DFU_MAGIC_UF2_RESET 0x57

void bootloader_reboot_to_uf2(void)
{
    LOG_INF("rebooting into UF2 bootloader");
    LOG_PANIC();

    NRF_POWER->GPREGRET = ADAFRUIT_DFU_MAGIC_UF2_RESET;
    NVIC_SystemReset();
}
