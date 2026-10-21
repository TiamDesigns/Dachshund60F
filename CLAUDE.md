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
- GUI: TypeScript UI; shell TBD (Tauri recommended for signing and auto-update, or a WebHID web app)
- Board: `Dachshund60F` (MDBT50Q-1MV2, 6×15 COL2ROW matrix, 1 encoder, 2× SK6812MINI-E). Pins are in `boards/Dachshund60F/board.json` and are still proposed, since no PCB exists yet.
- USB VID/PID: pid.codes **test** PIDs 0x1209:0x0001 (keyboard) and 0x1209:0x0002 (dongle). They're for in-house testing only; get a registered pid.codes PID before any board is sold or given away.

## Repo layout

- `protocol/`: protocol definition (`protocol.yaml`) and code generators
- `firmware/`: board support, core keyboard logic, USB, BLE, storage
- `dongle/`: 2.4 GHz receiver firmware (second nRF52840)
- `host-lib/`: hidapi and WebHID transports, typed API, mock device
- `app/`: the GUI
- `boards/<name>/`: pin map, matrix and keyboard definition JSON

## Commands

Firmware commands run from an NCS v3.4.1 shell (nRF Connect for VS Code, or `nrfutil sdk-manager` / `nrfutil toolchain-manager launch`), with the repo as the current directory. They build the apps as freestanding applications. For a self-contained workspace instead, clone the repo into `<ws>/kb`, then `cd <ws> && west init -l kb && west update`. Keep `<ws>` out of OneDrive; NCS is several GB.

- Build firmware: `west build -p -b nrf52840dk/nrf52840 -d build/firmware firmware` (until `boards/<name>/` exists)
- Build dongle: `west build -p -b nrf52840dongle/nrf52840 -d build/dongle dongle`
- Release build (logging compiled out): add `-- -DEXTRA_CONF_FILE=release.conf`
- Flash (UF2): not available yet. Phase 2 adds the custom board with the UF2 bootloader's partition layout and `CONFIG_BUILD_OUTPUT_UF2=y`; then copy `zephyr.uf2` onto the bootloader drive. Until then, flash a DK over SWD with `west flash -d build/firmware`.
- Run core (keymap) tests: `cmake -S firmware/core -B firmware/core/build && cmake --build firmware/core/build && ctest --test-dir firmware/core/build --output-on-failure`
- Run host-lib tests (also validates every `boards/<name>/` definition): `cd host-lib && npm ci && npm test`
- Run the GUI against the mock device: not available yet (phase 8)
- CI: `.github/workflows/ci.yml` runs core tests, host-lib tests, and debug and release firmware builds for both apps

## Hard rules

- Keymap logic (keycodes, layers, tap-hold, combos, macros) stays independent of hardware and of the transport (USB/BLE/dongle), and is covered by host unit tests with simulated key events and timestamps.
- `protocol/protocol.yaml` is the only place commands are defined. Regenerate bindings; never hand-edit generated files.
- Bump the protocol version on any breaking change, and keep the app refusing writes on a version mismatch.
- Firmware validates every index and length from the host. A malformed packet must never crash the board or corrupt flash.
- Settings in flash are versioned, CRC-checked and wear-levelled. Schema changes come with a migration.
- Never log typed keystrokes in release builds.
- Pin assignments come only from `boards/<name>/`. If a pin is missing, ask; never guess.

## Working style

- Do only the phase I name, then stop and summarise what changed and how to test it.
- Write tests alongside features, not after.
- Ask before adding a dependency, changing the repo layout, or changing the protocol frame format.
- At the end of every session, update `PROGRESS.md`: current phase, what is done, what has been tested on real hardware, known bugs.
