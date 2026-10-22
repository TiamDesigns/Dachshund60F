# PROGRESS

## Current phase

Phase 2 (blinky and USB enumeration): the firmware half is done and builds in CI on branch `phase2-firmware`. Hardware testing waits for fabricated boards and a Dachshund60F build of the UF2 bootloader.

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
- The board has no USB-C connector. J1 is a JST-SH 4-pin for the daughterboard, carrying VBUS, D−, D+ and GND in the Unified Daughterboard order; that order still needs checking against the UDB spec.
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

### Doghouse configurator, first cut (2026-10-22, branch `doghouse-app`)

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

## PCB status and known issues

- SW604, SW606 (rotated 180°) and SW615 are soldered 3-pin footprints, placed with their centre holes on the KLE key centres. Stock MX footprints have their origin at pin 1, not the centre.
- DRC before routing: there are no shorts or drill overlaps, except that the 6.25u stabilizer's right holes and the right Mod's stabilizer holes coincide (0.04 mm apart, a warning). The remaining courtyard errors on the spacebar row are expected because the two layouts deliberately overlap; add them as DRC exclusions in KiCad. The stock SK6812MINI-E reverse-mount footprint has 0.25–0.35 mm copper-to-cutout clearance against a 0.5 mm rule. About 189 "footprint differs from library" warnings remain (run Update Footprints from Library in KiCad). Unrouted nets are expected.
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
- Not done: mounting holes (need a case design), checking the J1 pin order against the UDB spec, a plate and case cutout for the roller, and the USB-C daughterboard.

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

## Tested

- host-lib `npm test`: passes locally (Node 24.12, Windows), including board and devicetree validation.
- Core C tests (core, hold, screen, version): pass in CI. Not run locally; there's no C compiler or CMake here.
- Firmware builds: debug and release for `dachshund60f/nrf52840` with no screen and with each of the four panels, and the dongle, pass in CI (NCS v3.4.1, Zephyr 4.4.2), run 39862064737. Not built locally; NCS isn't installed here.
- Real hardware: nothing yet. Phase 2 is still to be checked on a board:
  - flash the UF2 bootloader over SWD, then `zephyr.uf2`
  - check that it enumerates as 1209:0001 with a unique serial number (USB Device Tree Viewer or `lsusb -v`)
  - check that it works as a boot keyboard in the BIOS
  - check that the LEDs walk in chain order with correct colours
  - check that holding Esc for 3 s mounts the UF2 drive
  - check that REGOUT0 reads 3.3 V
  - for the chosen panel: check that the screen shows layer, battery and text, that Caps/Num/Scroll follow the host, that "USB" appears once enumerated, and that the orientation is right in the case

## Open decisions

- Registered pid.codes PID before distributing boards; this needs a licence and a public repo with PCB files
- PCB design: the pin map is proposed and must be confirmed (or edited) once the schematic is routed
- Case and mounting (sets the outline, mounting holes and daughterboard position)
- When to add the Tauri shell (needs Rust and the MSVC build tools); WebHID stays possible for a hosted version
- Display panel: OLED (which size) or e-paper; also its orientation in the case, since the overlays assume the panel's default rotation
- Licence

## Known bugs

- Release builds print Kconfig warnings, because `prj.conf` sets logging options that `release.conf` turns off. PRINTK also stays on in release, since something selects it; nothing prints, because no console or RTT backend exists. This is harmless, but the warnings are noise.
- E-paper builds warn that Zephyr's MIPI-DBI driver is "experimental". It is what Zephyr's own e-paper shields use; this is a notice, not a fault.
- Doghouse keycode ids (`host-lib/src/keycodes.ts`) are QMK-style names. Phase 3 (firmware keycode model) and phase 5 (protocol) must adopt them or map them, so the GUI, mock and firmware agree.
- Doghouse doesn't yet show key labels in the user's OS keyboard layout (AZERTY and so on), edit the status screen layout, or update firmware. The SPEC lists these for later phases.
- An OLED left on all the time can burn in. It gets an idle timeout with power management in phase 13.
- No Dachshund60F build of the Adafruit nRF52 bootloader exists yet. It's needed for the first SWD flash. Where its board files live (in this repo or a fork) is still open.
