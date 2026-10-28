# Dachshund60F

A tri-mode 60% keyboard I'm designing from scratch: the PCBs, the switch plate, the firmware, the config protocol, the driver and the configurator app. It runs over USB, Bluetooth LE and its own 2.4 GHz dongle, all on the nRF52840.

![Dachshund60F layout](KLE%20Info/keyboard-layout.jpg)

No boards have been made yet. All three PCBs are designed and the firmware is built and tested on a PC, so the next step is ordering them and bringing them up.

## Why I'm building this

I'm using this project to do two things at once:

1. **Explore hardware design with AI.** I want to see how far I can push an AI-assisted workflow on a real product: schematics, PCB layout, firmware, tooling and docs. I make the decisions and review everything, and Claude Code does a lot of the legwork. This repo is the record of how that went.
2. **Learn high-speed peripherals properly.** A keyboard looks simple, but doing it right covers USB device design, Bluetooth LE, a proprietary 2.4 GHz radio link, SPI and I2C displays, addressable LEDs, flash storage and power management. I want to understand each one well enough to design it, not just copy it.

And at the end of it I want a keyboard I actually enjoy using every day.

## Project goals

### Hardware

- Design all the boards myself in KiCad: the mainboard, a USB-C daughterboard and the dongle, plus an FR4 switch plate.
- Use a pre-certified nRF52840 module (Raytac MDBT50Q-P1MV2) and get the antenna keep-out, power and USB right the first time.
- Make it nice to type on: hot-swap switches, screw-in stabilisers, a gasket-mounted plate, and a horizontal roller encoder that turns like a mouse wheel.
- Run from USB or a single LiPo cell, with proper charging and a power path so it still works with the battery switched off.
- Measure everything when the boards arrive: rails, current draw in every state, scan rate, latency and radio range. The plan is in [docs/bringup-test-plan.md](docs/bringup-test-plan.md).

### Firmware

- Write my own firmware on Zephyr / nRF Connect SDK, not a fork of QMK or ZMK.
- Keep the keymap logic in plain C with no hardware in it, so it runs as unit tests on a PC.
- Be a complete USB keyboard: NKRO with boot-protocol fallback for BIOS and KVMs, media keys, lock LEDs, and suspend and remote wakeup.
- Support the advanced stuff: tap-hold, combos, tap dance, macros, key overrides, Caps Word, Auto Shift, Leader, mouse keys and the encoder.
- Never crash or corrupt flash on a bad packet. Every index and length from the host is checked, and the parser is fuzzed.

### Wireless

- Bluetooth LE HID with several host profiles and a switch key.
- A 2.4 GHz dongle on a second nRF52840 with low latency, polling at 1000 Hz (the limit for full-speed USB).
- Battery reporting, idle sleep and wake on key press.

### Driver and app

- No kernel driver. The board shows up as a normal HID keyboard plus one vendor-defined HID interface for configuration, so it works anywhere with nothing to install.
- A TypeScript host library (the "driver") that talks to that interface over WebHID in the browser or node-hid on the desktop.
- **Doghouse**, the configurator: keymap editing, advanced keys, macros, settings and firmware updates. It runs in Chrome or Edge, or as a desktop app for Windows, macOS and Linux that updates itself. It's in warm earth tones and dog-themed, because the boards are named after dogs.

### Doing it properly

- One protocol definition (`protocol/protocol.yaml`), with the C and TypeScript code generated from it.
- Versioned, CRC-checked, wear-levelled settings in flash, with migrations.
- CI that runs the unit tests, the host-library tests, the app build, protocol fuzzing, and debug and release firmware builds on every PR.
- Write down what changed and what's been tested in [PROGRESS.md](PROGRESS.md).

## The board

| | |
| --- | --- |
| MCU | nRF52840 (Raytac MDBT50Q-P1MV2 module) |
| Layout | 60% with an F-row, 80 switch positions, split or 6.25u spacebar |
| Matrix | 6 × 15, COL2ROW, diodes on every switch |
| Switches | Kailh hot-swap sockets (3 soldered positions on the spacebar row) |
| Encoder | Bourns PEC12R-2230F-S0024: shaft parallel to the board, 24 detents, push switch, 30 mm shaft, used as a roller |
| Lighting | 6 × SK6812MINI-E indicators: layer, a 4-step battery gauge, and Caps Lock. No per-key RGB |
| Display | Optional OLED or e-paper on header J4 (panel not chosen yet) |
| Power | USB-C on a daughterboard, 1S LiPo on JST-PH, MCP73831 charger with a load-sharing power path |
| Connectivity | USB, Bluetooth LE, 2.4 GHz dongle |
| Bootloader | Adafruit nRF52 UF2 bootloader: double-tap reset and drag a `.uf2` onto the drive |
| USB IDs | pid.codes test PIDs 1209:0001 (keyboard), 1209:0002 (dongle), 1209:0003 (bootloader) |

## Where it's at

The firmware follows the build order in [SPEC.md](SPEC.md): wired first, then wireless.

