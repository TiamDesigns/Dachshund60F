# Dachshund60F

79-key board with an F-row, arrows and a split or full spacebar. It has one
rotary encoder right of Delete and runs on a Raytac MDBT50Q-P1MV2 (nRF52840).
The layout comes from `KLE Info/` at the repo root.

- `keyboard.json`: physical layout, matrix mapping, layout options, encoder
  and indicator positions. The GUI reads this. Indicator `id` is the LED's
  index in the data chain. An indicator under a key has `underKey`; a
  free-standing one has `at`, its centre in key units.
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
| UF2 bootloader | 0x1209 | 0x0003 |

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
| LED data | P0.06 | SK6812MINI-E chain of 6, in this order: LED 0 layer, LEDs 1–4 battery gauge (25/50/75/100%), LED 5 under Caps Lock. LEDs 0–4 sit in a row above the Right arrow. Driven by SPI MOSI. |
| LED power enable | P0.08 | Active-high EN of the TPS22917 load switch feeding the LEDs, with a 100 kΩ pull-down so the LEDs stay off through reset and sleep. |
| Display CLK / DIN | P0.19 / P0.21 | J4 pins 4 and 3. SPI SCK/MOSI for e-paper, or SCL/SDA for an I2C OLED (the display module supplies the pull-ups). |
| Display CS / DC / RST / BUSY | P0.20 / P0.23 / P0.22 / P0.24 | J4 pins 5–8, for e-paper. An I2C OLED leaves them unused. J4 pins 1 and 2 are +3V3 and GND. |
| Charger status | P0.15 | MCP73831 STAT through a BAT54WS Schottky (cathode at STAT), so it behaves as open-drain: low while charging. Needs the internal pull-up. |
| Reset | P0.18 | nRESET: reset button to GND and SWD header. Double-tap enters the UF2 bootloader (so does holding Esc at reset). |
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
  P0.25–P0.27, P1.00, P1.08, P1.09. P0.19–P0.24 go to the display header J4.

## Hardware requirements these pins assume

- **Power:** VBUS to the module's VBUS pad. VDDH (nRF52840 high-voltage
  mode) is VSYS, fed from USB through a Schottky, or from the battery through
  the power slide switch and a load-sharing P-MOSFET that turns off while USB
  is in. On USB, VDDH reads the USB rail, not the battery.
- **GPIO voltage:** set REGOUT0 to 3.3 V (UICR). The default is 1.8 V, too low
  to drive the SK6812 data input.
- **LED supply:** the SK6812s run from VSYS through the load switch, which is
  about 4.7 V on USB. Their input-high threshold (0.7 × VDD) is then about
  3.3 V, so the data line goes through an SN74LV1T34 buffer powered from
  LED_VDD. Its TTL-level input reads 3.3 V logic at any LED voltage.
- **Encoder:** Bourns PEC12R-4220F-S0024, horizontal shaft, with a wide
  roller so it turns like a mouse wheel. A/B go to GPIO and C to GND, with
  10 nF from each of A and B to GND. Its body sits just right of the last
  key column (SW3 at x 314.3, y 52.025 mm, rotated 180°) with the shaft
  pointing right, toward the board edge, so the wheel can be about 17 mm
  wide. The PCB is about 12.7 mm wider than the layout.
- **USB-C:** 5.1 kΩ pull-downs on CC1 and CC2, and ESD protection on D+/D−
  (e.g. USBLC6-2).
- **Antenna:** keep the MDBT50Q antenna keep-out free of copper on all layers,
  at the board edge.
