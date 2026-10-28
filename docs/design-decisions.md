# Design decisions and what I've learnt

This project is built with a lot of help from AI, so I want a clear record of which calls were mine. Claude Code drafts, routes, writes and tests. I set the direction, pick between options and review the result. This page lists the decisions I made myself, and the topics the project has taught me so far.

Last updated 2026-10-24.

## Decisions I made

### Scope (October 21, before any design work)

| Decision | What I chose | What I passed on, and why |
| --- | --- | --- |
| Microcontroller | nRF52840 on the keyboard, and a second one in the dongle | RP2040 and STM32 have no Bluetooth. Using one chip family for both ends keeps one toolchain. |
| Connectivity | Tri-mode: USB, Bluetooth LE and a 2.4 GHz dongle, built wired-first | Wireless doubles the firmware. Getting USB solid first keeps wireless bugs separate from keymap bugs. |
| Form factor | Unibody 60% with an F-row, no split | A split needs a half-to-half sync protocol, which isn't what I want to learn here. |
| Switch sensing | Diode matrix | Hall-effect sensing is its own project. |
| Lighting | Indicators only (layer, lock state, battery) | Per-key RGB costs power and battery life, and it isn't what I care about. |
| Extras | A rotary encoder and an optional status display | No trackball, trackpad or haptics. |
| Firmware stack | C on Zephyr / nRF Connect SDK | Rust + Embassy was the other option. Zephyr has the mature Bluetooth stack, and it's what ZMK uses. |
| Driver | No kernel driver: a vendor HID interface plus a user-space library | A real driver needs signing on Windows and entitlements on macOS, and it would break "works anywhere". |
| Bootloader | Adafruit's UF2 bootloader | Drag-and-drop updates, and a bad flash can always be recovered. |
| Left out of v1 | Unicode input, per-key RGB effects, pointing devices, haptics, split halves, Hall-effect, signed A/B firmware, physical unlock for config writes | Listed in SPEC.md so they're easy to bring back. |

### Hardware

| Decision | What I chose | Why |
| --- | --- | --- |
| Encoder | A Bourns PEC12R-2230F-S0024 (shaft parallel to the board, 24 detents, 30 mm shaft) used as a roller, turning front to back like a mouse wheel | I wanted a scroll wheel, not a knob. It replaced the EC11 in the first layout. |
| Board shape | 5 mm corner radius (it was 1 mm) | Softer corners for the case. About 6 mm is the most the encoder corner allows. |
| Indicators | A row of five LEDs above the arrow keys (layer plus a 4-step battery gauge), with Caps Lock under its key | The layer LED moved out from under Esc, and the battery level is visible without opening the app. |
| Display | A header (J4) for an OLED or e-paper panel, with the panel choice left open | I want to see the board and case before picking. The firmware supports four panels. |
| USB-C | On its own small daughterboard, with a Unified Daughterboard-compatible pinout and screw spacing | The port can sit at the case edge, and a stock uDB board fits the same case. |
| Dongle | A desk puck on a USB-C cable, not a stick that plugs into the PC | It can sit next to the keyboard, away from USB 3 noise and the metal of the PC case. |
| Power path | Microchip's load-sharing circuit for the MCP73831 (a P-MOSFET and a Schottky) | Before this, the board only ran from the battery. Now it runs from USB with the switch off or no battery fitted. |
| LED data line | A SN74LV1T34 level-shifting buffer | On USB the LEDs get about 4.7 V, so a 3.3 V data line has no margin. I chose a proper buffer over a diode drop or testing as-is. |
| Test pads | Nine labelled pads on the back for every rail | I want to measure the boards properly, not just see if they work. |
| Mounting | No mounting holes on the mainboard: the switches hold it, and an FR4 plate is gasket-mounted | A softer, more even typing feel, and a simpler PCB. I'm handling the case and mounting myself. |
| Polling rate | Stay on the nRF52840 at 1 kHz, and cut input delay in the firmware instead | Its USB is full-speed, so 1 kHz is the ceiling. An nRF54H20 (high-speed USB) could go higher, but it meant a new module, a new bootloader and redoing most of the board. The firmware now scans the moment a key goes down, and the biggest delays I control are tap-hold and debounce anyway. |
| Stabilisers | Screw-in only, with plate cutouts sized for brands like Swagkeys Knight | The stabilisers I want to use. Plate-mount ones no longer clip in. |

### Project and process

