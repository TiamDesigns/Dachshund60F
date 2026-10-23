# PROGRESS

## Current phase

Phase 3 (matrix and basic keymap): done and merged to main. Core tests pass locally and the firmware builds in CI. The Dachshund60F UF2 bootloader also builds in CI, ready for the first SWD flash. Hardware testing for phases 2 and 3 waits for fabricated boards.

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

### Board definition: Dachshund60F (2026-10-21, before phase 2)

- `boards/Dachshund60F/keyboard.json`: generated from `KLE Info/keyboard-layout(2).json`. It has 79 keys plus an alternate 6.25u spacebar (layout option), an encoder at x=15 right of Delete, and 2 indicators (Caps, layer).
- `boards/Dachshund60F/board.json`: proposed pin map for the MDBT50Q-1MV2, a 6×15 COL2ROW matrix, encoder P0.11/P0.12, SK6812 data on P0.06 with LED power enable on P0.08, charger status on P0.15, and battery measured through VDDH.
- USB IDs: pid.codes test PIDs, 1209:0001 for the keyboard and 1209:0002 for the dongle.
- `host-lib/test/boards.test.ts` validates every board folder: matrix range and uniqueness per layout option, no key overlaps, and pins that are valid, unique and not reserved.

### PCB tooling: KiCad MCP (2026-10-21)

- `.mcp.json` registers the Konnect v0.13.0 MCP server (mixelpixx/Konnect, AGPL-3.0, beta) for KiCad 10. It runs from the KiCad 3rd-party plugin folder.
- Konnect is installed through KiCad's Plugin and Content Manager, and KiCad's IPC API server is enabled. It connects from Claude Code with 228 tools, and KiCad 10.0.3 was detected.
- `hardware/Dachshund60F/`: the KiCad 10 project (`.kicad_pro`, `.kicad_sch`, `.kicad_pcb`).
- `.gitignore` excludes KiCad lock files, `.history/` backups, `*.kicad_prl` view state and `fp-info-cache`.
- The kbplacer and kleplacement KiCad plugins are already installed and can place switches from the KLE layout.

### PCB: Dachshund60F schematic and placement (2026-10-21)

Decisions: MX hot-swap (Kailh sockets), 2 layers, MCP73831 charger at 500 mA (2 kΩ PROG) for a 1000–2000 mAh cell, and USB-C on a separate daughterboard.

- Schematic: one A2 sheet with 195 parts. ERC is clean (0 errors, 0 warnings). A script checked all 29 module pin nets against `board.json` (0 mismatches).
- The board has no USB-C connector. J1 is a JST-SH 4-pin for the daughterboard, carrying VBUS, D−, D+ and GND in the Unified Daughterboard order (checked on 2026-10-22; see the daughterboard section).
- Power path: VBUS feeds the MCP73831, which charges VBAT (JST-PH battery). The slide switch connects VBAT to VSYS, which feeds VDDH (high-voltage mode). VDD (REGOUT0) is +3V3.
- Charge status: a BAT54WS Schottky sits between the MCP73831 STAT pin and P0.15. STAT drives 5 V when charging completes, and the diode passes only the low (charging) level.
- LEDs: the load switch is a TPS22917 (KiCad has no TPS22918 symbol); its QOD pin is unconnected and EN has a 100 kΩ pull-down.
- DCCH is unconnected, so firmware must run REG0 in LDO mode.
- Matrix: 80 switch positions with one 1N4148W each. The 6.25u bar (SW606) and the 2.75u split space (SW615) share diode D606. The encoder push is at (0,14) through D114.
- Footprints: the hot-swap and stabilizer footprints are vendored into `hardware/Dachshund60F/lib/keyswitch.pretty` (perigoso v2.4, CC-BY-SA-4.0) and converted to the KiCad 10 format; see `lib/README.md`.
- Spacebar: hot-swap sockets for the 6.25u bar and the split space physically overlap. SW604, SW606 and SW615 use solder-only 3-pin plate-mount footprints (stock `SW_Cherry_MX_*_Plate`); the rest of the board is hot-swap.
- Placement: switches, stabilizers and the encoder are on the front at KLE positions (19.05 mm pitch). All SMD parts are on the back. An 18 mm strip above the F-row holds the module (antenna at the top edge), power, connectors, reset and SWD. The outline is 306.8 × 138.1 mm.

### PCB: horizontal encoder, compact layout and routing (2026-10-21)

- Encoder: the EC11 is replaced by a Bourns PEC12R-4220F-S0024 (horizontal, 24 detents, push, 20 mm shaft), using the stock `RotaryEncoder_Bourns_Horizontal_PEC12R-2x17F-Sxxxx` footprint. A roller about 12 mm wide and 20 mm across sits on the shaft in the encoder's KLE column (x = 15u) and rolls front to back like a mouse wheel. The body sits just past 16u.
- Layout: the 18 mm top strip is gone. The module, charger, connectors, power switch, reset, SWD and LED load switch sit on the back in the empty column under the encoder. The module antenna faces the right edge (0.75 mm away), and the slide switch actuator is at the right edge.
- Outline: 318.5 × 121.1 mm (x 19–337.5, y 39–160.1), about 12.7 mm wider than 16u. Corner radius is 5 mm (changed from 1 mm on 2026-10-22). About 6 mm is the most the top-right corner allows before the encoder's mounting tab gets too close to the edge.
- Routing: Freerouting 2.5.0 (JAR on a portable Temurin 25 JRE) autorouted every signal and power net with 0 violations: 0.25 mm signal tracks, and a 0.3 mm power class for VBUS, VBAT, VSYS, LED_VDD, GND and +3V3. KiCad's Specctra export drops the module's antenna rule area, so a matching keep-out was added to the DSN before routing. The SES was imported with KiCad's own `pcbnew.ImportSpecctraSES`, and 118 unused fanout vias were removed.
- Ground: GND pours on F.Cu and B.Cu (0.3 mm clearance, thermal reliefs), about 690 GND stitching vias, and short via ties for boxed-in decoupling-cap pads.
- Design rules: the minimum track width is now 0.15 mm, because Freerouting necks tracks down to 0.187 mm at fine-pitch pads.

