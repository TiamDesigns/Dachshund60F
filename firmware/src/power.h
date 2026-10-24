/*
 * Power: the battery gauge (VDDH through the SAADC, the charger's CHG_STAT
 * pin), whether USB powers the board, and deep sleep. The decisions on
 * when to idle and sleep are kb/power.h's, made on the scan thread.
 *
 * Readings go to the Bluetooth Battery Service, the indicators' gauge and
 * the status screen.
 */
#ifndef POWER_H
#define POWER_H

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/toolchain.h>

#include "kb/protocol_gen.h"

/* Read the battery now and then every so often. Returns -ENODEV without a gauge (still usable). */
int power_init(void);

/* This build reads the battery. */
bool power_has_battery(void);

/* USB (VBUS) powers the board. Safe from any thread. */
bool power_usb(void);

/* On battery with a cell too low to keep going. */
bool power_battery_critical(void);

/* Read again soon: USB came or went. Safe from any thread, including interrupts. */
void power_poke(void);

/* The get_power response. */
void power_get(struct kb_proto_get_power_resp *out);

/* The board came up from deep sleep (a key woke it), rather than power-on or reset. */
bool power_woke_from_sleep(void);

/*
 * Deep sleep (System OFF): lights and screen off, the Bluetooth host told,
 * then only a key press wakes the board, through a reset. From the scan
 * thread, with the keymap lock not held.
 */
FUNC_NORETURN void power_off(void);

#endif /* POWER_H */