| Decision | What I chose | Why |
| --- | --- | --- |
| Licence | MIT for code, CERN-OHL-P-2.0 for hardware | Permissive, so anyone can build on it. |
| Openness | The repo is public (since October 24) | It's a learning project worth sharing. |
| USB IDs | Stay on the pid.codes test PIDs and don't register my own (decided October 24) | The board is only for my desk, which is what test PIDs are for. If a board ever leaves my hands, I'll register a PID first. The README warns anyone who rebuilds it. |
| CAD files | Nothing CAD-related is git-ignored, and STEP exports are committed | I work across several devices and want every file everywhere. |
| Code generation | Python + PyYAML for the protocol generator | It's already part of the nRF Connect SDK, so it adds no new dependency. |
| Workflow | One PR per phase, ten required CI checks, and rebase merges | Every phase is reviewable on its own, and `main` always builds. |
| Testing | A full bring-up and measurement plan before the boards arrive | I want numbers: rail voltages, current in every state, scan rate, latency and range. |
| App | Doghouse, a dog-themed configurator in warm earth tones | Boards are named after dogs, so the app is where they live. |

## What I've learnt

These are the topics the project has pushed me into so far. Some are understood well, some are just started, and the boards will test all of them.

### USB

- **How a USB keyboard works:** descriptors, interfaces, endpoints and report IDs, and why one cable can carry a keyboard, media keys and a config channel as separate HID interfaces.
- **Boot protocol vs NKRO:** a BIOS or KVM only understands the 8-byte boot report, so the board has to fall back cleanly without sticking keys.
- **Full-speed limits:** the nRF52840 only does 12 Mbit/s full-speed USB, so 1000 Hz is the ceiling for polling. 4–8 kHz would need a High-Speed chip.
- **Suspend, resume and remote wakeup**, and why a report queue matters when the host stops polling.
- **USB-C as a sink:** 5.1 kΩ CC pull-downs, ESD clamping on D+/D−, a PTC fuse, and a shield tied to ground through a ferrite.
- **VID/PID rules:** what pid.codes is, and why you can't ship someone else's IDs.

### Radio and wireless

- **Antenna keep-outs** on a module design, and keeping metal (and an OLED's metal back) away from the antenna.
- **Bluetooth LE HID** (HID over GATT), profiles and bonding, and its latency (7.5–15 ms) compared with a proprietary link.
- **Nordic's ESB protocol** for the dongle: 2 Mbit/s, ACK payloads, and a latency budget of about 1 ms.

### Power

- **Load sharing:** why a charger feeding the load directly is a problem, and how a P-FET and Schottky fix it.
- **Logic levels:** an SK6812's input-high threshold scales with its supply, which is why the LED line needed a buffer.
- **LiPo charging and safety**, battery sensing through the nRF52840's VDDH divider, and the high-voltage regulator modes (REG0 and REGOUT0).

### PCB design

- **KiCad 10 end to end:** schematic, footprints, custom design rules, DRC and ERC, and STEP export for case design.
- **Autorouting with Freerouting**, and its gaps: it drops antenna keep-outs, and it necks tracks down at fine-pitch pads.
- **Matched differential pairs** for USB D+/D−, ground pours and stitching vias.
- **Mechanical fit:** MX plate height, gasket mounts, stabiliser cutouts, hot-swap sockets that collide on split spacebars, and reverse-mount LEDs shining through cutouts.

### Firmware

- **Zephyr and devicetree:** board definitions, pin control, Kconfig, and freestanding vs workspace builds.
- **Matrix scanning and debounce:** COL2ROW scanning, settle times, and five debounce algorithms and what each trades off.
- **The keymap engine:** layers, tap-hold decisions, combos, tap dance, Caps Word and macros, all tested on a PC with simulated key timings.
- **Flash storage:** wear levelling, CRCs, power-loss safety, and schema migrations.
- **Bootloaders:** UF2, flash layouts, and the region the Adafruit bootloader reserves below itself.

### Protocols and software

- **Designing a protocol:** framing, sequence numbers, capability bits, versioning, and generating both sides from one YAML file.
- **Defensive parsing:** checking every index and length, and fuzzing with libFuzzer and sanitisers. A config interface that can write macros can make a keyboard type commands, so this is a security feature.
- **WebHID and node-hid:** how browsers and desktops reach a HID device, and the OS gotchas (Windows locking keyboard interfaces, macOS Input Monitoring, Linux udev rules).
- **Testing across layers:** running the real firmware protocol code on a PC so the app's tests check the firmware too.

### Working with AI on hardware

- AI is fast at the grind: routing passes, footprint checks, boilerplate, tests and docs.
- The decisions still need me. The encoder part number, the power path and the LED logic levels each came up as a question with several reasonable answers, and the choice had to be mine.
- Checks have to be mechanical to be trusted: DRC and ERC, pin-map tests against the PCB, CI on every change, and a bring-up plan with measurements.
