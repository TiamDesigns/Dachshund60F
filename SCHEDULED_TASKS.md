# Scheduled tasks

Open work to pick up in later prompts. Each task is written so it can be pasted into a prompt. When a task is finished, delete it here in the same change, and record what was done in `PROGRESS.md`.

Last updated: 2026-10-23.

## Before ordering boards

- **Order the boards (Tiam, planned for the week of 2026-10-23).** Mainboard, USB-C daughterboard and dongle. Ordering also locks the proposed pin map in `boards/Dachshund60F/board.json`.

## Firmware and app, testable before boards arrive (in phase order)

- **Phase 9b: advanced keys in the firmware and protocol.** The core engine has them (phase 9a, `kb/features.h`); the firmware doesn't use them yet. Give `keyboard.c` a `struct kb_features`, save it as new settings sections (no schema bump needed), feed the encoder driver to `kb_engine_encoder()`, and add a mouse HID report for mouse keys. Add protocol commands to read and write each table and the macro buffer in chunks, validated with `kb_features_valid()`, and the `advanced_keys`, `macros` and `encoders` capabilities. Then the matching `HidBoard` methods in host-lib, tested against `kb_host_device`.
- **Phase 9c: Doghouse editors for advanced keys.** Pick `M(n)`, `TD(n)` and the new keycodes in the keymap editor; edit combos, tap dances, key overrides, Leader sequences, macros, per-key tap-hold rules and the encoder per layer; mock board support for all of it.
- **Matrix test mode and a Doghouse key tester.** Protocol v1 has the `matrix_stream` capability bit but no command for it. Add one (a new command isn't a breaking change), stream raw matrix state while Doghouse's key tester is open, and light up pressed keys. The bring-up plan's smoke test and every-key check want it.
- **Settings over the protocol (with phase 10).** The firmware stores debounce, tapping term, NKRO, sleep timeout, brightness and the board name (phase 6), but protocol v1 can't read or write them, so Doghouse hides its settings tab for real boards. Add get/set settings commands and the `settings` capability; `HidBoard.getSettings`/`setSettings` then replace their `not-supported` errors.
- **Phase 13 note: battery reading with the power path.** On USB, VDDH shows the USB-fed VSYS, so read the cell only on battery and show charging from CHG_STAT (P0.15).

## Tiam's own items

- **Case and mounting.** The mainboard needs no mounting holes (the switches hold it and the plate is gasket-sandwiched). Still open: the daughterboard screws (M2 or M3) and the dongle enclosure.
- **Display panel.** Pick OLED or e-paper (`firmware/displays/README.md`) and buy one so it arrives with the boards.
- **pid.codes PID.** Make the repo public, then apply for a registered PID before any board is given away or sold.
- **Optional: an nRF52840 Dongle (PCA10059)** to test the USB phases on real silicon before the boards arrive. It is already a CI build target.
