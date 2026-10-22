/*
 * Dachshund60F early board init.
 *
 * The board runs in high-voltage mode (battery/VSYS on VDDH), where REG0
 * outputs 1.8 V by default. GPIO then runs at 1.8 V, which is too low for the
 * SK6812 data input. Set UICR REGOUT0 to 3.3 V once, then reset so it takes
 * effect. The Adafruit bootloader may already have done this.
 */

#include <zephyr/init.h>
#include <hal/nrf_power.h>

static int dachshund60f_init(void)
{
    if ((nrf_power_mainregstatus_get(NRF_POWER) == NRF_POWER_MAINREGSTATUS_HIGH) &&
        ((NRF_UICR->REGOUT0 & UICR_REGOUT0_VOUT_Msk) ==
         (UICR_REGOUT0_VOUT_DEFAULT << UICR_REGOUT0_VOUT_Pos))) {
        NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen << NVMC_CONFIG_WEN_Pos;
        while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {
        }

        NRF_UICR->REGOUT0 = (NRF_UICR->REGOUT0 & ~((uint32_t)UICR_REGOUT0_VOUT_Msk)) |
                            (UICR_REGOUT0_VOUT_3V3 << UICR_REGOUT0_VOUT_Pos);

        NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos;
        while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {
        }

        NVIC_SystemReset();
    }

    return 0;
}

SYS_INIT(dachshund60f_init, PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
