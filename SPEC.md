# Custom Keyboard — Full-Stack Build Spec

Oct 21, 2026 · @Tiam

## Read first: architecture and the driver question

You almost certainly do not want a kernel driver. A keyboard that enumerates as a standard USB HID device works on Windows, macOS, Linux, iOS, Android and in BIOS with zero drivers, and a custom kernel driver would break that.

Kernel drivers also carry real cost: Windows requires Microsoft-signed drivers (EV certificate plus attestation signing), macOS requires DriverKit entitlements granted by Apple, and every OS update can break them.

What QMK/VIA, ZMK Studio and every commercial configurator do instead, and what this spec assumes:

- **Firmware** on the microcontroller exposes a composite USB device: normal keyboard interfaces the OS handles itself, plus one extra vendor-defined HID interface for configuration.
- **"Your driver"** becomes a user-space host library that speaks your own protocol over that vendor interface. No install, no signing, works through hidapi on desktop and WebHID in Chrome/Edge.
- **GUI app** sits on top of that library.

```
 Firmware on the MCU                  Host computer
 +-----------------------+
 | Keyboard HID          | --> OS built-in HID driver --> every app, BIOS, KVM (normal typing)
 | boot 6KRO, NKRO, media|     (no install, no signing)
 +-----------------------+
 | Vendor config HID     | <-> Host library ("your driver") <-> Configurator GUI
 | your protocol, 0xFF00+|     hidapi or WebHID                 keymap, macros, indicators
 +-----------------------+
 One USB cable (or BLE / dongle link) carries both.
```

Typing goes through the OS's own driver; configuration goes through your vendor interface and host library.

Keep a true kernel driver on the list only if you later need something HID cannot do. Nothing in a keymap configurator does.

Second honest point: a from-scratch firmware with every feature below is many months of work. A reasonable middle path is to write your own USB stack integration, protocol and GUI, while borrowing proven algorithms (debounce, tap-hold) from QMK, ZMK or RMK. Mind the licences: QMK is GPL-2.0+, ZMK is MIT, RMK is MIT/Apache-2.0.

## Decisions to lock before prompting Claude Code

Claude Code writes much better code when these are fixed up front; put the answers at the top of a `SPEC.md` in the repo.

**Locked from your selections:**

| Decision | Choice |
| --- | --- |
| Microcontroller | nRF52840 on the keyboard, a second nRF52840 for the 2.4 GHz dongle |
| Connectivity | Tri-mode: USB, Bluetooth LE, 2.4 GHz dongle; built wired-first |
| Form factor | Unibody, no split |
| Switch sensing | Diode matrix (digital), no Hall-effect |
| Lighting | Layer and lock-state indicators only, no per-key RGB |
| Peripherals | Rotary encoders; a status display on header J4 (OLED or e-paper, panel not yet chosen); no pointing device or haptics |
| Firmware language and stack | Open: C on Zephyr / nRF Connect SDK (what ZMK uses, mature BLE), or Rust + Embassy with a Rust BLE stack (what RMK uses) |

The full table of options below is kept for reference.

| Decision | Common options | What it affects |
| --- | --- | --- |
| Microcontroller | RP2040 / RP2350 (cheap, UF2 drag-and-drop flashing); STM32F4/G4 (mature USB, DFU); nRF52840 (needed for Bluetooth) | Toolchain, bootloader, flash size, wireless |
| Firmware language and stack | C + TinyUSB (+ Pico SDK or bare HAL); Rust + Embassy + `usbd-hid` | Every firmware prompt; Rust gives safer protocol parsing |
| Connectivity | Wired only; Bluetooth LE; 2.4 GHz dongle; tri-mode | Doubles firmware scope once wireless is in |
| Form factor | Unibody; split (wired TRRS/UART or wireless) | Split adds a half-to-half sync protocol |
| Switch sensing | Diode matrix (COL2ROW or ROW2COL); direct pins; Hall-effect analog | Scan code, debounce, actuation features |
| Host targets | Windows, macOS, Linux; browser-only | Packaging, permissions, signing |
| GUI stack | Tauri (Rust core + web UI); Electron + node-hid; web app over WebHID; native (Qt, SwiftUI) | Distribution and HID access path |
| Protocol transport | Vendor HID, 32 or 64-byte reports; optional CDC serial for logs | Host library design |
| USB VID/PID | pid.codes (free PID for open-source hardware); buy a VID from USB-IF; never ship a borrowed VID | Device discovery, legality of shipping |
| Licence | MIT/Apache vs GPL | Whether you can reuse QMK code |