### Phase 2 firmware: board bring-up (2026-10-22, merged to main)

- Pin check: all 29 module pins in `board.json` match the routed PCB pad for pad, and USB, SWD and VDDH sit on the right pads. Diodes are COL2ROW in the PCB (cathode on the ROW net).
- Zephyr board `dachshund60f/nrf52840` (HWMv2) in `boards/Dachshund60F/`:
  - devicetree written from `board.json`
  - Adafruit UF2 flash layout: app at 0x26000, storage at 0xEC000, bootloader at 0xF4000
  - `zephyr.uf2` output, with uf2 as the default `west flash` runner, plus nrfutil, jlink and pyocd for SWD
  - P0.18 as nRESET, and REG0/REG1 left in LDO mode, since DCCH is unconnected
  - `board.c` sets UICR REGOUT0 to 3.3 V on first boot in high-voltage mode, then resets
- Raw pin groups (matrix, encoder, charge status) sit in `zephyr,user` until the phase 3, 9 and 13 drivers replace them.
- USB (device_next): boot-protocol HID keyboard on 1209:0001, "Dachshund" / "Dachshund60F". The serial number comes from the chip's factory ID, so it's unique per board; `bcdDevice` comes from `firmware/VERSION`; bMaxPower is 500 mA. VID, PID and strings are Kconfig options in `firmware/Kconfig`. It sends no key reports yet, and the lock-LED output report is stored but not shown.
- LED bring-up: enables the TPS22917 (P0.08), then lights one SK6812 at a time in chain order (layer, battery 1–4, Caps), cycling red, green and blue every 500 ms at low brightness. This shows both the chain order and the colour order.
- Reboot to UF2: hold Esc (row 0, col 0) for 3 s. That writes GPREGRET = 0x57 and resets. The phase 3 keymap action replaces this.
- Logging over SEGGER RTT on the SWD header, since the board has no UART. Release builds have no logging and no RTT.
- Core: `kb_version_bcd_device` and `kb_hold` (long-press detector, timestamp wrap-safe), with host tests.
- host-lib `firmware-board.test.ts`: the devicetree matrix, encoder, charger and LED pins, the LED chain length and the Kconfig USB IDs must all match `board.json`. A deliberately wrong DTS pin makes it fail.
- CI builds `dachshund60f/nrf52840` with `-DBOARD_ROOT` and uploads `zephyr.uf2`. Debug: 67.6 KB flash, 16.9 KB RAM. Release: 51.9 KB flash, 13.4 KB RAM. Both UF2 files start at 0x26000 with family ID 0xADA52840.

### Status screen on J4, OLED or e-paper (2026-10-22, merged to main)

The status display is now in scope: SPEC.md moved it out of "Not in v1", and added it under Tier 3 plus a layout editor in the GUI list. The panel is not chosen yet, so the firmware supports both kinds, picked at build time.

- Board devicetree:
  - `display_header` is a GPIO nexus indexed by J4 pin number: 3 DIN, 4 CLK, 5 CS, 6 DC, 7 RST, 8 BUSY. Its binding is in `boards/Dachshund60F/dts/bindings/`.
  - `display_i2c` (TWIM0: SDA P0.21, SCL P0.19, with pull-ups) and `display_spi` (SPIM2: SCK P0.19, MOSI P0.21, CS on J4-5) are both disabled until a panel overlay enables one.
- Panels, in `firmware/displays/`: SSD1306 128×32, SSD1306 128×64, SH1106 128×64 (all I2C at 0x3C), and the Waveshare 2.13" e-Paper V4 (SSD1680, 250×122, SPI). Panel settings are copied from Zephyr's own shield definitions. Select one with `-DEXTRA_DTC_OVERLAY_FILE=displays/<panel>.overlay`. `CONFIG_KB_SCREEN` then turns on from the `zephyr,display` chosen node. With no overlay, the build is exactly as before.
- Core `kb/screen.h`:
  - Layouts are up to 8 widgets on a character grid: text, layer, locks, battery, connection.
  - Each widget has a row, a column and an alignment (left, right or centre), plus 16 layer names.
  - Rows that don't fit are skipped, so one layout works on every panel.
  - `kb_screen_layout_validate` checks every field, ready for layouts sent from the host later.
  - Host tests cover rendering, alignment, truncation and rejected layouts.
- Firmware `status_screen`:
  - Draws with Zephyr's character framebuffer (10×16 font: 12×2 cells on 128×32, 12×4 on 128×64, 25×7 on the 2.13" e-paper).
  - OLED changes redraw within 20 ms. E-paper redraws at most every 3 s, and changes in between are combined.
  - Lock state (from the USB LED report) and the USB connection are live. Layer shows "Base" until phase 3, battery "BAT --" until phase 13.
  - `status_screen_set_layout()` swaps the layout at run time, for the configurator once the protocol exists.
- Build-time options (Kconfig): custom text (default "Dachshund60F"), which widgets to show, font index, invert, and the e-paper refresh interval.
- host-lib tests:
  - The J4 map and the display I2C/SPI pinctrl must match `board.json`.
  - Panel overlays must not use raw `&gpio` pins or pinctrl.
  - The LED MOSI check now looks only at the SPIM3 pinctrl.
  - Moving the DC pin in the DTS makes the test fail.
