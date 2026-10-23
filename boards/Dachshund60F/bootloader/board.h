/*
 * Dachshund60F board definition for the Adafruit nRF52 bootloader.
 *
 * Pins come from boards/Dachshund60F/board.json. Copy this directory to
 * src/boards/dachshund60f/ in the bootloader tree (build.sh does that).
 */

#ifndef DACHSHUND60F_H
#define DACHSHUND60F_H

/* VDDH (battery or USB) powers the module, so GPIO runs at REGOUT0. The
 * SK6812 data input needs 3.3 V. Written to UICR on first boot. */
#define UICR_REGOUT0_VALUE UICR_REGOUT0_VOUT_3V3

/*------------------------------------------------------------------*/
/* LED
 *------------------------------------------------------------------*/
/* No plain LEDs: status goes to the SK6812MINI-E chain on P0.06, behind
 * the TPS22917 load switch on P0.08 (active high). */
#define LEDS_NUMBER          0
#define LED_NEOPIXEL         PINNUM(0, 6)
#define NEOPIXELS_NUMBER     6
#define NEOPIXEL_POWER_PIN   PINNUM(0, 8)
#define BOARD_RGB_BRIGHTNESS 0x101010

/*------------------------------------------------------------------*/
/* BUTTON
 *------------------------------------------------------------------*/
/* Esc is matrix (row 0, col 0), COL2ROW. board_init2() drives col 0
 * (P0.02) high, so holding Esc pulls row 0 (P1.10) high through the
 * diode. Hold Esc while plugging in or resetting to stay in UF2 mode. */
#define DFU_ESC_COL_PIN PINNUM(0, 2)
#define BUTTON_DFU      PINNUM(1, 10)
#define BUTTON_PULL     NRF_GPIO_PIN_PULLDOWN

//--------------------------------------------------------------------+
// BLE OTA
//--------------------------------------------------------------------+
#define BLEDIS_MANUFACTURER "Dachshund"
#define BLEDIS_MODEL        "Dachshund60F"

//--------------------------------------------------------------------+
// USB
//--------------------------------------------------------------------+
/* pid.codes TEST PID: in-house testing only, like the firmware's 0x0001.
 * Replace with a registered PID before any board is sold or given away. */
#define USB_DESC_VID          0x1209
#define USB_DESC_UF2_PID      0x0003
#define USB_DESC_CDC_ONLY_PID 0x0003

//--------------------------------------------------------------------+
// UF2
//--------------------------------------------------------------------+
#define UF2_PRODUCT_NAME "Dachshund60F"
#define UF2_VOLUME_LABEL "DACHSHUND"
#define UF2_BOARD_ID     "nRF52840-Dachshund60F-v1"
#define UF2_INDEX_URL    "https://github.com/TiamDesigns/Dachshund60F"

#endif // DACHSHUND60F_H
