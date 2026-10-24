# Scheduled tasks

Open work to pick up in later prompts. Each task is written so it can be pasted into a prompt. When a task is finished, delete it here in the same change, and record what was done in `PROGRESS.md`.

Last updated: 2026-10-24 (phase 14).

## Before ordering boards

- **Order the boards (Tiam, planned for the week of 2026-10-23).** Mainboard, USB-C daughterboard and dongle. Ordering also locks the proposed pin map in `boards/Dachshund60F/board.json`.

## Firmware and app, testable before boards arrive (in phase order)

- **Encoder on real hardware (bring-up).** Check that one click is one step with `CONFIG_KB_ENCODER_STEPS_PER_DETENT=4` and that clockwise is clockwise; if not, change the setting or swap A and B in the devicetree.
- **Matrix test mode and a Doghouse key tester.** Protocol v1 has the `matrix_stream` capability bit but no command for it. Add one (a new command isn't a breaking change), stream raw matrix state while Doghouse's key tester is open, and light up pressed keys. The bring-up plan's smoke test and every-key check want it.
- **Firmware update on real hardware.** Run Doghouse's About → Update firmware… against a board (or an nRF52840 dongle running the Adafruit bootloader): the drive picker must accept the DACHSHUND drive's root, the write must complete before the bootloader resets, and the board must come back over WebHID by itself. Check Windows and macOS.
- **Bluetooth on real hardware.** Pair with Windows, macOS, Linux, iOS and Android; check that NKRO and the media keys work over Bluetooth on each (fall back to 6KRO over Bluetooth if a host misbehaves), that unplugging the cable moves typing to Bluetooth without a stuck key, that BT1-BT3 and BT Clr behave, and that Doghouse finds the board over Bluetooth in Chrome (WebHID) with an ATT MTU of at least 67. Measure the key-to-host latency.
- **Keyboard firmware on an nRF52840 Dongle, to try Bluetooth before the boards arrive.** A test build with the dongle's button as the only key needs a `boards/` entry for it (pins come only from `boards/`), so ask Tiam first.
- **Undo for advanced keys, macros, settings and colours.** Doghouse's undo and redo cover keys and encoders only; the Advanced, Macros and Settings tabs apply straight to the board (Revert still undoes everything unsaved).
- **Power management on real hardware (phase 13).** With the PPK2 at 3.7 V on J2 (bring-up plan, Stage 4), measure the current while typing, idle with the lights on, lights out, advertising paused and in deep sleep. Check that a key wakes the board from deep sleep and how long it takes to type again over Bluetooth, and that the LED walk is skipped after a wake (if the UF2 bootloader clears the reset reason, keep a wake marker in GPREGRET2 instead). Check the gauge against a real cell (VDDH/5 reading, the curve in `kb/battery.c`) and CHG_STAT while charging. Tune `CONFIG_KB_IDLE_MS`, `CONFIG_KB_LIGHTS_OFF_S` and `CONFIG_KB_ADVERTISING_OFF_S` from the results.
- **2.4 GHz dongle on real hardware (phase 14).** Flash an nRF52840 Dongle (`dongle/README.md`) and pair it with a board (or a second nRF52840 running the keyboard firmware, once it has a `boards/` entry). Check: pairing from the dongle button plus `RF_PAIR` and from Doghouse; typing with `OUT_2G4`; lock LEDs following the computer; Doghouse reaching the keyboard through the dongle, and showing nothing (then the board again) while the keyboard sleeps; unplugging the dongle mid-keypress leaves nothing stuck. Measure range through a desk, dropouts next to busy 2.4 GHz Wi-Fi, key-to-USB latency (logic analyser on a column and the USB D+ line, or the bring-up plan's end-to-end method), the keyboard's current while polling at 10 ms and 100 ms, and typing over the dongle while Bluetooth stays connected (MPSL timeslots). If 2475 MHz is noisy, add channel hopping.
- **Encrypt the 2.4 GHz link.** Keys over the dongle are sent in the clear. Exchange a key at pairing (ECDH on both nRF52840s, or a key from the dongle in the PAIR_OFFER, accepting the pairing-window risk), then encrypt each packet with AES-CCM (the nRF52840's CCM peripheral, or the ECB block with a packet counter), with replay protection. Store the key with the dongle address on the keyboard and in the dongle's settings. Bump nothing in the config protocol; the link format in `kb/link.h` changes, so keyboard and dongle must update together.
- **Dongle firmware updates.** The dongle is flashed with nrfutil today. For the Dachshund dongle PCB, settle its bootloader (see the open decision in `PROGRESS.md`), then let Doghouse update it like the keyboard (a reboot-to-bootloader command over the dongle's own config interface, which today relays everything to the keyboard).
- **Input delay on real hardware.** Read Doghouse's About → Input timing after a minute of typing (matrix read time, key-to-report time, how many scans started on a key press), and compare with the end-to-end method in Stage 4 of the bring-up plan. If the matrix read takes long, try a lower `CONFIG_KB_MATRIX_SETTLE_US`.

## Tiam's own items

- **Case and mounting.** The mainboard needs no mounting holes (the switches hold it and the plate is gasket-sandwiched). Still open: the daughterboard screws (M2 or M3) and the dongle enclosure.
- **Display panel.** Pick OLED or e-paper (`firmware/displays/README.md`) and buy one so it arrives with the boards.
- **Optional: an nRF52840 Dongle (PCA10059)** to test the USB phases on real silicon before the boards arrive. It is already a CI build target.
