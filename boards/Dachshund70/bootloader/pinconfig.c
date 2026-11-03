#include "boards.h"
#include "uf2/configkeys.h"

__attribute__((used, section(".bootloaderConfig")))
const uint32_t bootloaderConfig[] =
{
  /* CF2 START */
  CFG_MAGIC0, CFG_MAGIC1,                       // magic
  5, 100,                                       // used entries, total entries

  204, 0x100000,                                // FLASH_BYTES = 0x100000
  205, 0x40000,                                 // RAM_BYTES = 0x40000
  208, (USB_DESC_VID << 16) | USB_DESC_UF2_PID, // BOOTLOADER_BOARD_ID = USB VID+PID, used for verification when updating bootloader via uf2
  209, 0xada52840,                              // UF2_FAMILY = 0xada52840
  210, 0x20,                                    // PINS_PORT_SIZE = PA_32

  0, 0, 0, 0, 0, 0, 0, 0
  /* CF2 END */
};

/* Drive Esc's column so BUTTON_DFU (its row) reads the key. */
void board_init2(void)
{
  nrf_gpio_cfg_output(DFU_ESC_COL_PIN);
  nrf_gpio_pin_write(DFU_ESC_COL_PIN, 1);
  NRFX_DELAY_US(100); // let the row settle through the diode
}

/* Hand the pins back to the app in their reset state. */
void board_teardown2(void)
{
  nrf_gpio_cfg_default(DFU_ESC_COL_PIN);
  nrf_gpio_cfg_default(BUTTON_DFU);
}
