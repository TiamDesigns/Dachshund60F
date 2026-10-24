# Scheduled tasks

Open work to pick up in later prompts. Each task is written so it can be pasted into a prompt. When a task is finished, delete it here in the same change, and record what was done in `PROGRESS.md`.

Last updated: 2026-10-24.

## Before ordering boards

- **Order the boards (Tiam, planned for the week of 2026-10-23).** Mainboard, USB-C daughterboard and dongle. Ordering also locks the proposed pin map in `boards/Dachshund60F/board.json`.

## Firmware and app, testable before boards arrive (in phase order)

- **Encoder on real hardware (bring-up).** Check that one click is one step with `CONFIG_KB_ENCODER_STEPS_PER_DETENT=4` and that clockwise is clockwise; if not, change the setting or swap A and B in the devicetree.
- **Matrix test mode and a Doghouse key tester.** Protocol v1 has the `matrix_stream` capability bit but no command for it. Add one (a new command isn't a breaking change), stream raw matrix state while Doghouse's key tester is open, and light up pressed keys. The bring-up plan's smoke test and every-key check want it.
- **Firmware update on real hardware.** Run Doghouse's About → Update firmware… against a board (or an nRF52840 dongle running the Adafruit bootloader): the drive picker must accept the DACHSHUND drive's root, the write must complete before the bootloader resets, and the board must come back over WebHID by itself. Check Windows and macOS.
- **Bluetooth on real hardware.** Pair with Windows, macOS, Linux, iOS and Android; check that NKRO and the media keys work over Bluetooth on each (fall back to 6KRO over Bluetooth if a host misbehaves), that unplugging the cable moves typing to Bluetooth without a stuck key, that BT1-BT3 and BT Clr behave, and that Doghouse finds the board over Bluetooth in Chrome (WebHID) with an ATT MTU of at least 67. Measure the key-to-host latency.
- **Keyboard firmware on an nRF52840 Dongle, to try Bluetooth before the boards arrive.** A test build with the dongle's button as the only key needs a `boards/` entry for it (pins come only from `boards/`), so ask Tiam first.
- **Undo for advanced keys, macros, settings and colours.** Doghouse's undo and redo cover keys and encoders only; the Advanced, Macros and Settings tabs apply straight to the board (Revert still undoes everything unsaved).
- **Phase 13 note: battery reading with the power path.** On USB, VDDH shows the USB-fed VSYS, so read the cell only on battery and show charging from CHG_STAT (P0.15). Feed the level to the Bluetooth Battery Service with `kb_ble_set_battery()`, which still reports its default.
- **Phase 13 note: Bluetooth power.** The board advertises without a timeout and keeps the link up while USB is in use; stop advertising after a while and on sleep.

## Tiam's own items

- **Case and mounting.** The mainboard needs no mounting holes (the switches hold it and the plate is gasket-sandwiched). Still open: the daughterboard screws (M2 or M3) and the dongle enclosure.
- **Display panel.** Pick OLED or e-paper (`firmware/displays/README.md`) and buy one so it arrives with the boards.
- **Optional: an nRF52840 Dongle (PCA10059)** to test the USB phases on real silicon before the boards arrive. It is already a CI build target.