| Phase | What it covers | Status |
| --- | --- | --- |
| 1 | Scaffold: monorepo, CI, Zephyr app | Done |
| 2 | Board bring-up, USB enumeration, UF2 bootloader | Done |
| 3 | Matrix scan, debounce, keycodes, layers, tap-hold | Done |
| 4 | Full USB HID: NKRO, media keys, lock LEDs, sleep and wake | Done |
| 5 | Config protocol v1 over a vendor HID interface | Done |
| 6 | Saving settings to flash | Done |
| 7 | Host library (WebHID and node-hid) | Done |
| 8 | Doghouse on real boards | Done |
| 9 | Advanced keys, macros and the encoder | Done |
| 10 | Indicator colours and global settings | Done |
| 11 | In-app firmware updates | Done |
| 12 | Bluetooth LE | Done |
| 13 | Power management, battery and input delay | Done |
| 14 | 2.4 GHz dongle firmware | Done |
| 15 | Desktop app packaging and auto-update | Done |

"Done" means built and tested on a PC and in CI. None of it has run on a real board yet, because there isn't one.

## Repo layout

| Folder | What's in it |
| --- | --- |
| `hardware/` | KiCad 10 projects: mainboard, USB-C daughterboard, dongle and plate, STEP exports, and the case (`hardware/case/`, CadQuery) |
| `boards/Dachshund60F/` | Pin map, keyboard definition, Zephyr board files and the UF2 bootloader board |
| `boards/Pup/` | A one-key test board: the keyboard firmware on Nordic's nRF52840 Dongle, for trying the wireless outputs before the PCB exists |
| `firmware/` | The keyboard firmware. `firmware/core/` is the hardware-free keymap engine and its tests |
| `dongle/` | 2.4 GHz receiver firmware (see `dongle/README.md` to flash and pair it) |
| `protocol/` | `protocol.yaml` and the code generator |
| `host-lib/` | The TypeScript host library, mock board and tests |
| `app/` | Doghouse, the configurator (Svelte 5 + Vite), and its desktop app in `app/src-tauri/` (Tauri) |
| `docs/` | Bring-up and test plan, and my design decisions |

## Trying it out

You don't need a board to play with most of it.

**Doghouse with demo boards**

```sh
cd app
npm install
npm run dev
```

Then open http://localhost:5173/?demo. To use a real board, open http://localhost:5173 in Chrome or Edge and click **Connect a board…**.

**Tests**

```sh
# keymap engine and protocol (C, runs on the PC)
cmake -S firmware/core -B firmware/core/build
cmake --build firmware/core/build
ctest --test-dir firmware/core/build --output-on-failure

# host library
cd host-lib && npm ci && npm test
```

**Firmware**

From an nRF Connect SDK v3.4.1 shell, in the repo root:

```sh
west build -p -b dachshund60f/nrf52840 -d build/firmware firmware -- -DBOARD_ROOT="$PWD"
```

[CLAUDE.md](CLAUDE.md) has every command, including release builds, display panels, the bootloader and flashing.

## Docs

- [SPEC.md](SPEC.md): scope and build order
- [PROGRESS.md](PROGRESS.md): what's done, what's tested, known issues
- [docs/design-decisions.md](docs/design-decisions.md): the decisions I made along the way and what I've learnt
- [docs/bringup-test-plan.md](docs/bringup-test-plan.md): how each board gets flashed, tested and measured
- [protocol/README.md](protocol/README.md): the config protocol
- [SCHEDULED_TASKS.md](SCHEDULED_TASKS.md): open tasks

## Licence and disclaimer

This is open source and open hardware. You're free to copy it, change it, build it and sell it.

- **Code** (firmware, host library, Doghouse, protocol, scripts) is under the [MIT License](LICENSE).
- **Hardware** (everything in `hardware/`: the three PCBs and the plate) is under the [CERN Open Hardware Licence v2, Permissive](hardware/LICENSE) (CERN-OHL-P-2.0).
- **Third-party files** keep their own licences. The keyswitch footprints and 3D models are CC-BY-SA-4.0 (see `hardware/Dachshund60F/lib/README.md`), and the bootloader build fetches Adafruit's MIT-licensed source.

Both licences are permissive. You can make your own boards, fork the firmware, put it in a product, or sell keyboards based on it, commercially or not. All I ask, and all the licences require, is that you keep the copyright and licence notices with what you copy.

Please read this before you build one:

- **No warranty.** This is a personal learning project, provided as is. At the time of writing no board has been manufactured or tested, so the designs may have mistakes. Check them yourself before ordering.
- **Lithium batteries can be dangerous.** The board charges a LiPo cell. Use a cell with its own protection circuit, check the connector polarity (JST-PH polarity isn't standard between vendors), and don't leave it charging unattended until you've tested your build.
- **USB IDs: change them before you give a board to anyone.** I've chosen not to register USB IDs for this project, because my board only lives on my desk. The firmware uses pid.codes *test* PIDs (1209:0001, 0002 and 0003), which anyone may use for in-house testing but which **must not** be used on a device that's given away, sold or manufactured. Every board using them looks identical to the OS, and to every other project that uses the same test IDs. If your boards will leave your hands, get your own VID/PID (a free PID from [pid.codes](https://pid.codes/howto/) for open-source hardware, or a VID from the USB-IF) and change them in `firmware/Kconfig`, the dongle and bootloader configs, and `boards/Dachshund60F/board.json`.
- **Certification.** The radio module is pre-certified, but a finished product may still need its own testing (FCC, CE and so on) before you sell it. That's on you.
