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
- Outline: 318.5 × 121.1 mm (x 19–337.5, y 39–160.1), about 12.7 mm wider than 16u.
- Routing: Freerouting 2.5.0 (JAR on a portable Temurin 25 JRE) autorouted every signal and power net with 0 violations: 0.25 mm signal tracks, and a 0.3 mm power class for VBUS, VBAT, VSYS, LED_VDD, GND and +3V3. KiCad's Specctra export drops the module's antenna rule area, so a matching keep-out was added to the DSN before routing. The SES was imported with KiCad's own `pcbnew.ImportSpecctraSES`, and 118 unused fanout vias were removed.
- Ground: GND pours on F.Cu and B.Cu (0.3 mm clearance, thermal reliefs), about 690 GND stitching vias, and short via ties for boxed-in decoupling-cap pads.
- Design rules: the minimum track width is now 0.15 mm, because Freerouting necks tracks down to 0.187 mm at fine-pitch pads.

## PCB status and known issues

- SW604, SW606 (rotated 180°) and SW615 are soldered 3-pin footprints, placed with their centre holes on the KLE key centres. Stock MX footprints have their origin at pin 1, not the centre.
- DRC before routing: there are no shorts or drill overlaps, except that the 6.25u stabilizer's right holes and the right Mod's stabilizer holes coincide (0.04 mm apart, a warning). The remaining courtyard errors on the spacebar row are expected because the two layouts deliberately overlap; add them as DRC exclusions in KiCad. The stock SK6812MINI-E reverse-mount footprint has 0.25–0.35 mm copper-to-cutout clearance against a 0.5 mm rule. About 189 "footprint differs from library" warnings remain (run Update Footprints from Library in KiCad). Unrouted nets are expected.
- DRC fixes (2026-10-21, after review): removed a duplicate GND via at (319.3, 67.1) and a dangling /COL2 stub. The GND pours now use solid connections for SMD pads and thermal reliefs for through-hole pads, which cleared all starved thermals. The ST1–ST6 reference text is hidden. `Dachshund60F.kicad_dru` adds two rules: 0.2 mm edge clearance at the D2/D3 LED cutouts (stock reverse-mount pads are 0.247 mm from the cutout corners, so check with the fab), and 0 mm for SW1's edge-mount tabs.
- DRC now (kicad-cli): no shorts, clearance, crossing, width or thermal errors. The only errors left are the 7 intentional spacebar-overlap courtyard items (exclude them in KiCad) and 3 unconnected GND items.
- The 3 unconnected items are the GND pads (pad 2) of C5, C6 and C7 in the decoupling column beside the module. VSYS, +3V3, NRESET, CHG_STAT and VBUS tracks on B.Cu, and COL12/13/14, XL1, VSYS and LED_DIN on F.Cu, box them in. No single via within 6 mm can reach the main pour, and a GND-only Freerouting pass ripped up locked tracks, so that attempt was thrown away. Fix by hand: route from each pad to U1 pad 15 (321.85, 77.8) or C9 pad 2 (313.0, 77.02), which are in the main pour, using KiCad's interactive router in shove mode. A tie via at (318.676, 74.775) already joins C6 pad 2 and can be reused.
- Warnings left: the ST4/ST6 coincident stabilizer holes, silk overlaps on the spacebar row and stabilizers, 2 SW1 silk segments at the edge, and 189 stale footprint copies (fix with Update Footprints from Library).
- Schematic parity: only the Description and Datasheet text fields differ. Run Update PCB from Schematic in KiCad to sync them.
- Not done: the hand fix for the 3 GND items, mounting holes (need a case design), checking the J1 pin order against the UDB spec, a plate and case cutout for the roller, and the USB-C daughterboard.

## Tested

- host-lib `npm test`: 13/13 pass locally (Node 24.12, Windows), including the board validation.
- Core C tests: not run locally because there is no C compiler or CMake on this machine. CI only.
- Firmware builds: not run locally because NCS is not installed. CI has not run either, since the repo is not under git or pushed yet.
- Real hardware: nothing yet.

## Open decisions

- Registered pid.codes PID before distributing boards; this needs a licence and a public repo with PCB files
- PCB design: the pin map is proposed and must be confirmed (or edited) once the schematic is routed
- Case and mounting (sets the outline, mounting holes and daughterboard position)
- GUI shell: Tauri or a WebHID web app
- Licence

## Known bugs

None yet.
