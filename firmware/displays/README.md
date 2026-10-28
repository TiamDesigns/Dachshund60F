# firmware/displays/

One devicetree overlay per supported panel on the Dachshund60F's J4 display
header. Pick one at build time:

```
west build -p -b dachshund60f/nrf52840 -d build/firmware firmware -- -DBOARD_ROOT="$PWD" -DEXTRA_DTC_OVERLAY_FILE=displays/oled-ssd1306-128x64.overlay
```

With no overlay, the firmware is built without a screen. CI builds a UF2 for
every panel below.

| Overlay | Panel | Bus | Text grid (10×16 font) |
|---|---|---|---|
| `oled-ssd1306-128x32` | 0.91" SSD1306, 128×32 | I2C 0x3C | 12 × 2 |
| `oled-ssd1306-128x64` | 0.96" SSD1306, 128×64 | I2C 0x3C | 12 × 4 |
| `oled-sh1106-128x64` | 1.3" SH1106, 128×64 | I2C 0x3C | 12 × 4 |
| `oled-sh1107-128x128` | 1.12" SH1107, 128×128 | I2C 0x3C | 12 × 8 |
| `eink-2in13-v4` | Waveshare 2.13" e-Paper V4 (GDEY0213B74, SSD1680), 250×122 | SPI | 25 × 7 |
| `eink-2in13-v4-portrait` | The same panel on end, 120×248 (the case's screen) | SPI | 12 × 15 |

The portrait overlay chooses a `dachshund,display-rotate` node
(`firmware/src/display_rotate.c`) as the display. It reports the panel's size
turned by 90°, and turns each frame back before writing it to the panel, so
the character framebuffer draws in portrait. The SSD16xx driver's own
`rotation` property doesn't do this: it keeps reporting the landscape size.

## Wiring

J4 follows the Waveshare e-Paper module order: 1 VCC (+3V3), 2 GND, 3 DIN,
4 CLK, 5 CS, 6 DC, 7 RST, 8 BUSY. A 4-pin I2C OLED uses VCC, GND, DIN (SDA)
and CLK (SCL). Pins come from `boards/Dachshund60F/board.json`. Overlays
refer to J4 pins only through `<&display_header PIN FLAGS>`; the host-lib
tests reject raw `&gpio` references here.

## What is shown

The screen draws the layout from `firmware/core/include/kb/screen.h`, a grid
of character cells. By default:

- Row 0: active layer (left) and battery (right)
- Row 1: lock keys (left) and connection (right)
- Row 2: custom text, centred, if the panel has the row

Kconfig options under "Dachshund60F keyboard → Status screen" change it
without code changes: `CONFIG_KB_SCREEN_TEXT`, the `CONFIG_KB_SCREEN_SHOW_*`
switches, `CONFIG_KB_SCREEN_FONT_INDEX`, `CONFIG_KB_SCREEN_INVERT` and
`CONFIG_KB_SCREEN_EPD_MIN_INTERVAL_MS`. Put the settings in a `.conf` file in
`firmware/`, for example `CONFIG_KB_SCREEN_TEXT="Hello"`, and add
`-DEXTRA_CONF_FILE=<that file>` after the `--` (separate several files with
`;`, e.g. together with `release.conf`).

At run time, `status_screen_set_layout()` replaces the whole layout (widget
positions, alignment, text and layer names), after validating it. Once the
config protocol exists (phase 5), the configurator will use it to edit the
screen.

Layer and battery currently show "Base" and "BAT --": the keymap arrives in
phase 3 and battery measurement in phase 13. Lock state and the USB
connection are live now.

## Adding a panel

Copy the panel's node from Zephyr's `boards/shields/` (ssd1306,
waveshare_epaper and others). Put it under `&display_i2c` or under a
`zephyr,mipi-dbi-spi` node using `&display_spi`, as the files here do. Then
replace the Arduino header references with `&display_header` J4 pin numbers,
and add the overlay to the CI matrix.
