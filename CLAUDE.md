# CLAUDE.md

## Project

Custom tri-mode keyboard (USB, Bluetooth LE, 2.4 GHz dongle) built on the nRF52840, with our own firmware, a config protocol over a vendor-defined HID interface, a host library (the "driver"), and a configurator GUI. One monorepo.

- `SPEC.md` is the source of truth for scope and build order. Read it before starting any phase.
- Checkboxes in SPEC.md mean "in scope for v1", not "done". Do not tick or untick them.
- Anything under "Not in v1" in SPEC.md is out of scope. Do not build it unless asked.

## Stack

- Firmware: C11 on Zephyr / nRF Connect SDK v3.4.1 (pinned in `west.yml`); keyboard and dongle are separate Zephyr apps
- Core keymap logic: plain C11 in `firmware/core/`, host-tested with CMake/CTest and the header-only harness in `firmware/core/tests/kb_test.h`
- Bootloader: UF2 (Adafruit nRF52 bootloader)
- Host library: TypeScript (Node ≥ 20, ESM), `node-hid` on desktop and WebHID in the browser; tests use `node:test`
- GUI: **Doghouse** in `app/`, Svelte 5 + TypeScript + Vite. It runs in the browser against mock boards for now; a Tauri shell comes later (needs Rust and the MSVC build tools). Design language: warm earth tones, dog-themed (boards are named after dogs).
- Board: `Dachshund60F` (MDBT50Q-1MV2, 6×15 COL2ROW matrix, 1 encoder, 2× SK6812MINI-E). Pins are in `boards/Dachshund60F/board.json` and are still proposed, since no PCB exists yet.
- USB VID/PID: pid.codes **test** PIDs 0x1209:0x0001 (keyboard), 0x1209:0x0002 (dongle) and 0x1209:0x0003 (UF2 bootloader). They're for in-house testing only; get a registered pid.codes PID before any board is sold or given away.

## Repo layout

- `protocol/`: protocol definition (`protocol.yaml`) and code generators
- `firmware/`: board support, core keyboard logic, USB, BLE, storage
- `dongle/`: 2.4 GHz receiver firmware (second nRF52840)
- `host-lib/`: hidapi and WebHID transports, typed API, mock device
- `app/`: the GUI
- `boards/<name>/`: pin map, matrix and keyboard definition JSON
- `hardware/<name>/`: KiCad 10 schematic and PCB project. The Konnect KiCad MCP server is registered in `.mcp.json`; its PCB tools need KiCad running with the board open and the IPC API server enabled.

## Commands

Firmware commands run from an NCS v3.4.1 shell (nRF Connect for VS Code, or `nrfutil sdk-manager` / `nrfutil toolchain-manager launch`), with the repo as the current directory. They build the apps as freestanding applications. For a self-contained workspace instead, clone the repo into `<ws>/kb`, then `cd <ws> && west init -l kb && west update`. Keep `<ws>` out of OneDrive; NCS is several GB.

- Build firmware: `west build -p -b dachshund60f/nrf52840 -d build/firmware firmware -- -DBOARD_ROOT="$PWD"` (`BOARD_ROOT` must be an absolute path to the repo root, where Zephyr finds `boards/Dachshund60F/board.yml`)
- Build with the status screen: add `-DEXTRA_DTC_OVERLAY_FILE=displays/<panel>.overlay` after the `--`. Panels are listed in `firmware/displays/README.md`: `oled-ssd1306-128x32`, `oled-ssd1306-128x64`, `oled-sh1106-128x64` and `eink-2in13-v4`. Without one, the firmware has no screen.
- Build dongle: `west build -p -b nrf52840dongle/nrf52840 -d build/dongle dongle`
- Release build (logging compiled out): add `-DEXTRA_CONF_FILE=release.conf` after the `--`
- Flash (UF2): double-tap reset, or hold Esc for 3 s, so the board mounts as a UF2 drive. Then copy `build/firmware/firmware/zephyr/zephyr.uf2` onto it, or run `west flash -d build/firmware` (the default runner is `uf2`).
- Build the UF2 bootloader (Linux or WSL; needs `arm-none-eabi-gcc`, CMake, Ninja and Python `intelhex`): `boards/Dachshund60F/bootloader/build.sh`. CI builds it too and uploads `dachshund60f_bootloader_s140_6.1.1.hex` as the `bootloader-<sha>` artifact.
- First-time bootloader flash: program that hex over the SWD header with a chip erase (`nrfutil device program --firmware <hex> --options chip_erase_mode=ERASE_ALL`, or pyocd/J-Link). Steps and checks are in `boards/Dachshund60F/bootloader/README.md`.
- Debug logs: SEGGER RTT over SWD (the board has no UART). Release builds have none.
- Run core (keymap) tests: `cmake -S firmware/core -B firmware/core/build && cmake --build firmware/core/build && ctest --test-dir firmware/core/build --output-on-failure`
- Run host-lib tests (also validates every `boards/<name>/` definition): `cd host-lib && npm ci && npm test`
- Regenerate the protocol bindings after editing `protocol/protocol.yaml`: `python protocol/gen/gen.py` (needs PyYAML, which NCS has). CI fails if the generated files are stale (`--check`).
- Fuzz the protocol parser (Linux, clang): `cd firmware/core && clang -g -O1 -fsanitize=fuzzer,address,undefined -Iinclude -Itests fuzz/fuzz_protocol.c src/*.c -o fuzz_protocol && ./fuzz_protocol -max_total_time=60 corpus`. A deterministic random-frame test (`test_protocol_fuzz`) also runs with the core tests.
- Export STEP models of every board for case design: `python hardware/export_step.py` (writes `hardware/step/`, which is committed so the files reach every device; commit a fresh export when you want a snapshot; needs KiCad 10's `kicad-cli`)
- Run the GUI against the mock device: `cd app && npm install && npm run dev`, then open http://localhost:5173. Check, test and build with `npm run check`, `npm test` and `npm run build`.
- CI: `.github/workflows/ci.yml` runs core tests (plus the protocol generator check), host-lib tests, the Doghouse app (check, test, build), two minutes of protocol fuzzing, and debug and release firmware builds. It runs on pull requests and on pushes to `main`, so a branch gets CI once it has a PR (or from the Actions tab with "Run workflow").

## Hard rules

- Keymap logic (keycodes, layers, tap-hold, combos, macros) stays independent of hardware and of the transport (USB/BLE/dongle), and is covered by host unit tests with simulated key events and timestamps.
- `protocol/protocol.yaml` is the only place commands are defined. Regenerate bindings; never hand-edit generated files.
- Bump the protocol version on any breaking change, and keep the app refusing writes on a version mismatch.
- Firmware validates every index and length from the host. A malformed packet must never crash the board or corrupt flash.
- Settings in flash are versioned, CRC-checked and wear-levelled. Schema changes come with a migration.
- Never log typed keystrokes in release builds.
- Pin assignments come only from `boards/<name>/`. If a pin is missing, ask; never guess.

## Licence

- Code is MIT (`LICENSE`); hardware under `hardware/` is CERN-OHL-P-2.0 (`hardware/LICENSE`). Third-party files keep their own licences.

## Working style

- Do only the phase I name, then stop and summarise what changed and how to test it.
- Write tests alongside features, not after.
- Ask before adding a dependency, changing the repo layout, or changing the protocol frame format.
- At the end of every session, update `PROGRESS.md`: current phase, what is done, what has been tested on real hardware, known bugs.
- `SCHEDULED_TASKS.md` lists open tasks to pick up in later prompts. Delete a task from it in the same change that finishes it; add new open tasks as they come up.
