/*
 * Reboot into the Adafruit nRF52 UF2 bootloader (on the Pup, Nordic's nRF5
 * bootloader: Kconfig KB_BOOTLOADER_GPREGRET).
 */
#ifndef BOOTLOADER_H
#define BOOTLOADER_H

/*
 * Does not return. The board comes back as a UF2 mass-storage drive.
 * Reached from QK_BOOT in the keymap, or by holding Esc for
 * CONFIG_KB_BOOTLOADER_HOLD_MS (see keyboard.c).
 */
void bootloader_reboot_to_uf2(void);

#endif /* BOOTLOADER_H */
