# boards/

One folder per board: `boards/<name>/`. This is the only source of pin
assignments. If a pin is missing here, firmware work stops and asks.

Each board folder will hold:

- **Pin map** — GPIO for every row, column, indicator LED, encoder A/B, BOOT
  button, battery-sense divider, and any I2C bus.
- **Matrix** — rows × columns and diode direction (COL2ROW or ROW2COL).
- **Keyboard definition JSON** — KLE-style physical key positions, sizes and
  rotation mapped to matrix (row, col); layout options; encoder positions;
  LED-to-key mapping. Read by the GUI.
- **Zephyr board files** — devicetree and Kconfig for the custom PCB
  (including the flash partition layout the UF2 bootloader expects).
- **Bootloader board files** (optional) — `bootloader/` with the board
  definition for the Adafruit nRF52 UF2 bootloader and a script that builds it.

Boards:

- `Dachshund60F/`: MDBT50Q-P1MV2, 79 keys + encoder. Its pin map is proposed and
  not yet backed by a PCB. The `dongle` section of its `board.json` holds the
  pins of its 2.4 GHz receiver (`hardware/Dachshund60F-Dongle/`), a second
  nRF52840; the host-lib tests check those pins too.

- `Pup/`: a one-key test board, the keyboard firmware on Nordic's nRF52840
  Dongle (PCA10059), for trying Bluetooth and the 2.4 GHz dongle before the
  PCB exists. It builds on Zephyr's own `nrf52840dongle/nrf52840` board, so it
  has no Zephyr board files: its pins and flash layout are in
  `Pup/firmware/board.overlay`, its default keymap next to it.

Required files per board: `keyboard.json` (layout) and `board.json` (pins,
USB IDs, power). The host-lib tests validate both for every board folder.

Until the Zephyr board files exist (phase 2), CI builds the firmware against
Nordic's `nrf52840dk/nrf52840` and the dongle against `nrf52840dongle/nrf52840`.
