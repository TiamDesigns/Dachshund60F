/*
 * Reboot into the Adafruit nRF52 UF2 bootloader.
 */
#ifndef BOOTLOADER_H
#define BOOTLOADER_H

/* Does not return. The board comes back as a UF2 mass-storage drive. */
void bootloader_reboot_to_uf2(void);

/*
 * Phase 2 bootloader key: holding Esc for CONFIG_KB_BOOTLOADER_HOLD_MS reboots
 * into UF2 mode. Replaced by a keymap action once the matrix exists (phase 3).
 */
int bootloader_key_start(void);

#endif /* BOOTLOADER_H */