Still open: firmware language (C/Zephyr vs Rust/Embassy), key count and layer count, GUI stack (desktop app vs WebHID web app; your app-quality picks of signing and auto-update point to a desktop app such as Tauri), and VID/PID source (pid.codes if open-source, your own VID if you plan to sell boards).

## Hardware prerequisites the firmware depends on

Firmware prompts need these facts as inputs; collect them into a `board.toml` or `board.h` before writing code.

- **Pin map:** which GPIO drives each row, column, LED data line, encoder A/B, I2C bus and button.
- **Matrix:** rows × columns, diode direction, and the physical position of every switch (needed later for the GUI layout file).
- **USB:** USB-C receptacle with 5.1 kΩ pull-downs on CC1/CC2, ESD protection on D+/D− (e.g. USBLC6-2), correct crystal if the MCU needs one for USB.
- **Recovery:** a BOOT/RESET button or pad so a bad firmware never bricks the board; SWD/JTAG header for debugging with a probe (Raspberry Pi Debug Probe, ST-Link, J-Link).
- **Flash budget:** enough flash for firmware plus a reserved region for keymap/settings storage with wear levelling.
- **Wireless:** nRF52840 module with a certified antenna, battery, charger IC, voltage divider or fuel gauge for battery reporting, antenna keep-out on the PCB, a power switch. A second nRF52840 board (e.g. a USB dongle form factor) for the 2.4 GHz receiver.
- **Indicators:** one or a few LEDs (single-colour on GPIO, or a short WS2812 chain) for layer and lock state; keep current low for battery life.
- **Encoders:** rotary encoder pins and positions, since encoder mapping is in scope.

## Firmware feature list

Checked items are in scope for v1; everything you dropped is listed under Not in v1 at the end. Tier 1 is a usable keyboard; tier 2 is what makes a configurator worth building.

### Tier 1: core input (must have)

- [x] Matrix scanning (or direct-pin reading) at a fixed rate, 1 kHz or better
- [x] Debounce with a selectable algorithm (eager or deferred, per-key or global) and configurable time in ms
- [x] Keycode model: a 16-bit keycode space covering HID usages, layer actions, modifiers and your custom actions
- [x] Keymap lookup by (layer, row, col) with transparent and no-op keys
- [x] Layers: default layer, momentary, toggle, to, one-shot, layer-tap; at least 4, ideally 8–16
- [x] Modifiers including one-shot mods and mod-tap
- [x] HID reports: 6KRO boot-protocol report plus an NKRO bitmap report, with automatic fallback when the host requests boot protocol (BIOS, KVMs)
- [x] Consumer control (media, volume, brightness) and system control (sleep, power) reports
- [x] Lock-key LEDs driven from the host's output report (Caps, Num, Scroll)
- [x] USB suspend, resume and remote wakeup
- [x] Jump to bootloader from a key and from a host command

### Tier 2: advanced key behaviour

- [x] Tap-hold engine with tapping term, quick-tap term, permissive hold, hold-on-other-key-press, retro tapping (per key, configurable)
- [x] Combos (several keys pressed together emit another action), with timeout
- [x] Tap dance (different actions on 1, 2, 3 taps or tap-then-hold)
- [x] Macros: stored sequences with press, release, tap, delay and text; plus dynamic record/playback
- [x] Key overrides (e.g. Shift+Backspace = Delete)
- [x] Caps Word, Auto Shift, Leader key, Repeat key
- [x] Mouse keys (move, scroll, buttons) with acceleration settings
- [x] Rotary encoder mapping per layer (clockwise / counter-clockwise actions)

### Tier 3: indicators

- [x] Layer and lock-state indicator colours
- [x] Status display on the J4 header, OLED or e-paper chosen at build time: active layer, lock state, battery, connection and custom text, in a layout the user can change

