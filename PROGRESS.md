# PROGRESS

## Current phase

Phase 1 (Scaffold) is complete. Next is phase 2 (blinky and USB enumeration).

## Done

### Phase 1: Scaffold (2026-10-21)

- Stack locked: C on Zephyr / NCS v3.4.1 for firmware and dongle; TypeScript host-lib.
- `west.yml` pins NCS v3.4.1 for CI and for optional self-contained workspaces.
- `firmware/`: Zephyr app that boots and logs its version (`VERSION`, `prj.conf`, and `release.conf`, which compiles logging out).
- `firmware/core/`: hardware-independent C library with a standalone CMake/CTest host build and a header-only test harness. It holds only a placeholder API-version function for now.
- `dongle/`: Zephyr app skeleton.
- `host-lib/`: TypeScript ESM package with `tsc` and `node:test`, and one smoke test. Dev dependencies: `typescript` 7, `@types/node` 24.
- `protocol/`, `boards/` and `app/` hold READMEs only.
- `.github/workflows/ci.yml`: core tests, host-lib tests, and debug and release firmware builds (DK and dongle board targets).

## Tested

- host-lib `npm test`: passes locally (Node 24.12, Windows).
- Core C tests: not run locally because there is no C compiler or CMake on this machine. CI only.
- Firmware builds: not run locally because NCS is not installed. CI has not run either, since the repo is not under git or pushed yet.
- Real hardware: nothing yet.

## Open decisions

- USB VID/PID (needed for phase 2)
- Board name, pin map and matrix for `boards/<name>/` (needed for phases 2–3)
- Key count and layer count
- GUI shell: Tauri or a WebHID web app
- Licence

## Known bugs

None yet.