- CI builds one firmware per panel (debug and release) and checks `CONFIG_KB_SCREEN=y`. Run 39862064737 is green. The screen adds about 22 KB of flash and 9 KB of RAM (debug: 91.5 KB flash, 26.1 KB RAM with the 128×32 OLED).

### Doghouse configurator, first cut (2026-10-22, merged to main)

Started ahead of phases 5 and 7, so the design can be reviewed before boards arrive. It runs in the browser against simulated boards.

- Choices: Svelte 5 + TypeScript + Vite in `app/`; Tauri later; the app is named Doghouse. New dev dependencies in `app/`: vite, svelte, @sveltejs/vite-plugin-svelte, svelte-check and typescript (6, because svelte-check doesn't support 7 yet). host-lib gained no dependencies.
- host-lib:
  - Typed `Board`/`BoardManager` API (`types.ts`): board info with connection kind (usb, dongle, ble), battery, keymap and encoders, settings with limits, save/revert/factory reset, identify, events, and `BoardError` codes matching SPEC.
  - Keycode catalog (`keycodes.ts`): QMK-style ids in 9 categories, plus layer and Bluetooth profile keys that depend on the layer count.
  - `default-keymaps.ts`: base and Fn layers for the Dachshund60F. Fn has media keys on the F-row, BT1–3 and output selection on 1–6, navigation on the arrows, and QK_BOOT on Esc.
  - `MockBoard`/`MockBoardManager`: they validate like the firmware, keep live and saved copies, simulate battery charge and drain, and support plug/unplug.
  - 45 host-lib tests in total.
- App:
  - Pack sidebar: dachshund avatars, connection chips, bone battery meters, unsaved dots.
  - Board header: name, model, serial, connection, battery, and Identify/Revert/Save.
  - Keys tab: board drawn from `keyboard.json`; layer tabs with colours and counts; spacebar option; transparent keys show what they fall through to; roller forward/back/press; LED row with a live battery gauge; searchable palette; drag and drop; undo/redo; keyboard shortcuts.
  - Settings and About tabs; light and dark themes; toasts; a "Simulated" panel to plug in, unplug and set battery.
  - 14 Node tests for geometry, transparent-key resolution and undo history.
- Checked in the browser at 1440×900 in light and dark, and at 800×600: editing a key, the Fn layer, Save, rename, and plugging in more boards. svelte-check reports 0 errors and 0 warnings; the production build is 102 KB of JS (36 KB gzipped).

### Repo housekeeping (2026-10-23)

- CI runs once per pull request. Before, every push to a PR branch started two identical runs (push and pull_request). Pushes now trigger CI only on `main`; a branch gets CI through its PR or with "Run workflow" in the Actions tab. A newer push to a PR cancels its run that is still going.
- The USB-C daughterboard (`hardware/Dachshund60F-USB/`), designed on 2026-10-22 on a local branch, was rebased onto main and got its own PR.
- Deleted the fully merged branches `doghouse-app`, `phase2-firmware`, `status-screen`, `dongle-pcb` and `ci-one-run-per-pr`, locally and on GitHub.

### UF2 bootloader board definition (2026-10-23)

- `boards/Dachshund60F/bootloader/`: board files for the Adafruit nRF52 bootloader (`board.h`, `pinconfig.c`, `board.mk`, `board.cmake`) and `build.sh`, which clones the bootloader at a pinned master commit (c67f0bcf, 2026-05-21), adds the board and builds it. The 0.9.2 release uses an older board API (`BUTTONS_NUMBER`), so it can't build these files.
- Output: `dachshund60f_bootloader_s140_6.1.1.hex` (MBR, S140 6.1.1, bootloader and UICR) for the first SWD flash, and `bootloader_mbr.uf2` for later updates from the UF2 drive. The steps are in the folder's README.
- Entering UF2 mode: double-tap reset, or hold Esc at power-up or reset. `board_init2()` drives col 0 (P0.02) high, and the bootloader reads row 0 (P1.10) with a pull-down. Holding Esc for 3 s in the firmware still works (GPREGRET 0x57).
- Status shows on all six SK6812s (P0.06), with P0.08 turning on LED power. REGOUT0 is set to 3.3 V on first boot. USB is 1209:0003, a pid.codes test PID, with a "DACHSHUND" drive. `board.json` now lists it under `usb.bootloader`.
- Flash layout fix: on the nRF52840 the bootloader reserves 40 KB below itself (0xEA000–0xF3FFF), and it ignores UF2 app writes there. `code_partition` shrinks from 0xC6000 to 0xC4000, so it ends at 0xEA000. `storage` (0xEC000) was already in the reserved area, which DFU never erases.
- host-lib `bootloader-board.test.ts`: the LED pin, count and power pin, the Esc row and column, the USB IDs and names must all match `board.json`, and the devicetree partitions must fit the bootloader's layout. Moving the code partition back to 0xC6000 makes the test fail.
- CI job "UF2 bootloader (Dachshund60F)" builds the bootloader with Ubuntu's `gcc-arm-none-eabi` and pip's `intelhex`, and uploads the hex and UF2 as `bootloader-<sha>`.

### Phase 3: matrix, debounce, keycodes and keymap (2026-10-23)

- Core (hardware- and transport-independent, host-tested):
  - `kb/keycodes.h`: the 16-bit keycode model. Ranges for HID keys and modifiers, consumer and system keys, keys with modifiers (`LSFT(KC_1)`), layer actions (`MO`, `TG`, `TO`, `DF`, `OSL`), one-shot mods (`OSM`), layer-tap (`LT`), firmware actions (`QK_BOOT`, connection, mouse...) and mod-tap (`MT`, `LCTL_T`...). 0xA000 and up is reserved for phase 9. Names match Doghouse's QMK-style ids. `kb_keycode_kind()` rejects anything undefined, including layer keys past the layer count; the protocol will use it to refuse bad keycodes.
  - `kb/keymap.h`: a keymap in caller-owned RAM, with get/set by (layer, row, col) that bounds-checks every index and keycode, and lookup through active layers with transparent (`KC_TRNS`) and opaque no-op (`KC_NO`) keys. Up to 16 layers.
  - `kb/debounce.h`: five algorithms, picked at run time: deferred or eager, per key or whole matrix, plus eager press with deferred release (the default). Time 0–100 ms (default 5). Wrap-safe timestamps.
  - `kb/engine.h`: turns key events into HID state (modifier byte, six boot keys in press order, an NKRO bitmap, consumer and system usages). Handles layers, held modifiers counted per key, one-shot mods and layers (held, they act like normal mods and `MO`), and layer-tap/mod-tap with a tapping term (default 200 ms). Keys pressed while a tap-hold key is undecided wait in a 16-event queue, so they land on the right layer. Firmware actions go to a callback. Permissive hold and the other tap-hold options are phase 9.
  - Tests: `test_keycodes`, `test_keymap`, `test_debounce` and `test_engine` (28 engine cases with simulated events and timestamps). They also pass with AddressSanitizer and UBSan.
- Firmware:
  - `matrix.c`: COL2ROW scan with pins from the devicetree; it drives one column, waits 5 µs (`CONFIG_KB_MATRIX_SETTLE_US`), reads the rows, then waits until the rows drop (at most 10 µs) before the next column.
  - `keyboard.c`: a scan thread on a 1 kHz timer (`CONFIG_KB_MATRIX_SCAN_PERIOD_US`): scan, debounce, engine, tick. Debounce algorithm and time, and the tapping term, are Kconfig options for now; the host sets them in phase 10. The layer shows on the status screen. Key events are never logged.
  - `default_keymap.c`: the 4-layer default (Base, Fn, two transparent layers), the same as `host-lib/src/default-keymaps.ts`. The firmware runs a RAM copy, ready for live edits.
  - `QK_BOOT` (Fn+Esc) reboots into UF2 at once; `QK_RBT` reboots. Holding Esc for 3 s still reboots into UF2, now read from the debounced matrix and independent of the keymap, as a recovery path.
  - USB: key reports are queued (16 deep) and sent one per poll, so a tap within one millisecond still reaches the host as press then release. Before this, `hid_device_submit_report` ran synchronously and would have blocked the scan thread while the host wasn't polling.
- host-lib `firmware-keymap.test.ts`: every Doghouse keycode id is defined in `kb/keycodes.h`, and the firmware default keymap matches host-lib's key for key. Encoder mapping is compared once the firmware has it (phase 9).

## PCB status and known issues

- SW604, SW606 (rotated 180°) and SW615 are soldered 3-pin footprints, placed with their centre holes on the KLE key centres. Stock MX footprints have their origin at pin 1, not the centre.
- DRC before routing: there are no shorts or drill overlaps, except that the 6.25u stabilizer's right holes and the right Mod's stabilizer holes coincide (0.04 mm apart, a warning). The remaining courtyard errors on the spacebar row are expected because the two layouts deliberately overlap; add them as DRC exclusions in KiCad. The stock SK6812MINI-E reverse-mount footprint has 0.25–0.35 mm copper-to-cutout clearance against a 0.5 mm rule (fixed on 2026-10-23 with a project footprint; see "LED cutout clearance"). About 189 "footprint differs from library" warnings remain (run Update Footprints from Library in KiCad). Unrouted nets are expected.
- DRC fixes (2026-10-21, after review): removed a duplicate GND via at (319.3, 67.1) and a dangling /COL2 stub. The GND pours now use solid connections for SMD pads and thermal reliefs for through-hole pads, which cleared all starved thermals. The ST1–ST6 reference text is hidden. `Dachshund60F.kicad_dru` adds two rules: 0.2 mm edge clearance at the D2/D3 LED cutouts (stock reverse-mount pads are 0.247 mm from the cutout corners, so check with the fab), and 0 mm for SW1's edge-mount tabs.

### PCB: module decoupling rework and re-route (2026-10-22)

The old column of decoupling caps at x = 318 sat across the module's left-edge signal exit, so C5, C6 and C7 ended up with GND pads boxed in by tracks on both layers. Rework:

- Power decoupling now sits in a row above the module's top-left corner, next to VBUS, VSYS and +3V3 (pins 28–32), with every GND pad facing up: C6 (+3V3 4.7 uF), C7 (+3V3 100 nF), C4 (VSYS 4.7 uF), C5 (VSYS 100 nF) and C2 (VBUS 4.7 uF), at x 312.6–321.0, y 64.
- The crystal sits left of the module's lower-left corner, level with XL1/XL2, with C8/C9 to its left: Y1 at (317.6, 75.8), C8/C9 at x 315.2. Its tracks run straight left, which keeps U1 pad 15 (GND) open to the pour. With the crystal below the module, as first tried, its tracks walled that pad in.
- Every SMD GND pad on the board (31 pads, module included) has its own via, placed before routing and kept fixed, so GND reaches both pours whatever the router does.
- The whole board was re-routed with Freerouting 2.5.0, with GND left to the pours and fanout vias; all signal and power nets routed. Then: 0.3 mm power class, antenna keep-out, unused router vias removed, GND pours (solid to SMD pads, thermal reliefs on through-hole pads), and about 670 stitching vias, none under back-side parts.
- DRC (kicad-cli, 2026-10-22): 0 errors and 0 unconnected items; the 7 spacebar items stay excluded through the project file. No copper in the antenna keep-out. Some SWDCLK and matrix tracks run on B.Cu under the module body between its pad rows (solder-masked, no exposed pads there).
- Warnings left: the ST4/ST6 coincident stabilizer holes, silkscreen overlaps on the spacebar row, 2 SW1 silkscreen segments at the edge, and 189 stale footprint copies (fix with Update Footprints from Library).
- Schematic parity: only the Description and Datasheet text fields differ. Run Update PCB from Schematic in KiCad to sync them.
- Not done: mounting holes (need a case design) and a plate and case cutout for the roller. The J1 pin order and the USB-C daughterboard were done later (see the daughterboard section).

### PCB: layer and battery indicator row (2026-10-22)

The layer LED no longer sits under Esc. A row of five reverse-mount SK6812MINI-E LEDs now sits on the back, shining through cutouts into the free area above the Right arrow (level with the Up key, y = 130.5 mm): the layer LED at x = 310, then a 4-step battery gauge (25/50/75/100%) at x = 316.5, 322, 327.5 and 333. Each LED has a 100 nF cap just above its VDD pad. The Caps Lock LED stays under Caps Lock.

- Schematic: D4–D7 (battery) and C15–C18 were added. The chain is now U1 P0.06 → D3 (layer) → D4 → D5 → D6 → D7 → D2 (Caps), and D2's DOUT is unconnected. ERC is clean, and the chain was checked in the exported netlist.
- `board.json`: indicator count 6. `keyboard.json`: indicator `id` is the chain index. Free-standing LEDs have an `at` position (centre, key units) instead of `underKey`, and battery LEDs carry a `level` from 1 to 4. The host-lib tests now check that ids follow the chain, that each indicator has exactly one position, and that no free-standing LED sits under a key.
- Re-routed the whole board with the same Freerouting pipeline. The DSN export now adds router keep-outs that KiCad leaves out: each LED cutout plus 0.15 mm, and a 0.35 mm band inside the board edge that follows the 5 mm corners. GND stitching now stays clear of the rounded corners and of all six LEDs.
- `Dachshund60F.kicad_dru`: the 0.2 mm LED cutout rule now covers D2–D7. KiCad's `memberOfFootprint` didn't match a `D[2-7]` pattern, so the six references are listed one by one.
- DRC (kicad-cli): 0 errors and 0 unconnected items. The warnings are the same as before, plus 8 stale footprint copies for the new parts.
- The LED_D5 data line and LED_VDD both run about 310 mm across the board to the Caps LED; that's fine at 800 kHz.
- SPEC.md lists only layer and lock-state indicators under Lighting, so the battery gauge needs adding to the spec before the firmware and GUI work on it.

### PCB: display header (2026-10-22)

The case will carry a low-power display (e-ink or OLED) between the encoder and the indicator row. J4 is a 1×8 2.54 mm through-hole header on the front at y = 112.5 mm (pin 1 at x = 309.1), between the reset button and the LED load switch on the back. Each pin is labelled on the front silkscreen.

- Pinout, in Waveshare e-Paper module order: 1 +3V3, 2 GND, 3 DIN (P0.21), 4 CLK (P0.19), 5 CS (P0.20), 6 DC (P0.23), 7 RST (P0.22), 8 BUSY (P0.24). A 4-pin I2C OLED wires to +3V3, GND, CLK (SCL) and DIN (SDA). The nRF52840 maps SPI or TWI to any pin, and OLED modules carry their own pull-ups.
- Power comes straight from +3V3 (REGOUT0). That rail has about 25 mA for outside parts: enough for e-ink and a small OLED, tight for a large OLED at full white.
- `board.json` has a new `display` section, and the host-lib pin test now includes its pins.
- Re-routed with the same pipeline. DRC: 0 errors and 0 unconnected items; the warnings are unchanged. Nothing is in the antenna keep-out.
- Mechanical: a straight header stands about 8.5 mm tall and won't fit under a plate at 5 mm. The plate or case needs a cutout above J4, or use a right-angle header or solder wires directly.
- Keep the display, especially an OLED with a metal back, away from the module antenna at the right edge (x > 333, y 67–79 mm).
- Firmware support for the header followed on the same day (see Status screen above); SPEC.md now has the display in scope.

### PCB: USB-C daughterboard (2026-10-22)

The USB-C port lives on its own board, `hardware/Dachshund60F-USB/` (KiCad 10, 2 layers, 1.6 mm). It connects to mainboard J1 with a JST-SH 4-pin cable.

- Shape: a 42 × 9 mm rectangle with 1 mm corners, so it sits along the case bezel. Two 3.2 mm plated holes sit 33 mm apart, on the board's centre line. They take M3 screws, or M2 with a washer. The 33 mm spacing matches the uDB S-series screw pattern, so a stock uDB-S board can use the same case bosses (it has a Pico-EZmate connector, so it needs an EZmate-to-JST-SH cable).
- Layout: the USB-C receptacle (HRO TYPE-C-31-M-12) is centred on the front edge. Its shell sticks out 1.3 mm past the edge, the same as uDB-S. J2 (JST SM04B-SRSS-TB) sits at the right end with its opening on the rear edge. The protection parts sit at the left end. All parts are on the front. The back carries the board name and J2's pinout.
- Circuit:
  - USB 2.0 sink only: 5.1 kΩ Rd on CC1 and CC2, no PD.
  - USBLC6-2SC6 (U1) clamps ESD on D+, D− and VBUS. The mainboard has no USB protection of its own, so this is the only ESD clamp.
  - A 750 mA-hold 1206 PTC (Bourns MF-NSMF075-2) sits between the connector's VBUS and J2.
  - The receptacle shell and both screw holes are a SHIELD net, tied to GND only through a 600 Ω ferrite bead (FB1), as on uDB boards. Metal screws ground a metal case through FB1.
- Pinout: J2 is 1 VBUS, 2 D−, 3 D+, 4 GND, the same as mainboard J1 and the Unified Daughterboard spec (checked against the uDB-S design files). Use a 1:1 cable, pin 1 to pin 1, not a reversed one.
- Routing: Freerouting, with a 0.4 mm keep-out band inside the board edge. Power nets (VBUS, GND, SHIELD) are 0.4 mm. Both layers have a GND pour, with stitching vias.
- Checks (kicad-cli): ERC 0 violations. DRC: 0 errors, 0 unconnected items, 0 schematic parity issues. Two warnings remain: the stock receptacle footprint draws silkscreen past the board edge, which the fab clips.
- BOM fields: every part has an MPN, and LCSC numbers where checked (C165948, C160404, C7519, C25905). F1 and FB1 have no LCSC number yet.
- KiCad's library has no 3D model for the HRO receptacle. A generated model is used (see "3D models for case design").

### PCB: 2.4 GHz dongle (2026-10-23, merged to main in PR #1)

The receiver for phase 14 now has a board: `hardware/Dachshund60F-Dongle/` (KiCad 10, 2 layers, 1.6 mm), a 40 × 24 mm desk puck that connects to the computer with a USB-C cable. `hardware/Dachshund60F-Dongle/README.md` has the full design notes, the polling-rate limit and the latency budget.

- Form factor: a puck with a cable rather than a plug-in stick, chosen so the receiver can sit next to the keyboard, away from USB 3 noise and the PC case.
- Parts:
  - Raytac MDBT50Q-1MV2, the same module as the keyboard. Its antenna is flush with the right edge, with a full-width copper keep-out on both layers.
  - HRO USB-C, USBLC6-2SC6 and a 750 mA PTC, the same parts as the USB-C daughterboard (see its section above).
  - PAIR and RESET buttons, an RGB status LED, and an SWD header in the keyboard's pin order.
- Power: VBUS feeds both VBUS and VDDH (high-voltage mode). REGOUT0 must be set to 3.3 V by firmware, and DCCH is unconnected (LDO). There's no 32.768 kHz crystal, so firmware uses LFRC.
- Pins: `board.json` has a new `dongle` section, where the generator reads them from:
  - PAIR on P1.06
  - LED red P0.08, green P1.09, blue P0.12 (active low)
  - nRESET on P0.18
  These match Nordic's PCA10059, so dongle firmware can be developed on one. Adafruit's `pca10059` UF2 bootloader uses the same DFU button (P1.06), so it should work here as-is. This is untested.
- host-lib: a new test checks the dongle pins (valid, unique, not reserved). It fails if PAIR is moved to the reserved P0.18. There are now 46 tests.
- Routing:
  - D+/D− are hand-routed as a matched pair (about 23.5 mm each) over the B.Cu ground plane.
  - The USB-C fan-out has no vias on the data lines.
  - Supply tracks into the module are hand-routed and locked.
  - Freerouting did the rest, then GND pours and stitching vias were added.
- Checks (kicad-cli):
  - ERC 0.
  - DRC: 0 errors, 0 unconnected items, 0 parity issues. The two warnings are the stock receptacle footprint's silkscreen at the board edge, which the fab clips.
  - A script checked every module pad and each GPIO's net through to its button, LED, SWD and USB pins against `board.json`: 0 mismatches.
- Polling rate: **1000 Hz maximum.** The nRF52840 has full-speed USB only, which polls at most once per 1 ms frame. 4–8 kHz would need High-Speed USB, which means another receiver chip (for example the nRF54LM20A). That's out of v1 scope.
- Estimated latency over the dongle, to be measured in phase 14: about 1 ms typical and about 2.5 ms worst case without retransmits, compared with 7.5–15 ms for BLE. The README lists the firmware settings this depends on: GPIO-sense wake, eager press debounce, ESB at 2 Mbit/s with fast ramp-up, HFXO kept on, BLE off in dongle mode, ACK payloads for host→keyboard data, and bInterval 1 on the dongle.
- Not done: the case (light pipe, button caps, USB-C cutout; the M2 holes are placeholders), the manufacturer's STEP for the receptacle (a generated model is used for now), and a Zephyr board target for the dongle (phase 14).

### Plate: FR4 gasket-mount switch plate (2026-10-23)

`hardware/Dachshund60F-Plate/` holds a generated 1.6 mm FR4 plate. `gen_plate.py` (run with KiCad's Python) reads the mainboard and writes `Dachshund60F-Plate.kicad_pcb`; the README has the full geometry.

- Decision: the mainboard gets no mounting holes. The PCB hangs from the hot-swap switches, and the plate is gasket-mounted.
- 80 switch cutouts (14 mm, 0.5 mm corners) and 12 stabilizer cutouts (screw-in stabilizers only since 2026-10-23; see below). Both spacebar layouts are cut; where they overlap the holes merge, and only soldered switches sit in the merged slot.
- A notch at the top right for the roller encoder, a slot over the display header J4, and 4 mm windows over the indicator LEDs D3–D7.
- 8 gasket tabs (4 top, 4 bottom), 2.8 × 6.5 mm, for 8 × 5 × 5 mm silicone socks. Sock outlines are on Cmts.User for the case design.
- An exposed-copper accent ring around every switch and stabilizer cutout, ≥ 0.4 mm from every routed edge. A stabilizer and its switch share one outline, since the 1.44 mm web between them is too narrow for two rings. ENIG recommended.
- DRC (kicad-cli): 0 violations.

### PCB: LED cutout clearance (2026-10-23)

- D2–D7 now use a project footprint, `Dachshund60F:LED_SK6812MINI-E_3.2x2.8mm_P1.5mm_ReverseMount_Cutout3.4` in `hardware/Dachshund60F/lib/Dachshund60F.pretty` (registered in the project `fp-lib-table`). It is the stock KiCad footprint with the corner-relief light cutout replaced by a plain 3.4 × 3.4 mm cutout with 0.5 mm corners.
- Pads are now 0.35 mm from the routed edge (was 0.247 mm), and the nearest track about 0.36 mm. The LED body (3.2 × 2.8 mm) clears the rounded corners by about 0.05 mm. Nothing was re-routed.
- `Dachshund60F.kicad_dru`: the LED cutout rule is raised from 0.2 mm to 0.3 mm. The schematic's footprint fields for D2–D7 point at the new footprint.
- Checks (kicad-cli): ERC 0. DRC: 0 errors and 0 unconnected items after refilling the zones. The warnings are unchanged.

### 3D models for case design (2026-10-23)

`python hardware/export_step.py` writes STEP files of every board to `hardware/step/` (committed since 2026-10-23, so they reach every device): the mainboard, the plate, the mainboard and plate stacked, the daughterboard and the dongle. The mainboard, plate and assembly share an origin (the centre of the mainboard outline). The plate's bottom face is 5.0 mm above the PCB's bottom face, so the plate's top is 5.0 mm above the PCB's top (the MX spec).

- Mainboard: the footprints had lost their 3D model links (only J4 had one). Every footprint now carries its library's models again.
- Keyswitch models (perigoso v2.4, CC-BY-SA-4.0) are vendored into `hardware/Dachshund60F/lib/keyswitch.3dshapes/`: the Kailh hot-swap socket, the Cherry MX switch (PCB and plate versions) and the 2u and 6.25u stabilizers. The vendored hot-swap footprints now show the switch body as well as the socket. The three soldered spacebar-row switches use the plate-mount switch model without a keycap.
- The two spacebar layouts are both modelled, so their switches overlap in the 3D view.
- USB-C receptacle (daughterboard and dongle J1): KiCad has no model for the HRO TYPE-C-31-M-12, so `hardware/3dmodels/` has one (see below).
- Dongle D1 uses KiCad's Würth PLCC4 3.2 × 2.8 mm RGB LED model (same package as the ASMB-MTB0).
- Encoder SW3 and battery connector J2 use dimensioned stand-ins in `hardware/3dmodels/` (KiCad's 3D library has neither model). The real STEPs need a Bourns/SamacSys account and JST's emailed licence download; swap them in when you have them.

### Stabilizer cutouts, USB-C and JST models (2026-10-23)

- Plate stabilizer cutouts were the Cherry plate-mount size (6.75 × 12.3 mm), slightly smaller than the stabilizer model's housing at plate height (0.02 mm at the top, 0.025 mm on each side). They are now 7.0 × 14.7 mm, 6.5 mm above to 8.2 mm below the key centre, for screw-in stabilizers of any brand (the target is Swagkeys Knight). Plate-mount stabilizers no longer clip in. The size is `STAB_CUTOUT_*` in `gen_plate.py`. DRC: 0 violations.
- The old USB-C stand-in sat 7.3 mm behind its footprint on both the daughterboard and the dongle (its rotation flipped it), so in the STEP files the receptacle was inside the board instead of at its edge. That was only the 3D model; the footprints and copper didn't move.
- `hardware/3dmodels/gen_models.py` (run with FreeCAD 1.1's Python) now generates coloured models of the HRO receptacle (shell, opening, tongue and contacts, shell legs, SMD tails, locating posts) and the JST PH S2B-PH-SM4-TB (housing with cavity and latch window, contacts and tails, metal fittings), each in its footprint's frame. The footprints use them with no offset or rotation. On both boards the receptacle's front overhangs the board edge by 1.3 mm, as designed.
- Checked: the STEP exports place both receptacles and J2 on their footprints, and `kicad-cli pcb render` shows them as expected. Not checked against real parts.

## Tested

- host-lib `npm test`: passes locally (Node 24.12, Windows), including board and devicetree validation.
- Core C tests (core, hold, screen, version): pass in CI. Not run locally; there's no C compiler or CMake here.
- Firmware builds: debug and release for `dachshund60f/nrf52840` with no screen and with each of the four panels, and the dongle, pass in CI (NCS v3.4.1, Zephyr 4.4.2), run 39862064737. Not built locally; NCS isn't installed here.
- Bring-up plan for all three boards (inspection, power, flashing, functional tests and measurements with methods and equipment): `docs/bringup-test-plan.md` (2026-10-23).
- UF2 bootloader: builds cleanly (`-Werror`) on Linux with arm-none-eabi-gcc 13.2.1. The merged hex puts the SoftDevice at 0x1000–0x25DE8, the bootloader at 0xF4000–0xFC3AC, and the UICR words at 0x10001014/18. Not yet run on a chip.
- Core C tests for phase 3 (debounce, engine, keycodes, keymap): pass locally on Linux with GCC, also under ASan and UBSan.
- Real hardware: nothing yet. Phase 3 is to be checked on a board:
  - every key types what the README matrix table says, with no ghosting when three keys in a rectangle are held
  - Fn layer: media keys, Insert, Print Screen, Page Up/Down, Home/End; the status screen shows the layer
  - Fn+Esc mounts the UF2 drive at once; holding Esc for 3 s still does
  - scan rate is 1 kHz and a scan takes well under 1 ms (toggle a spare GPIO and use a logic analyser)
  - no chatter with the default debounce (eager press, 5 ms deferred release); try other algorithms through Kconfig if needed
- Phase 2 is still to be checked on a board:
  - flash `dachshund60f_bootloader_s140_6.1.1.hex` over SWD with a chip erase; check the DACHSHUND drive appears and `INFO_UF2.TXT` says `nRF52840-Dachshund60F-v1`; then copy `zephyr.uf2`
  - check that it enumerates as 1209:0001 with a unique serial number (USB Device Tree Viewer or `lsusb -v`)
  - check that it works as a boot keyboard in the BIOS
  - check that the LEDs walk in chain order with correct colours
  - check that holding Esc for 3 s mounts the UF2 drive, and that holding Esc while plugging in does too
  - check the SK6812s show the bootloader colours (its WS2812 timing is slightly long for the SK6812 spec: a 0.81 µs high bit against 0.75 µs at most)
  - check that REGOUT0 reads 3.3 V
  - for the chosen panel: check that the screen shows layer, battery and text, that Caps/Num/Scroll follow the host, that "USB" appears once enumerated, and that the orientation is right in the case
- USB-C daughterboard: not built. On first boards, check:
  - VBUS reaches mainboard J1 pin 1 with a 1:1 cable
  - the board enumerates through it with both plug orientations
  - a USB-C to USB-C cable from a charger supplies 5 V (the Rd pull-downs work)
  - the shell and both screw holes read close to 0 Ω to GND (through FB1), and D+/D− read open to the shell
- 2.4 GHz dongle board: not built. On first boards, check:
  - VBUS reaches 5 V; VDD reads 1.8 V on a blank module and 3.3 V once firmware sets REGOUT0
  - SWD flashing works, and whether Adafruit's `pca10059` UF2 bootloader runs (hold PAIR while plugging in)
  - it enumerates as 1209:0002 in both plug orientations and from a USB-C to USB-C cable
  - the LED shows red, green and blue; PAIR and RESET work
  - in phase 14: range, and latency with a logic analyser, with the puck next to the keyboard and then plugged straight into a USB 3 port

## Open decisions

- Registered pid.codes PID before distributing boards; this needs a licence and a public repo with PCB files
- PCB design: the pin map is proposed and must be confirmed (or edited) once the schematic is routed
- Case (sets the gasket pockets from the plate's sock outlines, the roller support and the daughterboard position). The mainboard and plate have no mounting holes. The daughterboard is designed for M3; if the case uses M2, swap H1/H2 to `MountingHole_2.2mm_M2_Pad_TopBottom` for a snug fit.
- When to add the Tauri shell (needs Rust and the MSVC build tools); WebHID stays possible for a hosted version
- Mainboard power path: VSYS is fed only from the battery through the slide switch, and VBUS only feeds the charger. So the keyboard is off on USB with the switch off, and browns out on USB with no battery. Add a VBUS-to-VSYS power path before ordering if USB-only use matters.
- Display panel: OLED (which size) or e-paper; also its orientation in the case, since the overlays assume the panel's default rotation
- Dongle case: it sets the M2 hole positions, the light pipe over D1 and the button caps. Use plastic, with nothing metal over the antenna strip.
- Dongle bootloader: reuse Adafruit's `pca10059` build or make a dongle-specific one. The DFU button matches (P1.06), but that build's LEDs are P0.06 and P0.12, so only the blue LED would light on this dongle. A dongle board can reuse the files in `boards/Dachshund60F/bootloader/` as a template. Try the existing build on the first board.
- Licence

## Known bugs

- Encoder part number checked against Bourns' PEC12R datasheet (2026-10-23): the BOM's PEC12R-4220F-S0024 is right. The first digit of the ordering code is the mounting: 2 = vertical (radial pins, shaft up from the board), 3 = horizontal with a threaded bushing, 4 = horizontal without a bushing. So -4220F-S0024 is horizontal, no bushing, 24 detents, 20 mm shaft, push switch, 24 pulses, and Mouser lists it as right angle. Don't order PEC12R-2217F-S0024: it is the vertical part. KiCad's footprint name `Horizontal_PEC12R-2x17F` uses the wrong series digit; its geometry is the horizontal part's. Still open: SW3's 3D model is a stand-in (see `hardware/3dmodels/README.md`), and the footprint's hole pattern hasn't been checked dimension by dimension against the 4xxx drawing.
- Release builds print Kconfig warnings, because `prj.conf` sets logging options that `release.conf` turns off. PRINTK also stays on in release, since something selects it; nothing prints, because no console or RTT backend exists. This is harmless, but the warnings are noise.
- E-paper builds warn that Zephyr's MIPI-DBI driver is "experimental". It is what Zephyr's own e-paper shields use; this is a notice, not a fault.
- Doghouse's palette has no layer-tap, mod-tap, one-shot mod, default-layer or modded keys yet, though the firmware supports them. Add them with the phase 9 editors.
- The matrix is scanned at 1 kHz all the time, which costs battery. Phase 13 adds idle scanning with GPIO sense wake-up.
- The 6KRO boot report drops a seventh held key (it still reaches the NKRO bitmap, which phase 4 sends).
- Doghouse doesn't yet show key labels in the user's OS keyboard layout (AZERTY and so on), edit the status screen layout, or update firmware. The SPEC lists these for later phases.
- An OLED left on all the time can burn in. It gets an idle timeout with power management in phase 13.