### Persistence

- [x] Versioned settings schema in flash (magic number, schema version, CRC)
- [x] Wear-levelled storage (flash sector rotation or a key-value store such as `sequential-storage` in Rust)
- [x] Compiled-in default keymap, factory reset, migration when the schema version changes
- [x] Live edits held in RAM, committed to flash on explicit save or after an idle delay

### Updates and safety

- [x] Bootloader: a UF2 bootloader on the nRF52840 (e.g. the Adafruit nRF52 bootloader), plus a host-triggered jump into it for in-app updates
- [x] Firmware version and build hash reported to the host
- [x] Watchdog, and a safe mode (hold a key at power-on to skip stored config)

### Wireless (after the wired core works)

- [x] BLE HID over GATT, multiple host profiles with switching keys, bonding management
- [x] Battery level reporting (BLE Battery Service and to your GUI)
- [x] Power management: idle sleep, deep sleep, wake on keypress
- [x] 2.4 GHz dongle on a second nRF52840, with its own firmware and pairing to the keyboard

### Diagnostics

- [x] Matrix test mode streaming raw key states to the host
- [x] Debug log channel (vendor HID or CDC serial), compiled out in release builds

## USB, host protocol and the driver layer

The protocol is the contract between firmware and GUI, so define it once in a single file (e.g. `protocol.yaml`) and generate the C/Rust firmware side and the TypeScript/Rust host side from it.

### USB descriptors (firmware side)

- [x] Composite device: keyboard interface (boot subclass, 6KRO), NKRO interface or report ID, consumer/system interface, optional mouse interface
- [x] Vendor-defined HID interface for config: usage page in the vendor range (0xFF00–0xFFFF), fixed 32 or 64-byte input and output reports, no report ID
- [x] String descriptors: manufacturer, product, serial number (unique per board so the GUI can tell two boards apart)
- [x] Correct `bcdDevice` set to the firmware version
- [x] Polling interval 1 ms for the keyboard endpoints

### Protocol design

- [x] Packet frame: command ID, sequence number, payload length, payload, status/error code; consider a CRC
- [x] Handshake: protocol version, firmware version, capability bitmap (which features this build has), matrix size, layer count, macro buffer size
- [x] Keymap: get/set one key at (layer, row, col); bulk read/write in chunks for fast full-keymap transfer
- [x] Advanced keys: get/set tap-hold settings, combos, tap dances, key overrides, encoder mappings
- [x] Macros: read/write the macro buffer in chunks, plus buffer size query
- [x] Indicators: get/set layer and lock-state indicator colours
- [x] Settings: debounce, tapping term, NKRO on/off, sleep timeout
- [x] Persistence: save to flash, revert to saved, factory reset
- [x] Live data: matrix state stream for the key tester, current layer, battery level
- [x] Device control: reboot, reboot to bootloader, identify (flash LEDs)
- [x] Error codes for unknown command, out-of-range index, busy, storage full, version mismatch
- [x] Rule: the device validates every index and length; a malformed packet must never crash or corrupt flash

### Keyboard definition file

- [x] JSON describing physical key positions, sizes and rotation (KLE-style), mapped to matrix (row, col)
- [x] Layout options (split spacebar, ISO vs ANSI enter), encoder positions, LED-to-key mapping
- [x] Either shipped inside the GUI or stored compressed in firmware and read over the protocol, so the GUI works with any of your boards

### Host library (your "driver")

- [x] Device discovery by VID, PID and the vendor usage page, so you open only the config interface
- [x] Transports: hidapi on desktop (Rust `hidapi` crate or `node-hid`), WebHID in the browser (Chrome, Edge and other Chromium browsers only)
- [x] Hotplug detection and automatic reconnect
- [x] Request/response matching with timeouts and retries
- [x] Typed API: `getKeymap()`, `setKey()`, `saveToFlash()` etc., with no raw bytes leaking to the GUI
- [x] Version negotiation: refuse to write with a clear message when firmware and app protocol versions are incompatible
- [x] Mock device implementation for GUI development and tests without hardware

### OS-specific gotchas

