/*
 * Battery gauge from the cell voltage, for a single Li-ion/LiPo cell.
 *
 * The board reads VDDH, which is the cell only while the cell powers the
 * board: with USB in, the power path feeds the board from VBUS. So:
 * - on battery, each reading is smoothed and turned into a percentage,
 *   which only goes down (a cell under no load creeps back up a little,
 *   and a gauge that rises on its own looks broken);
 * - on USB, readings are ignored; the percentage stays as it was, or reads
 *   100 once the charger says it is done, and the next reading on battery
 *   starts afresh (the cell may have charged meanwhile).
 */
#ifndef KB_BATTERY_H
#define KB_BATTERY_H

#include <stdbool.h>
#include <stdint.h>

#define KB_BATTERY_PERCENT_UNKNOWN 0xFFu

/* Readings outside this range aren't a cell (nothing connected, or a fault). */
#define KB_BATTERY_MIN_MV 2500u
#define KB_BATTERY_MAX_MV 4500u
/* Below this the board goes to sleep to spare the cell. */
#define KB_BATTERY_CRITICAL_MV 3300u

struct kb_battery {
    uint16_t mv;     /* smoothed cell voltage; 0 = no reading yet */
    uint8_t percent; /* KB_BATTERY_PERCENT_UNKNOWN until the first reading */
    bool fresh;      /* the next reading replaces the old ones */
    bool usb_power;
    bool charging;
};

/* State of charge for a resting cell at this voltage, 0-100. */
uint8_t kb_battery_percent_for(uint16_t mv);

void kb_battery_init(struct kb_battery *b);

/*
 * One reading: whether USB powers the board, whether the charger is
 * charging, and VDDH in mV. Returns true if the percentage, the power
 * source or the charging state changed.
 */
bool kb_battery_update(struct kb_battery *b, bool usb_power, bool charging, uint16_t vddh_mv);

/* On battery with a cell below KB_BATTERY_CRITICAL_MV. */
bool kb_battery_critical(const struct kb_battery *b);

#endif /* KB_BATTERY_H */
