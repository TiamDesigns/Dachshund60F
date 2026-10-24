# Dachshund60F UF2 bootloader

Board files for the [Adafruit nRF52 bootloader](https://github.com/adafruit/Adafruit_nRF52_Bootloader),
which gives the keyboard its UF2 drive. Pins come from `../board.json`; the
flash layout matches `../dachshund60f.dts`.

| File | What it is |
|---|---|
| `board.h` | Pins, LEDs, DFU key, USB IDs and UF2 strings |
| `pinconfig.c` | CF2 config block, plus `board_init2()` that makes Esc readable |
| `board.mk`, `board.cmake` | MCU variant (nrf52840) for Make and CMake |
| `build.sh` | Clones the bootloader at a pinned commit, adds these files, builds |

## What the bootloader does on this board

- **Entering UF2 mode:** double-tap reset, hold Esc while plugging in or
  pressing reset, or hold Esc for 3 s in the firmware (GPREGRET = 0x57). A board
  with no valid app also stays in UF2 mode.
- **Esc as the DFU key:** Esc is matrix (row 0, col 0), COL2ROW. `board_init2()`
  drives col 0 (P0.02) high, and the bootloader reads row 0 (P1.10) with a
  pull-down. This is the same scan the firmware uses.
- **Status light:** all six SK6812MINI-E LEDs (P0.06), powered through the
  TPS22917 (P0.08, active high), show the bootloader's colours at low brightness.
- **REGOUT0:** set to 3.3 V on first boot (`UICR_REGOUT0_VALUE`), as
  `../board.c` also does.
- **USB:** 1209:0003, "DACHSHUND" drive. This is a pid.codes **test** PID, for
  in-house testing only, like the firmware's 1209:0001.

## Flash layout

| Range | Contents |
|---|---|
| 0x00000–0x00FFF | MBR |
| 0x01000–0x25FFF | S140 SoftDevice 6.1.1 (ends at 0x25DE8) |
| 0x26000–0xE9FFF | App: `zephyr.uf2`. The bootloader accepts UF2 blocks only below 0xEA000 |
| 0xEA000–0xF3FFF | Reserved for app data (40 KB); DFU never erases it. Zephyr `bt_settings` (Bluetooth bonds, NVS) is 0xEA000–0xEBFFF and `storage` (keymap and settings) is 0xEC000–0xF3FFF |
| 0xF4000–0xFD7FF | Bootloader |
| 0xFE000–0xFFFFF | MBR params, bootloader settings |

## Build

Needs git, CMake, Ninja, `arm-none-eabi-gcc` and Python 3 with `intelhex`.
On Linux or WSL:

```sh
sudo apt install gcc-arm-none-eabi cmake ninja-build
pip install intelhex
boards/Dachshund60F/bootloader/build.sh    # from the repo root
```

The output lands in
`build/bootloader/Adafruit_nRF52_Bootloader/_build/dachshund60f/`. CI builds
the same files and uploads them as the `bootloader-<sha>` artifact. The two you
need are:

- `dachshund60f_bootloader_s140_6.1.1.hex`: MBR, SoftDevice and bootloader in
  one file, for the first flash over SWD.
- `bootloader_mbr.uf2`: updates the bootloader later by copying it onto the
  UF2 drive.

## First flash over SWD

Connect the probe to the SWD header (SWDIO, SWDCLK, nRESET, VDD, GND), power the
board, and erase the whole chip while programming. Use one of:

```sh
nrfutil device program --firmware dachshund60f_bootloader_s140_6.1.1.hex --options chip_erase_mode=ERASE_ALL,verify=VERIFY_READ
pyocd flash -t nrf52840 --erase chip dachshund60f_bootloader_s140_6.1.1.hex
JLinkExe -device NRF52840_XXAA -if SWD -speed 4000 -autoconnect 1   # then: erase, loadfile <hex>, r, g, q
```

Then:

1. Reset the board. It has no app yet, so the DACHSHUND drive appears and the
   LEDs show the bootloader colours.
2. Open `INFO_UF2.TXT` on the drive and check it reads
   `Board-ID: nRF52840-Dachshund60F-v1`.
3. Copy `build/firmware/firmware/zephyr/zephyr.uf2` onto the drive. The board
   resets into the firmware (1209:0001).
4. Hold Esc for 3 s and check the drive comes back. Then unplug, hold Esc, plug
   in, and check it comes back again.
