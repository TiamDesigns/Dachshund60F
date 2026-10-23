# Scheduled tasks

Open work to pick up in later prompts. Each task is written so it can be pasted into a prompt. When a task is finished, delete it here in the same change, and record what was done in `PROGRESS.md`.

Last updated: 2026-10-23.

## Before ordering boards

- **Order the boards (Tiam, planned for the week of 2026-10-23).** Mainboard, USB-C daughterboard and dongle. Ordering also locks the proposed pin map in `boards/Dachshund60F/board.json`.

## Firmware and app, testable before boards arrive (in phase order)

- **Phase 4: full HID over USB.** NKRO report with boot-protocol fallback, consumer and system control, Caps/Num/Scroll LEDs from the host, suspend, resume and remote wakeup. Host-test report packing and boot/NKRO switching in `firmware/core/`.
- **Phase 5: protocol v1.** `protocol/protocol.yaml`, code generators, the firmware parser and vendor HID interface, and parser fuzzing in CI.
- **Phase 6: persistence.** Versioned, CRC-checked, wear-levelled settings with defaults, factory reset and migration, tested against a simulated flash.
- **Phase 7: host library.** hidapi and WebHID transports, hotplug, request/response matching, version negotiation; round-trip tests against the mock and against the core built for the host.
- **Phase 8: Doghouse MVP on host-lib.** Wire the existing app to the real typed API.
- **Phase 9: advanced keys and encoders.** Tap-hold options, combos, tap dance, macros, key overrides, Caps Word, Auto Shift, Leader, Repeat, mouse keys and encoder mapping, host-tested. Code may be borrowed from ZMK or RMK (MIT/Apache), not QMK (GPL).
- **Phase 13 note: battery reading with the power path.** On USB, VDDH shows the USB-fed VSYS, so read the cell only on battery and show charging from CHG_STAT (P0.15).

## Tiam's own items

- **Case and mounting.** The mainboard needs no mounting holes (the switches hold it and the plate is gasket-sandwiched). Still open: the daughterboard screws (M2 or M3) and the dongle enclosure.
- **Display panel.** Pick OLED or e-paper (`firmware/displays/README.md`) and buy one so it arrives with the boards.
- **pid.codes PID.** Make the repo public, then apply for a registered PID before any board is given away or sold.
- **Optional: an nRF52840 Dongle (PCA10059)** to test the USB phases on real silicon before the boards arrive. It is already a CI build target.
