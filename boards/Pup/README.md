# Pup

A one-key test board: the keyboard firmware on Nordic's nRF52840 Dongle
(PCA10059). Use it to try Bluetooth and the 2.4 GHz dongle before the
Dachshund60F PCB exists. With two dongles, one runs the Pup and the other
runs the receiver (`dongle/`).

- **Key:** SW1, the button on top, types `a` by default. Remap it in
  Doghouse, which finds the Pup like any board (same USB IDs as the
  Dachshund60F; the model name `Pup` loads `keyboard.json`).
- **LED:** LD2, the RGB LED, walks red, green and blue at power-up, then
  shows Caps Lock in the Caps Lock colour from Doghouse (each colour is on or
  off, so dim colours come out bright). LD1, the green LED, is unused.
- **Everything else is the Dachshund60F's firmware:** USB, Bluetooth with
  three host profiles, the 2.4 GHz link, settings, the config protocol.
  There is no battery, encoder or screen.

## Files

- `board.json` and `keyboard.json`, as for every board. The pins come from
  Nordic's documentation for the PCA10059 (and Zephyr's board files for it).
- `firmware/board.overlay`: the button, the RGB LED and the flash layout,
  on top of Zephyr's `nrf52840dongle/nrf52840` board.
  `firmware/boards/nrf52840dongle_nrf52840.overlay` includes it, so Zephyr
  picks it up by board name.
- `firmware/default_keymap.c` and `board_keymap.h`: the default keymap
  (picked by the `KB_MODEL` name, `firmware/CMakeLists.txt`).
- `firmware/boards/nrf52840dongle_nrf52840.conf` (in the firmware app): the
  names, the bootloader and the longer recovery hold.

The button is wired straight to its pin, with no matrix
(`firmware/src/matrix.h`: a board without column pins reads each row pin as
one key).

## Build

From an NCS v3.4.1 shell, in the repo:

```
west build -p -b nrf52840dongle/nrf52840 -d build/pup firmware
```

Add `-- -DEXTRA_CONF_FILE=release.conf` for a release build. No
`BOARD_ROOT` is needed: the board is Zephyr's own.

## Flash

The PCA10059 ships with Nordic's USB bootloader, and the build sits after
it at 0x1000, the same as the receiver:

1. `nrfutil nrf5sdk-tools pkg generate --hw-version 52 --sd-req=0x00 --application build/pup/firmware/zephyr/zephyr.hex --application-version 1 pup.zip`
2. Press the sideways RESET button (SW2). The red LED pulses and a serial
   port appears. Holding SW1 for 10 s does the same once the Pup runs.
3. `nrfutil nrf5sdk-tools dfu usb-serial -pkg pup.zip -p <port>`

Doghouse's firmware update writes UF2 files and won't work here; use
nrfutil. The settings sit at 0xd6000 to 0xe0000, just below the bootloader.
An update should leave them alone, but that isn't guaranteed, so expect to
redo a custom keymap and pairings after one.

## Try the wireless outputs

Plugged into a computer, the Pup types over USB. Then:

- **Bluetooth:** in Doghouse, Connection → prefer Bluetooth, and pair
  "Dachshund Pup" from the computer's Bluetooth settings. Or plug the Pup
  into a USB charger, with no computer, so USB isn't there to fall back to.
- **2.4 GHz:** flash a second dongle with the receiver (`dongle/README.md`),
  press its button, then in Doghouse Connection → Pair a dongle, and
  prefer 2.4 GHz.

The single key can't hold `OUT_*`, `BT_SEL` or `RF_PAIR` alongside a
letter, so switch outputs from Doghouse, or map the key to one of them.
