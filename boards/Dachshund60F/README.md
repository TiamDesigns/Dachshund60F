# Dachshund60F

79-key board with an F-row, arrows and a split or full spacebar. It has one
rotary encoder right of Delete and runs on a Raytac MDBT50Q-1MV2 (nRF52840).
The layout comes from `KLE Info/` at the repo root.

- `keyboard.json`: physical layout, matrix mapping, layout options, encoder
  and indicator positions. The GUI reads this.
- `board.json`: pin map, USB IDs and power wiring. This is the source of truth
  for pins; firmware devicetree (phase 2) is written from it.

**Status: proposed.** No PCB exists yet. The pins below were chosen so the PCB
can be routed to them. If routing needs a swap, change `board.json` first,
then the firmware. Any matrix pin can trade places with any other matrix pin.

## USB IDs

| Device   | VID    | PID    |
|----------|--------|--------|
| Keyboard | 0x1209 | 0x0001 |
| Dongle   | 0x1209 | 0x0002 |

> **Warning:** these are [pid.codes](https://pid.codes/1209/0001/) *test* PIDs.
> They are not unique and must not be used on boards that are sold or given
> away. Before that, [request a real PID](https://pid.codes/howto/). That needs
> a public repo with a LICENSE file (OSI and OSHW licences) and the PCB design
> files.

## Matrix (6 × 15, COL2ROW)

Diodes point from column to row: cathode (stripe) toward the row. Columns are
driven and rows are read. Positions follow the physical columns, so traces stay
short; gaps are unused.

| Row \ Col | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 0 | Esc | F1 | F2 | F3 | F4 | F5 | F6 | F7 | F8 | F9 | F10 | F11 | F12 | Del | Enc push |
| 1 | ` | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 0 | - | = | Bksp | |
| 2 | Tab | Q | W | E | R | T | Y | U | I | O | P | [ | ] | \ | |
| 3 | Caps | A | S | D | F | G | H | J | K | L | ; | ' | | Enter | |
| 4 | LShift | Z | X | C | V | B | N | M | , | . | / | | RShift | ↑ | |
| 5 | LCtrl | Win | | Alt | LMod¹ | | Space² | | RMod¹ | | Fn | RCtrl | ← | ↓ | → |

¹ Only with the split spacebar.
² The 2.75u split Space and the 6.25u full bar share (5, 6). Overlap both
switch footprints and wire them to the same diode.

| Row | Pin | | Col | Pin | | Col | Pin |
|---|---|---|---|---|---|---|---|
| 0 | P1.10 | | 0 | P0.02 | | 8 | P1.03 |
| 1 | P1.11 | | 1 | P0.03 | | 9 | P1.04 |
| 2 | P1.12 | | 2 | P0.28 | | 10 | P1.05 |
| 3 | P1.13 | | 3 | P0.29 | | 11 | P1.06 |
| 4 | P1.14 | | 4 | P0.30 | | 12 | P1.07 |
| 5 | P1.15 | | 5 | P0.31 | | 13 | P0.04 |
| | | | 6 | P1.01 | | 14 | P0.05 |
| | | | 7 | P1.02 | | | |

## Other signals

| Signal | Pin | Notes |
|---|---|---|
| Encoder A | P0.11 | To GND through the encoder; internal pull-up. |
| Encoder B | P0.12 | Same as A. The push switch is in the matrix at (0, 14). |
| LED data | P0.06 | SK6812MINI-E chain: LED 0 under Caps Lock, LED 1 under Esc (layer). Driven by SPI MOSI. |
| LED power enable | P0.08 | Active-high EN of the TPS22917 load switch feeding the LEDs, with a 100 kΩ pull-down so the LEDs stay off through reset and sleep. |
| Charger status | P0.15 | MCP73831 STAT through a BAT54WS Schottky (cathode at STAT), so it behaves as open-drain: low while charging. Needs the internal pull-up. |
| Reset | P0.18 | nRESET: reset button to GND and SWD header. Double-tap enters the UF2 bootloader. |
| USB | D+, D−, VBUS | Dedicated module pads. |
| SWD | SWDIO, SWDCLK | Dedicated pads. Bring out SWDIO, SWDCLK, nRESET, VDD and GND to a header or pads. |

## Why these pins

- **Low-frequency pins carry slow signals.** The nRF52840 Product Specification
  marks P0.02/03/09/10/28–31 and P1.01–07/10–15 as "low-frequency I/O only",
  because they sit near the radio. The matrix scans at about 1 kHz, so it uses
  19 of them.
- **Full-speed pins carry fast signals.** The 800 kHz LED data line and the
  encoder go on full-speed P0 pins.
- **Reserved pins:** P0.00/P0.01 hold the 32.768 kHz crystal, which BLE timing
  needs. P0.09/P0.10 are NFC pads and stay unused, so the UICR doesn't need
  changing. P0.18 is the reset pin.
- **No pin for battery sensing.** The battery feeds VDDH and the SAADC measures
  VDDH/5 internally, so there is no divider leaking current.
- **Spare full-speed pins** for later: P0.07, P0.13, P0.14, P0.16, P0.17,
  P0.19–P0.27, P1.00, P1.08, P1.09.

## Hardware requirements these pins assume

- **Power:** VBUS to the module's VBUS pad. The charger's battery or system
  output goes to VDDH, through the power slide switch (nRF52840 high-voltage
  mode).
- **GPIO voltage:** set REGOUT0 to 3.3 V (UICR). The default is 1.8 V, too low
  to drive the SK6812 data input.
- **LED supply:** power the SK6812s from the battery or system rail through the
  load switch, not from 5 V VBUS. At 5 V their input-high threshold (0.7 × VDD)
  is above 3.3 V logic. If you want 5 V, add a level shifter such as a
  74AHCT1G125.
- **Encoder:** Bourns PEC12R-4220F-S0024, horizontal shaft, with a wide
  roller so it turns like a mouse wheel. A/B go to GPIO and C to GND, with
  10 nF from each of A and B to GND. Its body sits just past the 16u key
  area, so the PCB is about 12.7 mm wider than the layout.
- **USB-C:** 5.1 kΩ pull-downs on CC1 and CC2, and ESD protection on D+/D−
  (e.g. USBLC6-2).
- **Antenna:** keep the MDBT50Q antenna keep-out free of copper on all layers,
  at the board edge.