- Linux: a udev rule granting user access to the hidraw node for your VID/PID
- Windows: the OS opens keyboard interfaces exclusively, which is exactly why the config interface must be separate
- macOS: opening the keyboard interface triggers the Input Monitoring permission prompt; opening only the vendor interface avoids it

## Configurator GUI app feature list

Build against the mock device first, so the UI is usable before the firmware protocol is finished.

### Connection

- [x] Device picker (WebHID needs a user click to grant access; desktop can list devices directly)
- [x] Connection status, firmware version, protocol version, board name and serial
- [x] Clear messages for: no device, permission denied, incompatible firmware, device busy

### Keymap editor

- [x] Render the board from the keyboard definition file, with layout options
- [x] Layer tabs; show transparent keys and what they fall through to
- [x] Click a key, then pick from a searchable keycode palette grouped by category (basic, modifiers, media, layers, mouse, lighting, macros, custom)
- [x] Drag and drop between keys and from the palette
- [x] Show labels in the user's OS keyboard layout (keycodes are positions, so an AZERTY user sees A where a QWERTY user sees Q)
- [x] Undo/redo, unsaved-changes indicator, save and revert buttons

### Advanced editors

- [x] Tap-hold config per key (tap action, hold action, timing)
- [x] Combo editor (pick trigger keys, result, timeout)
- [x] Tap dance editor
- [x] Macro editor: record from the host keyboard, edit steps, insert delays, show buffer usage
- [x] Key override editor
- [x] Encoder mapping per layer
- [x] Indicator colour editor (layer and lock-state colours)
- [x] Status display layout editor (which widgets, where, custom text, layer names)
- [x] Global settings: debounce, tapping term, NKRO, sleep timeout

### Tools

- [x] Key tester showing live matrix state (catches dead switches and bad solder joints)
- [x] Import/export full configuration as JSON; named profiles
- [x] Firmware updater: check version, download or pick a file, reboot to bootloader, flash, verify, restore config
- [x] Printable keymap cheat sheet

### App quality

- [x] Cross-platform builds (Windows, macOS, Linux) or a hosted web app
- [x] Code signing: Windows Authenticode and macOS notarisation, or users get scary warnings
- [x] Auto-update for the desktop app
- [x] Keyboard-navigable and screen-reader-friendly UI
- [x] Light and dark themes

## Tooling, testing, security and release

### Repo layout (one monorepo)

- `protocol/`: the single protocol definition plus code generators
- `firmware/`: board support, core keyboard logic, USB, storage
- `host-lib/`: hidapi and WebHID transports, typed API, mock device
- `app/`: the GUI
- `boards/`: one folder per board with pin map and keyboard definition JSON
- `SPEC.md` and `CLAUDE.md`: this spec and project conventions for Claude Code

### Testing

- [x] Keep keycode processing, layers, tap-hold and combos hardware-independent so they run as host unit tests with simulated key events and timestamps
- [x] Protocol round-trip tests: host library against the mock device and against a firmware build running on the host
- [x] Fuzz the firmware's packet parser
- [x] Hardware-in-the-loop smoke test: flash, enumerate, read handshake, set a key, save, power-cycle, read back
- [x] Inspect descriptors with `lsusb -v` and `usbhid-dump` on Linux, or USB Device Tree Viewer on Windows
- [x] Measure scan rate and input latency with a logic analyser on a GPIO toggle
- [x] Test boot protocol in a BIOS/UEFI menu and through a KVM

### Security

- [x] Any program with access to the config interface can rewrite macros, which means it can make your keyboard type commands; treat protocol bounds checking as a security feature
- [x] Never log typed keystrokes in debug output that ships

### CI and release

- [x] CI builds firmware for every board, runs host tests, builds the app for three OSes
- [x] Firmware artifacts versioned with semver plus git hash; protocol version bumped on any breaking change
- [x] Changelog and a compatibility table (app version ↔ minimum firmware version)
- [x] User docs: flashing, recovery from a bad flash, udev rule on Linux

## Build order and Claude Code prompt sequence

One phase per Claude Code session works best: each phase ends with something you can test on real hardware. Save this doc as `SPEC.md` in the repo first, then reference it in every prompt.

