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

No board exists yet. Until it does, CI builds the firmware against Nordic's
`nrf52840dk/nrf52840` and the dongle against `nrf52840dongle/nrf52840`, using
only those boards' own devicetree.