1. **Scaffold.** "Read SPEC.md. Create the monorepo layout from the Tooling section for an nRF52840 using [C + Zephyr/nRF Connect SDK, or Rust + Embassy]. Add CI that builds firmware and runs host tests. Write CLAUDE.md with build, flash and test commands."
2. **Blinky and enumerate over USB.** "Bring up the board: clocks, GPIO, an indicator LED, and USB enumeration as a boot keyboard with VID/PID [x]. Add reboot-to-UF2-bootloader on a key."
3. **Matrix and basic keymap.** "Implement matrix scanning for the pin map in boards/[name], debounce (configurable), a 16-bit keycode model and a compiled-in 4-layer keymap. Keycode processing must be hardware-independent and transport-independent, with host unit tests."
4. **Full HID over USB.** "Add the NKRO report with boot-protocol fallback, consumer and system control, lock-key indicators, suspend/resume/remote wakeup."
5. **Protocol v1.** "Write protocol/protocol.yaml covering handshake, get/set key, bulk keymap, save, revert, factory reset, reboot and version mismatch errors. Generate firmware and host bindings. Add the vendor HID interface. Fuzz the parser."
6. **Persistence.** "Add versioned, wear-levelled settings storage with CRC, defaults, factory reset and schema migration."
7. **Host library.** "Build host-lib with hidapi and WebHID transports, hotplug, request/response matching, version negotiation and a mock device."
8. **GUI MVP.** "Build the app: connect, render the keyboard definition JSON, layer tabs, keycode palette, set/save keys, undo/redo, key tester. Develop against the mock device."
9. **Advanced keys and encoders.** "Add tap-hold, combos, tap dance, macros, key overrides, Caps Word, Auto Shift, Leader, Repeat, mouse keys and encoder mapping to firmware (with unit tests), extend the protocol, then add their GUI editors."
10. **Indicators and settings.** "Add layer and lock-state indicator colours and global settings (debounce, tapping term, NKRO, sleep timeout) with protocol commands and GUI editors."
11. **Firmware updater.** "Add the in-app firmware update flow: version check, reboot to UF2 bootloader, flash, verify, restore config."
12. **Bluetooth LE.** "Add BLE HID over GATT behind the same report layer as USB: multiple host profiles with switch keys, bonding management, battery service, and automatic USB/BLE output selection. Expose the config protocol over BLE too (a vendor GATT service, or vendor reports on the BLE HID service) so the GUI works wirelessly."
13. **Power management.** "Add idle sleep, deep sleep with wake on keypress, battery measurement and reporting to the GUI. Measure current draw in each state."
14. **2.4 GHz dongle.** "Write dongle firmware for a second nRF52840: a USB HID device that relays reports from the keyboard over a proprietary 2.4 GHz link (e.g. Nordic's ESB protocol), with pairing, and passes the config protocol through so the GUI can reach the keyboard via the dongle."
15. **Packaging, signing and auto-update** for the desktop app.

Prompting tips for this project:

- Paste the relevant checklist section into the prompt. Checkboxes in this doc mean "in scope", so ask Claude Code to track progress in a separate PROGRESS.md (phase, what is done, what is tested on hardware).
- Always ask for tests alongside features, and for the protocol version to be bumped when commands change.
- When something fails on hardware, give Claude Code the exact symptom plus `lsusb -v` or logic-analyser output rather than "it doesn't work".

Why wired-first: phases 1–11 give you a complete, testable keyboard and configurator over USB. BLE and the dongle then plug into a report layer and protocol that already work, so wireless bugs are isolated instead of tangled with keymap bugs.

## Not in v1

Dropped from your selections; kept here so they are easy to bring back later.

- Unicode input helpers
- Per-key RGB effects and the lighting editor beyond indicators
- Pointing device (trackball, trackpad)
- Haptic feedback
- Split halves and half-to-half sync
- Hall-effect analog sensing (actuation point, rapid trigger, calibration)
- Signed firmware images and A/B slots with rollback
- Physical unlock before config writes

One caution on that last pair: with Bluetooth and a dongle, more devices can reach the config interface, so revisit the physical unlock before shipping boards to other people.
