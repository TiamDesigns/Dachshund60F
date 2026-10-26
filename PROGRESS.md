# PROGRESS

## Current phase

Phase 15 (desktop packaging and auto-update) is done, and with it every phase in SPEC.md. Phases 1 to 14 and the Pup test board are merged to main. Core, host-lib, Doghouse and the desktop app's Rust tests pass locally, and the keyboard, Pup and dongle firmware build with no new warnings. Hardware testing for phases 2 to 14 waits for fabricated boards (or the Nordic nRF52840 Dongles Tiam is ordering: one runs the Pup, the other the receiver). What's left is in `SCHEDULED_TASKS.md`: bring-up and measurements on hardware, encrypting the 2.4 GHz link, and code signing for the desktop app.

## Done

### Doghouse 0.1.3: update checks, app settings and a UI pass (2026-10-26)

- Why 0.1.1 never offered 0.1.2: every Doghouse release so far (0.1.0 to 0.1.2) was built without the updater's public key, because the repo has no `TAURI_UPDATER_PUBKEY` variable yet. Those builds have no updater in them and no release has a `latest.json`, so they can't find or install updates. Installing 0.1.3 by hand is the way off them.
- Update checks (`app-version.ts`, `desktop.ts`): when the updater can't help (no key in the build, or its check fails), Doghouse asks GitHub's releases API for the newest `doghouse-v*` release and offers "Download from GitHub", which opens the release page (Rust command `open_releases_page`, fixed URL). The CSP allows `https://api.github.com`. The app checks at start-up and every six hours; errors are worded for people (offline, rate-limited, GitHub's status).
- App settings (sidebar → App settings, `AppSettings.svelte`): theme, the Doghouse, host-lib and protocol versions, and **Check for updates** with what it found or why it couldn't check, and when. The sidebar shows the version under the buttons; the About tab's footer had 0.1.0 hard-coded and now shows the real one (from `package.json` via Vite's `define`).
- The battery bone is a proper bone: two round knobs at each end and a thick shaft, with a clear outline in both themes.
- Accessibility and layout: light-theme text colours darkened to reach WCAG AA (muted, faint, accent, sage, key sub-legends) and the dark theme's faint text lightened; no text under 11 px; ghost buttons (Identify, Reset, Forget, Undo…) get an outline so they read as buttons; tabs move with the arrow keys, Home and End; the board list marks the selected board with `aria-current`; the header no longer breaks the serial number across lines; on the Advanced tab the ⓘ buttons sit beside their labels and the number boxes line up.
- Tests: `app-version.test.ts` (version order, picking the newest release, error wording). Check, test and build pass; looked at in Chromium in demo mode, light and dark. The desktop update paths weren't run (no Tauri build here); clippy runs in CI.

### Doghouse presets, feature info and a new dog (2026-10-26)

- Presets (Settings tab, `presets.ts`, `PresetPicker.svelte`): Everyday (the factory settings), Gaming (eager debounce, NKRO, a 30-minute nap, and tap-hold, tap dance, combos and Auto Shift off), Careful typing (wait-to-settle debounce, 230 ms tapping term) and Battery saver (10% lights, 5-minute nap). Save current settings as a preset adds your own; Doghouse keeps them in `localStorage`, so they work for every board. A preset sets every setting that changes how the board feels (not the name or colours), so switching always lands in the same place. The Presets section says which preset the board is on, or that it has its own mix. Switching is live like any other change; Save to board keeps it.
- Firmware: the options entry of the advanced-key tables gets an `off` byte (protocol.yaml; capability `feature_switches`, bit 13). With tap-hold off, LT and MT keys type their tap key the moment they go down; with tap dance off, TD keys send their one-tap action at once; with combos off, no key is held back waiting for a combo partner. The tables themselves are kept, and the field is saved with the other options. Not a breaking change: no protocol version bump, older firmware keeps its 14-byte entry, and host-lib reads it and writes only 14 bytes to it (refusing to switch features off there).
- Settings has a new "Speed or tricks" section with the three switches, and the Advanced tab says when a table is switched off.
- Info buttons: an ⓘ beside each setting, advanced option and table, the macros and where keys go, with what it does and its pros and cons (`feature-info.ts`, `InfoTip.svelte`).
- The dachshund has a proper eye (white, pupil and a glint; a closed eye while it naps), its far legs are always a shade darker than its coat, and it keeps the same coat in light and dark mode (the empty kennel shows the same dog, faded).
- Tests: core tests for each switch, the options entry and saving it; host-lib tests against the host-built firmware and a fake board with the old 14-byte entry; Doghouse tests for presets. Looked at in Chromium in demo mode, light and dark. Not tried on hardware.

### Doghouse polish after the first Windows build (2026-10-26)

- Demo mode can be left: when it was opened with `?demo` (rather than forced by a browser without WebHID), the demo panel in the sidebar has **Leave demo mode**, which reloads without `?demo`.
- Key legends are one size. Long legends were shrunk by length alone (so Space, Shift and Enter were smaller than Bksp); now a legend only shrinks when it wouldn't fit its key (`legendSize` in `geometry.ts`). Shifted symbols are the same size as the base legend, with a gap between the two.
- Battery colour (`battery.ts`): white when full, shading to yellow as it drains, and red below 15%. The battery LEDs on the board picture (the four dots right of the layer LED, above the arrows) and the bone gauges in the sidebar and header use it; charging still shows the bolt.
- Tests: `battery.test.ts` and a `legendSize` test; check, test and build pass, and the changes were looked at in Chromium in demo mode (light and dark).

### Phase 15: Doghouse desktop app, packaging and auto-update (2026-10-24)

- Decided with Tiam: Tauri 2 with auto-update. The app is `app/src-tauri/` (Rust): the same web UI in a native window for Windows, macOS and Linux, version taken from `app/package.json`, bundle identifier `com.tiamdesigns.doghouse`, icons made from the doghouse logo.
- HID (`src/hid.rs`): macOS and Linux WebViews have no WebHID, so the app uses hidapi on every OS. It lists devices, opens one per handle, writes reports and streams input reports to the UI over a Tauri channel (non-blocking reads, so a write waits at most 1 ms). On macOS devices are opened shared (hidapi's `macos-shared-device`), or the board would stop typing while Doghouse has it open. host-lib's new `TauriHidBoardManager` (`tauri-hid.ts`) puts the node-hid manager on that bridge, so finding boards, Bluetooth report IDs, the dongle and hotplug (a 1 s scan) all work the same as with node-hid. host-lib takes no Tauri dependency: the app passes in `invoke` and channels.
- Firmware updates in the app (`src/uf2.rs`): after the board reboots into UF2, Doghouse finds its drive by itself (mounted FAT drives with an `INFO_UF2.TXT` whose Board-ID matches, waiting up to 20 s for it to mount) and writes the file. There's no drive picker and nothing to click. The app only writes a well-formed UF2 image onto a drive that it lists as one.
- Auto-update: Tauri's updater plugin checks `releases/latest/download/latest.json` at start-up. A newer version shows "Doghouse x.y is out" in the sidebar with Install and restart (download progress, then a relaunch). The updater is only built in when the repo has the public key (variable `TAURI_UPDATER_PUBKEY`); update files are signed with the private key (secret `TAURI_SIGNING_PRIVATE_KEY`), which Tiam makes once (`app/src-tauri/README.md`).
- Releases: `.github/workflows/doghouse-release.yml` (manual, or a `doghouse-v*` tag) builds Windows (NSIS and MSI), macOS (universal .dmg and .app) and Linux (.deb, .rpm and AppImage) with tauri-action, and puts them and `latest.json` on a draft release. The .deb and .rpm install a udev rule (`linux/70-dachshund.rules`) so the logged-in user can open the boards' hidraw devices, over USB and Bluetooth.
- In the app, Doghouse hides "Connect a board…" (boards come in on their own) and the "this browser can't reach USB devices" note; demo mode still works with `?demo`.
- CI: a new job, "Doghouse desktop app (Rust)", runs `cargo fmt --check`, clippy with warnings as errors and `cargo test` on Linux.
- Tests: Rust unit tests for finding a UF2 drive and refusing anything that isn't a UF2 image or isn't on a listed drive. A host-lib test runs `TauriHidBoardManager` against a stand-in for the app's commands: it opens boards, writes with report ID 0 and drops a board when its reader closes. host-lib 152, Doghouse 14 and Rust 2 pass.
- Tried locally on Linux under Xvfb: the app starts and loads the UI under its content security policy, and finds no boards (there's no HID in the container). A release build signed with a throwaway key made the .deb (2.8 MB, with the udev rule), the AppImage and their `.sig` files. Pointed at a local `latest.json` for 0.2.0, the app offered the update. Installing it wasn't tried.
- Not tested yet: the app against a real board on each OS, Windows and macOS builds (CI builds them on release), installing an update, and the drive write on a real UF2 bootloader. Code signing (Authenticode, Apple notarisation) isn't set up; the installers are unsigned. These are in `SCHEDULED_TASKS.md`.
- How to test: `cd app && npm install && npm run tauri dev` (setup per OS in `app/src-tauri/README.md`). Plug in a board or the Pup and it shows up with no Connect step. For a release, set up the keys, then run the "Doghouse release" workflow.

### The Pup: keyboard firmware on an nRF52840 Dongle (2026-10-24)

- Tiam asked for it so Bluetooth and the 2.4 GHz dongle can be tried on the nRF52840 Dongles being ordered, before the PCBs exist. `boards/Pup/` holds its `board.json`, `keyboard.json` (one key, one indicator) and `firmware/`: the devicetree overlay (SW1 on P1.06, LD2's red, green and blue on P0.08, P1.09 and P0.12, all from Nordic's PCA10059 documentation; flash layout with the code at 0x1000 after Nordic's MBR, `bt_settings` at 0xd6000 and `storage` at 0xd8000, below Nordic's bootloader at 0xe0000) and its default keymap (`a`).
- It builds on Zephyr's own `nrf52840dongle/nrf52840` board, so it needs no Zephyr board files: `firmware/boards/nrf52840dongle_nrf52840.overlay` (which Zephyr picks by board name) includes `app.overlay` and the Pup's overlay, and `firmware/boards/nrf52840dongle_nrf52840.conf` sets the names (USB product and Bluetooth name "Dachshund Pup", `KB_MODEL` "Pup"), Nordic's bootloader and a 10 s recovery hold. Build: `west build -p -b nrf52840dongle/nrf52840 -d build/pup firmware`.
- Firmware changes, none for the Dachshund60F: a board without `matrix-col-gpios` is wired directly (each row pin one key, read as column 0, with its devicetree flags for the pressed level), the indicators can drive one RGB LED on GPIOs (`indicator-rgb-gpios`, Caps Lock) instead of an SK6812 chain, the GPREGRET value for the bootloader is a Kconfig option (`KB_BOOTLOADER_GPREGRET`: 0x57 for the Adafruit UF2 bootloader, 0xB1 for Nordic's), and a board folder named by `KB_MODEL` with `firmware/default_keymap.c` brings its own default keymap.
- host-lib and Doghouse: the Pup's default keymap is in `default-keymaps.ts` (so the mock can simulate one), and Doghouse knows its definition ("Pup · Puppy · 1 switch"). The board tests accept direct wiring and LEDs on their own pins, and new tests check the Pup's overlay pins, names and USB IDs, LED table and default keymap against `board.json`, `keyboard.json` and host-lib.
- CI builds the Pup (debug and release) as `Firmware (pup)`.
- Size, debug: 267.8 KB flash, 77.1 KB RAM; release 229.3 KB. The Dachshund60F's build is unchanged.
- Not tested on hardware yet: everything (`SCHEDULED_TASKS.md`, "Pup on real hardware"), including whether Nordic's bootloader takes GPREGRET 0xB1.
- How to test: host-lib and Doghouse tests as in CLAUDE.md. With a PCA10059: build, flash and try it as in `boards/Pup/README.md`.

### Phase 14: 2.4 GHz dongle (2026-10-24)

- Link (`kb/link.h`, host-tested): Enhanced ShockBurst at 2 Mbps on 2475 MHz (above Wi-Fi channel 11, below Bluetooth's 2480 MHz advertising channel), packets up to 72 bytes. The keyboard transmits (PTX) and the dongle listens (PRX). Keyboard to dongle: POLL, REPORT (kind plus the USB report), CONFIG (a response frame), PAIR. Dongle to keyboard, in ESB acknowledgements: STATUS (lock LEDs; USB ready, boot protocol, resync), CONFIG (a request frame), PAIR_OFFER. Every packet is length- and type-checked before use.
- Pairing: the dongle's link address comes from its device ID (FNV hash, never the pairing address or a preamble lookalike), so it never changes. The dongle's button turns on pairing for 30 s: it also listens on a fixed pairing address and offers its address there. `RF_PAIR` (new keycode, Doghouse "2.4G Pair") or Doghouse makes the keyboard ask on the pairing address for 30 s; it takes the first offer, keeps it in Zephyr settings (`kb/rf/dongle`, versioned, next to the Bluetooth bonds) and moves to it. The link isn't encrypted yet.
- Keyboard (`src/kb_esb.c`, `CONFIG_KB_DONGLE`, on by default): ESB on the MPSL timeslot API, so it shares the radio with Bluetooth (Nordic marks this experimental). Reports go out the moment they're queued. The keyboard polls every 10 ms in use, 2 ms while config traffic flows, 100 ms while idle, and not at all once power management turns the radios off (the next key starts it again) or nothing is paired. Three failed packets in a row (each after 6 retransmits) mark the dongle gone; a single failure triggers a resync, so a lost release can't stick a key.
- Output selection: the dongle is a third output. Keys go to the preferred one (`OUT_USB`, `OUT_2G4`, `OUT_BLE`, or Doghouse) while it's ready, otherwise to the first ready of USB, the dongle and Bluetooth. The dongle counts as ready when it answers and its computer has configured it. Lock LEDs, boot protocol and resync follow it like the other outputs.
- Dongle firmware (`dongle/`): reuses the keyboard's USB device (`firmware/src/kb_usb.c` and `app.overlay`) with its own IDs (1209:0002, "Dachshund60F Receiver", `dongle/Kconfig`), relays reports to USB and config frames both ways, releases every key if the keyboard is quiet for 400 ms, and answers Doghouse with the new `unreachable` status while the keyboard is away. RGB LED: white at power-up, blue blinking while pairing, green on pairing and when the keyboard comes in range, red when it goes. Built for the nRF52840 Dongle (PCA10059), whose button and LED pins match the Dachshund dongle's; flash and pair steps are in `dongle/README.md`.
- Protocol (still version 1; added): `get_dongle` 0x54 (paired, link state, pairing), `pair_dongle` 0x55 (stop, start, forget), the `dongle` capability (bit 12), `set_output 3`, and the `unreachable` status (10), which only a dongle sends.
- host-lib: the dongle's PID is found by both managers (connection `"dongle"`); while its keyboard is away the dongle isn't listed, and it's tried again every 2 s (`dongleRetryMs`), so the board leaves and comes back by itself. `Board` gains `getDongle()` and `pairDongle()`, and `setOutput` takes `"dongle"`. The mock simulates the dongle; `RF_PAIR` is in the keycode catalogue. A board test checks that the dongle's Kconfig USB IDs match board.json and both ends use the same ESB payload size.
- Doghouse: the Connection tab offers 2.4 GHz as an output and has a dongle card (state, Pair a dongle, Stop, Forget).
- Tests: core `test_link` (every packet, malformed input, addresses), `test_output` (dongle preference and fallback order), `test_protocol` (`get_dongle`, `pair_dongle`, `set_output 3`, refusals) and the fuzz command list; host-lib dongle tests against the mock and the host-built firmware, and the WebHID dongle wait-and-return test. Core 20/20, host-lib 131, Doghouse 14 pass.
- Size, debug without a screen: 274.0 KB flash, 78.2 KB RAM; release 234.8 KB. Dongle debug 74.1 KB flash, 24.7 KB RAM; release 58.6 KB.
- Not tested on hardware yet: pairing, range, latency (key to USB), dropouts next to Wi-Fi, the keyboard's current while polling, and Bluetooth and ESB sharing the radio. These are in `SCHEDULED_TASKS.md`.
- How to test: core and host-lib tests as in CLAUDE.md; `cd app && npm run dev`, open http://localhost:5173/?demo, Connection tab. With hardware: flash an nRF52840 Dongle (`dongle/README.md`), pair, prefer 2.4 GHz, type, then unplug the dongle and check that keys move to USB or Bluetooth with nothing stuck.

### Phase 13: power management, battery gauge and lower input delay (2026-10-24)

- Decided with Tiam: stay on the nRF52840 with 1 kHz scanning and polling (full-speed USB tops out at 1 kHz; the nRF54H20 move is off), and work on input delay instead.
- Wake-on-press scanning (`CONFIG_KB_WAKE_ON_PRESS`, on): while no key is down, every column is driven high between scans and the rows wait on a level interrupt, so the first key of a burst is scanned the moment it closes instead of at the next 1 ms tick, and the scan timer restarts from there. That saves half a scan period on average for most presses. Debounce stays eager on press (no added delay).
- Power states (`kb/power.h`, host-tested): quiet means no key down and nothing in the engine waiting on time (`kb_engine_idle`: tap-hold, combos, Caps Word, Leader, macros, mouse keys) and debounce settled (`kb_debounce_idle`). After 500 ms quiet (`CONFIG_KB_IDLE_MS`) the scan timer stops and the scan thread sleeps until a matrix or encoder interrupt, with no delay on the next key. On battery only: after 60 s the indicators and an OLED go dark (`CONFIG_KB_LIGHTS_OFF_S`), after 5 min Bluetooth stops advertising if no host is connected (`CONFIG_KB_ADVERTISING_OFF_S`; the next key starts it again), and after the sleep timeout from Settings (15 min by default, 0 = never) the board goes into deep sleep. USB power never sleeps.
- Deep sleep (`src/power.c`): tells the Bluetooth host it's going, turns off the LEDs' load switch and an OLED, leaves the columns driven high with the rows' SENSE armed, and enters System OFF. A key press wakes the board through a reset; the LED walk is skipped then (reset cause `RESET_LOW_POWER_WAKE`). The waking key is typed only if it is still held once a host is back. A new keycode, `QK_SLEEP` (Doghouse: Sleep), sends the board to sleep once every key is up, on battery only. A cell under 3.3 V sends it to sleep at once.
- Battery gauge (`kb/battery.h`, host-tested): VDDH/5 on the SAADC (a new `&adc` channel in the board devicetree, no pin), read once a minute on battery and turned into a percentage with a LiPo curve, smoothed and only ever going down while on battery. On USB the power path feeds VDDH from VBUS, so the cell isn't read; CHG_STAT (P0.15) shows charging, and the gauge reads 100 once charging stops. The level goes to the Bluetooth Battery Service, the indicators' 4-step gauge and the status screen.
- Advertising: fast for 30 s, then slow (1 s), and paused as above.
- Protocol (still version 1; commands added): `get_power` (percent, cell mV, charging, USB power; with the `battery` capability, set when the gauge is up) and `get_timing` (scan period, matrix read time, key-change-to-report time, average and max, scans started by a key interrupt, total scans; optional reset; the `timing` capability). Polling commands (`get_power`, `get_connection`, `get_timing`) don't keep the board awake; any other request counts as use.
- host-lib: `getBattery()` now reads `get_power` (with `millivolts` and `usbPower`), and `Board` gains optional `getTiming(reset)` (`InputTiming`). The mock simulates both. `QK_SLEEP` is in the keycode catalogue.
- Doghouse: the battery is refreshed every 30 s for real boards. About shows the cell voltage and power source, and a new **Input timing** card reads and resets the board's timing counters.
- Tests: core `test_power` (every threshold, USB, critical battery, wrap), `test_battery` (curve, unknown, smoothing, only down, charge and unplug, critical), `test_engine` (`kb_engine_idle`), `test_debounce` (`kb_debounce_idle` for every algorithm), `test_protocol` (both commands, refusals) and the fuzz test. host-lib: battery and timing against the mock and the host-built firmware.
- Size, debug without a screen: 261.0 KB flash, 74.5 KB RAM; release with an SSD1306 128x32: 244.6 KB.
- Not tested on hardware yet: current in each state, wake from System OFF, whether the reset cause survives the UF2 bootloader (if not, the LED walk shows after each wake), the gauge against a real cell. These are in `SCHEDULED_TASKS.md` and Stage 4 of the bring-up plan.
- How to test: core and host-lib tests as in CLAUDE.md; `cd app && npm run dev`, open http://localhost:5173/?demo, About tab (Input timing). On a board on battery: leave it 60 s (lights out), 5 min (advertising stops), 15 min (deep sleep), then press a key; read About → Input timing after typing.

### Phase 12: Bluetooth LE (2026-10-24)

- Firmware (`src/kb_ble.c`): HID over GATT with Nordic's HIDS library, plus the Battery and Device Information services. The report map (`kb_hid_ble_desc()` in the core) carries every USB report: the boot keyboard report and lock LEDs take report ID 5, NKRO, consumer, system and mouse keep IDs 1-4, and the config protocol is a vendor collection with report ID 6 (`hid.ble_report_id` in `protocol.yaml`). Boot protocol works too. Reports go through a queue and their own thread; a lost report triggers a full resync, like USB.
- Host profiles: three (`CONFIG_BT_ID_MAX=3`, matching `BT_SEL(0)`-`BT_SEL(2)` in Doghouse). Each profile has its own identity (address) and one bond, so each computer sees its own Dachshund60F. A profile with a bond only takes its own computer: anyone else who connects is dropped, and pairing from another computer is refused until `BT_CLR` (or Doghouse) forgets the host. Pairing is Just Works (no display). The selected profile, bonds and identities live in Zephyr settings (NVS, CRC-checked) on a new 8 KB `bt_settings` partition at 0xEA000, inside the area the UF2 bootloader never erases.
- Output selection (`src/output.c`, logic in `kb/output.h`): keys go to the preferred output (USB by default; `OUT_USB`, `OUT_BLE` or Doghouse change it, and it is kept across power cycles) while its host is ready, and to the other one otherwise. USB counts as ready once configured (also while suspended, so a key still wakes the computer); Bluetooth once the link is encrypted. On a switch, the old host gets all-released reports so nothing sticks down, and the new one gets the full state. Lock LEDs, the indicators and the status screen follow the active output. USB now starts and stops with VBUS, so unplugging moves keys to Bluetooth at once.
- Protocol (still version 1; commands added): `get_connection` (active and preferred output, USB and Bluetooth state, selected profile, profile count, paired mask), `set_output`, `select_profile`, `clear_profile`, and the `bluetooth` capability. The config protocol works over Bluetooth with the same frames; responses go back the way their request came. A 64-byte report needs an ATT MTU of 67, so the board asks for 247.
- host-lib: `Board` gains optional `getConnection`, `setOutput`, `selectProfile` and `clearProfile` (`ConnectionStatus`). WebHID reads the config collection's report ID and reports `connection: "ble"` when it isn't 0; node-hid does the same from hidapi's bus type. A board reachable over both USB and Bluetooth is listed once, over USB, and its Bluetooth link takes over when the cable comes out. The mock simulates hosts and profiles.
- Doghouse: a **Connection** tab: where keys go now, a USB/Bluetooth preference, and the three Bluetooth hosts with Switch to and Forget (with a warning when that would cut off Doghouse's own Bluetooth link). It refreshes every 2 s while open, so pairing shows up. Demo mode can "pair over Bluetooth".
- Not yet: passkey pairing, and the board's name from Settings as its Bluetooth name.
- Tests: core `test_output` (selection rules), `test_hid` (the Bluetooth report map's report IDs and sizes, released reports), `test_protocol` (every new command and refusal) and the fuzz test (new commands; output and profile stay in range). host-lib: the connection API against the mock and the host-built firmware, WebHID and node-hid over Bluetooth, the USB-over-Bluetooth listing, and `prj.conf`'s PnP IDs and profile count against `board.json` and host-lib. Doghouse's Connection tab was run in Chromium in demo mode.
- Size, debug without a screen: 250.7 KB flash, 74.0 KB RAM (the Bluetooth stack); release 213.5 KB.
- How to test: core and host-lib tests as in CLAUDE.md; `cd app && npm run dev`, open http://localhost:5173/?demo, Connection tab. On a board: pair it from a computer's Bluetooth settings, unplug the cable while typing, switch hosts with BT1-BT3, forget one with BT Clr, and open Doghouse over Bluetooth (Connect a board… lists it).

### Phase 11: in-app firmware updates (2026-10-24)

- Firmware: every image carries a 32-byte info block (`struct kb_fw_info` in `kb/version.h`: the magic `KBFWINFO`, layout, protocol version, firmware version and model), so Doghouse can tell what a `.uf2` file holds before flashing it. The handshake now takes its version and model from the same block, which also keeps it in the image.
- host-lib: `uf2.ts` checks a UF2 file block by block (magic numbers, numbering, nothing missing), refuses another chip family or any block outside the application flash (0x26000-0xEA000, so never the SoftDevice, the bootloader or the settings), and reads the info block; `compareVersions` and `uf2BoardId` (the Board-ID from the drive's INFO_UF2.TXT). `backup.ts`: `backupBoard` reads the keymap, encoders, settings, advanced keys, macros and colours into one JSON object, and `restoreBoard` writes back only what differs, then saves. The mock board can now reboot (unsaved edits are lost) and, in its simulated bootloader, take "firmware" through `MockBoardManager.flash()`.
- `keyboard.json` names the board's bootloader (`uf2BoardId`, `volume`), checked against the bootloader's `board.h`.
- Doghouse: About → **Update firmware…** picks a `.uf2`, checks it is firmware for this model, shows the version change and warns about downgrades and a different protocol version. Then a dialog runs the steps: back up the settings (also kept in the browser, and downloadable), restart into the bootloader, choose the DACHSHUND drive (File System Access API in Chrome and Edge; the drive's Board-ID is checked) and write the file, wait for the board to come back, check it runs the new version, and put back anything the update lost. Browsers without the drive picker get a download link and "I've copied it". About also gets **Restore a backup…**, and demo boards a **Try a pretend update** button that runs the whole flow.
- Tests: core `test_version` pins the info block's layout; host-lib `uf2.test.ts` (round trip, the info block across a block boundary, every refusal, versions, Board-ID) plus a check against the real `zephyr.uf2` done by hand (found 0.1.0, Dachshund60F, protocol 1); back up, factory reset and restore against both the mock and the host-built firmware; the mock's reboot and flash. The demo update flow was run end to end in Chromium.
- Size, debug without a screen: 94.1 KB flash, 40.3 KB RAM.
- Not yet tested: writing to a real UF2 drive from the browser (needs a board or an nRF52840 dongle with the Adafruit bootloader).
- How to test: `cd app && npm run dev`, open http://localhost:5173/?demo, About → Try a pretend update. On a board: build a firmware with a higher `VERSION`, then About → Update firmware….

### Phase 10: indicator colours and global settings (2026-10-24)

- Protocol (still version 1; only commands were added): `get_settings` and `set_settings` carry the debounce algorithm and time, tapping term, NKRO, sleep timeout, indicator brightness and the board's name (16 bytes of UTF-8) as one struct, checked whole with `kb_settings_valid()`. `get_indicators` and `set_indicators` carry the Caps, Num and Scroll Lock colours, the battery gauge's normal and low colours and the low level; `get_layer_colour` and `set_layer_colour` the layer LED's colour per layer. Colours are 0x00RRGGBB, and anything with a top byte set is refused. The firmware now reports the `settings` and `indicators` capabilities.
- Firmware: `keyboard.c` owns the indicator colours with the settings. A change applies at once through a new `settings_changed` protocol op: debounce restarts only if it changed, the tapping term and NKRO switch over, and the LEDs redraw (`indicators_set_config()` replaces `indicators_set_brightness()`). Applying settings no longer resets the macro edit buffer. The sleep timeout is stored but used from phase 13.
- Saving: a new `KB_SECTION_INDICATORS` section (the low level, then RGB for each colour and layer; no schema bump). An older blob without it keeps the default colours, and a section with fewer or more layers loads what fits.
- host-lib: `BoardSettings` gains `debounceAlgorithm`, and `SETTINGS_LIMITS` now match the firmware (debounce 0-100 ms, tapping term 50-2000 ms, sleep up to 240 min, the name in bytes). `Board` gains optional `getIndicators` and `setIndicators` with an `IndicatorColours` type ("#rrggbb" strings). `HidBoard` implements settings and colours; `settings.ts` and `indicators.ts` hold the firmware's checks and defaults, shared by the mock.
- Doghouse: the Settings tab now works on real boards. New: a debounce style picker, a "Light colours" group (layer light per layer, the lock lights and battery gauge the board actually has, and the low-battery level), and the name check counts bytes. Sliders keep their useful ranges but stretch to show a value set elsewhere.
- Docs: Tiam chose not to register pid.codes PIDs while the board stays on their desk. The README warns anyone rebuilding the project to use their own VID/PID before giving boards away, and the design decisions page records the choice.
- Tests: core `test_protocol` (every new command, each out-of-range field, not-supported, revert and factory reset), `test_settings` (the indicator section's round trip, older and newer layouts, malformed sections, the sector budget), and the protocol fuzz test now sends every command and checks the settings stay valid (also clean under ASan and UBSan). host-lib: settings and colour tests run against both the mock and the host-built firmware, plus a save and reboot round trip.
- Size, debug without a screen: 94.0 KB flash, 40.3 KB RAM.
- How to test: core and host-lib tests as in CLAUDE.md; `cd app && npm run dev`, open http://localhost:5173/?demo, change the colours and settings on the Settings tab, Save, change them again, and Revert. On a board: change the layer colour and switch layers, toggle Caps Lock, and change debounce while typing.

### README and design decisions (2026-10-24)

- The repo is public. `README.md` covers why the project exists, its goals (hardware, firmware, wireless, driver and app, process), the board, phase status, how to try it, and the licences with a disclaimer (no warranty, LiPo safety, test USB PIDs, certification).
- `docs/design-decisions.md` lists the decisions Tiam made personally (scope, hardware, process) and the topics the project has covered.

### Phase 9c: Doghouse editors for advanced keys and macros (2026-10-23)

- Doghouse gets two tabs, shown only for boards whose firmware has the `advancedKeys` and `macros` capabilities. **Advanced** edits the options (Leader timeout, Caps Word on both Shifts, Auto Shift, mouse keys) and the five tables: combos, tap dances, key overrides, Leader sequences and per-key tap-hold rules. Each entry is edited in a small form, checked as the firmware checks it before Apply, and summarised in one line. **Macros** edits M0 to M31 as steps (type text, tap, hold, release, pause), shows how much of the 1 KB macro buffer they use, and sends them together.
- The keymap editor's palette has a "Macros & dances" group (`M(n)`, `TD(n)`), plus `AS_TOGG`, the dynamic-macro keys and mouse buttons 4 and 5 and horizontal scroll. Keycode fields take any catalogue key or a modded key such as `LSFT(KC_1)`.
- host-lib: `advanced.ts` has the slot counts, limits and default options, and checks entries, options and macros the way the firmware's `kb_features_valid` does. The mock board now has the advanced-key and macro methods, saved, reverted and factory-reset with the keymap. The advanced-key and macro tests in `hid-board.test.ts` run against both the mock and the host-built firmware, so the two can't drift apart.
- Not yet: undo and redo for these tabs (in SCHEDULED_TASKS.md).
- How to test: `cd host-lib && npm test` (with `KB_HOST_DEVICE` set, as CI does, for the firmware half); `cd app && npm run check && npm test && npm run build`; `npm run dev` and open http://localhost:5173/?demo, then add a combo, a tap dance and a macro, Save, change something, and Revert.

### Phase 9b: advanced keys in the firmware, protocol and host library (2026-10-23)

- Protocol (still version 1; only commands were added): `get_feature` and `set_feature` read and write one entry of an advanced-key table (options, combos, tap dances, key overrides, Leader sequences, tap-hold rules, encoder mapping); `get_macro_chunk`, `set_macro_chunk` and `apply_macros` edit the macro buffer. Table ids, entry sizes and byte layouts are in `protocol.yaml` (`feature_tables`), and the generator now emits them and supports `u8[]` fields. A `set_feature` is checked against every table together with `kb_features_valid()` and undone on an error; `apply_macros` checks every step, and a refused edit is dropped. Revert and factory reset refresh the macro edit buffer.
- Saving: two new settings sections (no schema bump), `KB_SECTION_FEATURES` (blocks of table, entry size, count, entries) and `KB_SECTION_MACROS` (the buffer without trailing zeros). Loading checks the whole set and keeps the defaults if it fails; entries from newer or older firmware (longer or shorter) load field by field. The worst case (4 layers of 6×15 and a full macro buffer) fits one 4 KB store sector.
- Engine: `kb_engine_set_features()` is safe to call while keys are down. Keys waiting to become a combo go on as plain presses, fired combos are released, and a playing macro stops and lets go of its keys.
- Firmware: `keyboard.c` owns the tables (defaults: volume on the roller, tracks on Fn, as in Doghouse), loads and saves them with the keymap, and hands them to the engine. Mouse keys send a new mouse report (report ID 4 on the extra interface: five buttons, X, Y, wheel and horizontal pan). The encoder is read on the scan thread through a new quadrature decoder in the core (`kb/encoder.h`: counts transitions with direction, ignores impossible jumps, bounce cancels out; `CONFIG_KB_ENCODER_STEPS_PER_DETENT`, default 4) and fed to `kb_engine_encoder()`. The handshake now reports the `advanced_keys`, `macros` and `encoders` capabilities, one encoder and the 1 KB macro buffer.
- host-lib: `Board` gains optional `getAdvancedKeys`, `setAdvancedEntry`, `setAdvancedOptions`, `getMacros` and `setMacros`, with types for each table and `MacroStep` (tap, down, up, delay, text). `HidBoard` implements them and the encoder mapping (`getKeymap().encoders`, `setEncoder`). `advanced-codec.ts` turns entries and macros into bytes and back. The keycode codec knows `M(n)`, `TD(n)`, `AS_TOGG` and the `DM_` keys.
- Tests: core `test_encoder` (detents, bounce, impossible jumps, resolution), new `test_protocol` cases (every feature command and error, the edit-and-apply flow, revert), `test_settings` (round trip, invalid sets, forward and backward compatibility, the sector budget), engine tests for tables changing under held keys, the mouse report descriptor and packer. The protocol fuzz test now also checks that the tables stay valid. host-lib: codec tests, and round trips against `kb_host_device` (encoders, every table, options, macros, save and reboot, factory reset), which now runs on a 4 KB-sector flash like the firmware's.
- Size, debug without a screen: 92.1 KB flash, 40.3 KB RAM (the tables, the macro edit buffer and a 4 KB save buffer).

### Phase 9a: advanced keys in the core engine (2026-10-23)

- New `kb/features.h`: fixed-size tables the engine runs from, which the firmware will own, save and edit over the protocol in 9b. 16 combos (2–4 keys, a timeout and a layer mask each), 16 tap dances (`TD(n)`: one tap, hold, two taps, three taps), 16 key overrides (exact modifiers + key → replacement, e.g. Shift+Backspace = Delete), 16 Leader sequences (up to 4 keys), 16 per-key tap-hold rules, encoder actions per layer (transparent falls through), Caps Word, Auto Shift and mouse-key settings, and a 1 KB macro buffer. `kb_features_valid()` checks every index and keycode, so tables from the host or flash can't do anything a keymap couldn't.
- Macros (`M(0)`–`M(31)`): steps that tap, press or release a keycode, wait, or type ASCII text (US layout). Keys a macro presses and doesn't release come up when it ends.
- New keycodes: `M(n)`, `TD(n)`, `AS_TOGG`, `MS_BTN4`/`MS_BTN5`, `MS_WHLL`/`MS_WHLR`, and dynamic macros `DM_REC1`, `DM_REC2`, `DM_RSTP`, `DM_PLY1`, `DM_PLY2`.
- Engine: key events pass a combo stage, then the tap-hold stage, then the keymap. Fired combos and encoder detents enter as events of a virtual key row, so they take part in tap-hold decisions like any key. Tap-hold options, globally or per key: permissive hold, hold on other key press, quick tap and retro tapping. Also Caps Word (letters shifted, `-` becomes `_`, digits and Backspace keep it going, ends on anything else or after 5 s idle; optionally both Shifts toggle it), Auto Shift (held past 175 ms = shifted), Repeat, Leader, two dynamic macro slots of 64 steps, and mouse keys with acceleration through a new mouse report callback. All of it is written for this repo, not taken from QMK.
- `kb_engine_encoder()` turns a detent into a tap of the encoder's keycode on the active layer.
- Tests: `test_features` covers each feature and its edge cases (combo timeouts, overlapping combos, early release, layer masks, dance interrupted, override modifier hiding, Caps Word timeout, rolling under Auto Shift, Leader prefixes, macro delays, encoder layers, table validation) plus a randomized test. It runs 200 rounds of random presses, detents and ticks with every feature set up, and checks that nothing stays held afterwards. A larger run (20,000 rounds under ASan and UBSan) is clean. That test found three bugs, which are fixed: a modifier counted twice when two features added it to one key, macro-held keys never released, and a clock that could lag behind the first event.
- The firmware builds unchanged against the new engine (no tables yet): 87.8 KB flash, 32.8 KB RAM, debug without a screen.

### Phase 8: Doghouse MVP on host-lib (2026-10-23)

- Doghouse now finds real boards through `WebHidBoardManager`: **Connect a board…** in the sidebar opens the browser's picker the first time, and after that boards open on their own when plugged in and leave when unplugged.
- Demo mode keeps the simulated boards: it's on with `?demo` in the URL, or in a browser without WebHID (which also says to use Chrome or Edge). The empty kennel links to it. host-lib's new `CombinedBoardManager` puts real and demo boards in one list.
- The app follows the firmware's capabilities: no settings section without the `settings` capability (the current firmware; phase 10 adds it), a note on the roller until encoders arrive (phase 9), and Identify, Save, Revert and factory reset only when supported. Loading no longer needs settings or a battery reading.
- A board on another protocol version shows in the pack with a message saying whether to update Doghouse or the firmware, and nothing is written to it. A board model Doghouse has no definition for gets a toast and stays out of the pack.
- About: the firmware's features, and **Restart into bootloader…** (protocol `reboot` into UF2) for firmware updates.
- host-lib: `Board.reboot` (optional), `CombinedBoardManager`, and boards without a serial number (WebHID) get a unique id from their manager.

### Phase 7: host library (2026-10-23)

- `keycode-codec.ts`: keycode ids (`KC_A`, `MO(1)`, `LT(1,KC_SPC)`, `LSFT_T(KC_A)`, `OSM(MOD_LSFT)` and so on) to the firmware's 16-bit numbers and back. The numbers come from `keycodes.gen.ts`, which `protocol/gen/gen.py` now generates from `kb/keycodes.h`. A number with no name comes back as `RAW(0x....)` and encodes to the same number, so any keymap round-trips.
- `transport.ts`: the Transport interface (write a report, listen for reports and for the device going away).
- `client.ts`, `ProtocolClient`: one request in flight at a time, each with its own sequence number; responses are matched by command and sequence, so a late answer is never taken for the next request. A timeout is retried twice with a new sequence number, except `reboot`. Error statuses become `BoardError`s; `BoardErrorCode` gains `storage-error`, `not-supported`, `timeout` and `protocol-error`.
- `hid-board.ts`, `HidBoard`: the typed `Board` over the protocol. Opening runs the handshake and `get_serial` (the board's id). Version negotiation: a board on another protocol version still opens and shows in the list, but every call fails with `version-mismatch` and nothing is sent after the handshake. The keymap is read in 28-key chunks. Unsaved changes are tracked per connection (the protocol can't ask). Commands the firmware lacks a capability for fail with `not-supported` without asking the board: today settings (phase 10) and encoders (phase 9). `BoardInfo.capabilities` lists the firmware's capabilities, and `reboot(bootloader)` restarts the board, into UF2 if asked.
- `webhid.ts`, `WebHidBoardManager`: `request()` (from a click) asks for 1209:0001 with usage page 0xFF00 and usage 0x44; `start()` opens boards the site already may use; connect and disconnect events follow hotplug. The WebHID types are declared structurally, so there's no new dependency.
- `node-hid.ts`, `NodeHidBoardManager`, from the separate `@dachshund60f/host-lib/node` entry so browser builds never see node-hid: loads node-hid v3 at run time (it isn't a dependency of host-lib; the desktop shell will install it), polls the device list once a second for hotplug, matches the config interface by usage page or, on Linux hidraw, by interface number 2, and puts report ID 0 before each write.
- Round-trip tests: `kb_host_device` (in `firmware/core/tests/`) is the core protocol handler, store and settings code over a simulated flash with the firmware's default keymap, speaking hex frames on stdin and stdout. The same Board tests (default keymap, edit, save, revert, factory reset, bad input) run against the mock and against it, and a saved keymap survives its reboot. Other tests cover the client (sequence matching, serialising, retries, late answers, status mapping, disconnects), version mismatch, and both managers against stand-in WebHID and node-hid APIs. CI's host-lib job builds `kb_host_device` and runs them.

### Phase 6: persistence (2026-10-23)

- Core, `kb/store.h`: a log of whole snapshots in a flash area split into sectors. Each save appends one record (magic, sequence number, schema, length, CRC-32, data). When a sector is full the next one is erased, round robin, so wear is spread evenly and the newest record is never in the sector being erased. The magic word is written last and every write is read back, and mount checks that the space after the last record is still blank, so a save cut short by power loss is ignored and the previous snapshot stays. The flash is passed in as read, write and erase functions.
- Core, `kb/settings.h`: the blob saved in a record, schema 1. Tagged sections (keymap; settings: debounce algorithm and time, tapping term, NKRO, sleep timeout, indicator brightness, board name), each with a length, so unknown sections are skipped and missing ones keep their defaults. Adding a section or appending a field needs no schema bump; anything else bumps the schema and adds a migration in `kb_settings_decode()`. Every value is range-checked on load, and a keymap with other dimensions is ignored.
- Tests: `test_store` runs on a simulated 8 KB flash with 1 KB sectors that counts writes and catches writes to unerased cells. It covers wear levelling over many saves, power cut at every write of a save, a corrupted record, wipe, and sequence wrap. `test_settings` covers encode and decode round trips, bad values, unknown and short sections, and other keymap sizes.
- Firmware: `storage.c` puts the store on `storage_partition` (32 KB at 0xEC000, 4 KB sectors). At boot `keyboard.c` loads the saved keymap and settings, or the defaults if nothing valid is saved. The protocol's `save`, `revert` and `factory_reset` now work, and the handshake reports the `persistence` capability. Fn+N updates the NKRO setting, so a save keeps it; debounce and NKRO are applied from the loaded settings. New Kconfig defaults: `KB_SLEEP_TIMEOUT_MIN` (15) and `KB_INDICATOR_BRIGHTNESS` (13%).
- Size, debug without a screen: 85.4 KB flash, 30.5 KB RAM.

### Phase 5: config protocol v1 (2026-10-23)

- `protocol/protocol.yaml` defines version 1: the frame (4-byte header, up to 60 payload bytes in a 64-byte report), the vendor HID interface (usage page 0xFF00, usage 0x44), ten status codes, ten capability bits, and the commands: `handshake`, `reboot` (normal or into UF2), `identify` (blink the LEDs 1–30 s), `get_serial`, `get_key`, `set_key`, `get_keymap_chunk` and `set_keymap_chunk` (28 keycodes per frame; a chunk write is all or nothing), and `save`, `revert`, `factory_reset` (persistence arrives in phase 6). `protocol/README.md` documents the frame.
- `protocol/gen/gen.py` (Python 3 + PyYAML) writes `kb/protocol_gen.h`, `protocol_gen.c` and `host-lib/src/protocol.gen.ts`. CI runs it with `--check` so generated files can't drift.
- Core: `kb/protocol.h` handles one request and builds the response. Every length, index and keycode is checked before anything changes; any request but the handshake with the wrong version byte gets `version_mismatch`; error responses carry no payload. Tests: `test_protocol` (every command, every error path) and `test_protocol_fuzz` (random and mutated frames, with invariants: a response always comes back, nothing changes on an error, the keymap stays valid). A libFuzzer target (`firmware/core/fuzz/`) runs for two minutes in a new CI job with ASan and UBSan.
- `get_serial` exists because WebHID can't read the USB serial string; the firmware returns the same hex text as the USB descriptor, so the app can tell two boards apart.
- Firmware: a third HID interface (`hid_config`, 64 bytes in and out) in `kb_usb.c`; `config.c` queues requests from the USB thread and answers them on its own thread, locking the keymap against the scan thread. `reboot` waits 100 ms so the response leaves first. `identify` blinks the indicators.
- host-lib: `protocol.ts` builds request frames and parses responses (`encodeRequest`, `decodeResponse`), on top of the generated encoders and decoders. The transports and typed client come in phase 7.
- CI: the bootloader job now times out after 20 minutes (one run hung installing packages).
- Size, debug without a screen: 80.8 KB flash, 29.9 KB RAM.

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

Decisions: MX hot-swap (Kailh sockets), 2 layers, MCP73831 charger at 500 mA (2 kΩ PROG; lowered to 300 mA on 2026-10-26, see below) for a 1000–2000 mAh cell, and USB-C on a separate daughterboard.

- Schematic: one A2 sheet with 195 parts. ERC is clean (0 errors, 0 warnings). A script checked all 29 module pin nets against `board.json` (0 mismatches).
- The board has no USB-C connector. J1 is a JST-SH 4-pin for the daughterboard, carrying VBUS, D−, D+ and GND in the Unified Daughterboard order (checked on 2026-10-22; see the daughterboard section).
- Power path: VBUS feeds the MCP73831, which charges VBAT (JST-PH battery). The slide switch connects VBAT to VSYS, which feeds VDDH (high-voltage mode). VDD (REGOUT0) is +3V3. (A USB power path was added on 2026-10-23; see "PCB: USB power path".)
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

### Phase 4: full HID over USB (2026-10-23)

- Core (hardware- and transport-independent, host-tested), `kb/hid.h`:
  - Report descriptors for two HID interfaces. Keyboard: the standard boot keyboard (8-byte 6KRO report, no report ID, 5-bit lock-LED output). Extra: report ID 1 is an NKRO keyboard (modifier byte plus a 224-bit bitmap for usages 0x00–0xDF, 30 bytes with the ID), ID 2 consumer control (one 16-bit usage up to 0x3FF), ID 3 system control (one 16-bit usage up to 0xB7).
  - Packers for each report, and `kb_hid_reporter`, which turns the engine's HID state into the reports to send: only those that changed, keys on exactly one path (NKRO or boot), and on a path switch a release on the old path before the press on the new one, so no key sticks. NKRO is used only while it's enabled and the host hasn't asked for boot protocol, so a BIOS or KVM always gets the boot report. `resync` sends everything again after a resume or a new configuration. BLE and the dongle will reuse it.
  - `kb/indicators.h`: colour per LED from the lock state, active layer, battery level and suspend, for a chain described by role (layer, Caps/Num/Scroll, battery step). Defaults: layer 0 dark, Fn amber, layers 2 and 3 teal and violet, Caps warm white, brightness 32/255. Battery steps light from (k − 1)/n, red at 15% and below; the level arrives in phase 13. Phase 10 makes the colours settable.
  - New keycode `NK_TOGG` (0x7005), also in host-lib's catalogue. The default Fn layer has it on N, in both the firmware and Doghouse default keymaps.
  - Tests: `test_hid` (a small HID descriptor parser checks every report size against its packer; reporter paths, boot fallback with a key held, NKRO toggling, resync, and seven keys through the engine) and `test_indicators`. All core tests pass, also under ASan and UBSan.
- Firmware:
  - `kb_usb.c`: the second HID interface (`hid_extra` in `app.overlay`, 30-byte reports, 1 ms polling), a 16-deep queue per interface, GET_REPORT for every report, and remote wakeup in the configuration descriptor. If a queue overflows (the host stopped polling), it's purged and every report is sent again from the current state, so no release is lost.
  - Suspend: reports stop and the indicators go dark. The first key change while suspended asks the host to wake (if the host enabled remote wakeup); on resume, every report goes out again. Unplugging or a bus reset clears the suspended state.
  - `keyboard.c` drives the reporter from the scan thread: it follows the host's protocol choice, resyncs when USB asks, and handles `NK_TOGG`. `CONFIG_KB_NKRO` (default on) sets NKRO at power-up; phase 6 will store it and phase 10 lets the host set it.
  - `indicators.c` replaces the phase 2 LED walk: the walk now runs once at power-up (`CONFIG_KB_LED_SELFTEST`, 200 ms per colour, 3.6 s), then the Caps LED follows Caps Lock and the layer LED shows the active layer. LED power (the TPS22917) is switched off whenever no LED is lit. A host-lib test checks the firmware's LED chain table against `keyboard.json`.
  - Size, debug without a screen: 76.6 KB flash, 24.3 KB RAM (was 73.0 KB and 22.7 KB).
- Bring-up plan: new rows for rollover, boot fallback, media keys, and suspend and wake; the LED rows match the new behaviour.

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

### PCB: USB power path (2026-10-23)

The mainboard ran only from the battery: with the switch off it was off even on USB, and with no battery it browned out. Tiam chose Microchip's load-sharing circuit for the MCP73831 (AN1149) before ordering.

- Schematic: SW1's common pin now feeds a new net, VBAT_SW, instead of VSYS. Q1 (AO3401A P-MOSFET, SOT-23) has its drain on VBAT_SW, source on VSYS and gate on VBUS, with R3 (100 kΩ, 0603) pulling the gate to GND. D8 (B5819W Schottky, SOD-123) feeds VSYS from VBUS. On USB, Q1 is off and VSYS is about 4.7 V from D8. Unplugged, the gate falls to 0 V and Q1 connects the battery. The charger stays on VBAT, before the switch, so it charges with the switch off.
- Effects: the board runs from USB with no battery or with the switch off, and the switch is now only a battery disconnect. VDDH (and so the SAADC battery reading) shows the USB-fed VSYS while USB is in, so phase 13 reads the cell only on battery and shows charging from CHG_STAT. The SK6812s now see about 4.7 V on USB instead of at most 4.2 V. Their input-high threshold is 0.7 × VDD, about 3.3 V at 4.7 V, so a 3.3 V data line would have no margin on USB (at a full cell the threshold is 2.9 V). So a buffer was added, below.
- PCB: the three parts sit on the back between J2, J3 and SW1 (x 326–332, y 104–110), hand-routed: 0.3 mm power tracks, a 0.25 mm gate track. VBUS comes down on F.Cu from the J1 via at (331.7, 89.2), with vias at D8/R3 and at the gate. The old VSYS track from SW1 pin 2 now carries VBAT_SW to Q1's drain. 12 GND stitching vias in the way were removed, and the pours refilled.
- Checks (kicad-cli 10.0.7): ERC has the same 4 warnings as before and no errors. DRC with schematic parity: 0 errors, 0 unconnected items, and the same warnings as before (silkscreen, hole-to-hole on the stabilizers, and the stale footprint copies).
- LED data buffer (Tiam chose it over a silicon diode for D8 or testing as is): U4 (SN74LV1T34DBV, SOT-23-5), powered from LED_VDD with C19 (100 nF, 0402), sits between P0.06 (LED_DIN) and D3's DIN (new net LED_DIN_BUF). Its inputs are TTL-level (high above about 1.35 V at 4.5 V) at any supply from 1.6 V to 5.5 V, and its inputs tolerate 3.3 V while LED_VDD is switched off (Ioff). It sits on the back above D3 at (309.0, 121.2). VCC reaches the F.Cu LED_VDD track through a via, the output uses the old via down to D3, and four GND stitching vias were removed. Firmware is unchanged: the buffer is non-inverting.
- `docs/bringup-test-plan.md` has new continuity checks, a USB-only power stage and an LED check on USB.
- Saving with KiCad 10.0.7 re-wrote the zone fills and the 3D model blocks, so the PCB diff is large; only the parts above changed.
- STEP files in `hardware/step/` re-exported on Tiam's PC with KiCad 10.0.6 after the power path, LED buffer and test pads (no missing-model warnings; the encoder and J2 still use the stand-ins).
- KiCad 10.0.6 re-saves `Dachshund60F.kicad_pro` without the unused teardrop `td_enabled` flags and with `null` for empty net colours and net-class assignments, undoing what 10.0.7's pcbnew wrote. That re-save is committed; DRC and ERC are identical either way.

### PCB: test pads (2026-10-23)

- Nine test points on the mainboard's back for meter probing: TP1 VBUS, TP2 VBAT, TP3 VBAT_SW, TP4 VSYS, TP5 +3V3, TP6 LED_VDD, TP7 CHG_STAT, TP8 and TP9 GND. Each is KiCad's `TestPoint_Pad_D1.5mm`, a bare round SMD pad with no part to buy. The schematic symbols are excluded from the BOM.
- Each pad sits on its net's existing track, so no new routing was needed. The net name is on the back silkscreen, and the reference is on the fab layer. Positions are in `docs/bringup-test-plan.md`.
- To make room, 7 GND stitching vias were removed, SW1's reference moved below the switch, and the pours were refilled.
- Checks: ERC unchanged. DRC with schematic parity: 0 errors, 0 unconnected items, and the same warning counts as before the pads.

### PCB: charge current lowered to 300 mA (2026-10-26)

- The project review (2026-10-26) found the 500 mA charge current plus the keyboard's own load (about 100–130 mA worst case through D8: module and radio, LEDs at full white, a display) adds up to about 630 mA, more than a 500 mA USB port allows. Tiam chose to lower it.
- R1 (MCP73831 PROG) is now 3.3 kΩ (was 2 kΩ), so the charge current is 1000 V / 3.3 kΩ, about 300 mA, and the total stays under 500 mA. Value change only (same 0603 footprint), in the schematic and the PCB. A 1000–2000 mAh cell now takes about 4–7.5 h to charge, and the charger runs cooler (about 0.4 W at the start instead of 0.6 W).
- `docs/bringup-test-plan.md`'s charger numbers are updated to match.

### PCB: antenna keep-out under U1 (2026-10-26)

- Review finding M6: the mainboard's copy of `RF_Module:Raytac_MDBT50Q` had lost the library footprint's keep-out zones, so nothing stopped copper under the antenna (the dongle has its own rule area). Raytac's MDBT50Q spec (Ver.K, p.9–13) asks for a 12.4 × 3.8 mm "no ground pad" band on every layer at the antenna end, plus a 1.6 × 1.2 mm area on the module's own layer, with the module at the board edge.
- Two board-level rule areas, mapped from the library footprint through U1's placement (back side, 90°, at (329, 73)) and checked against pads 1 and 55. "U1 antenna keep-out" covers F.Cu and B.Cu from (333.0, 66.8) to (337.5, 79.2): the library band, carried 0.75 mm further to the right board edge. "U1 antenna keep-out (B.Cu)" covers (331.75, 73.7) to (332.95, 75.3). Both block tracks, vias, pads and copper pour. The footprints rule is off, because U1's own courtyard overlaps them; pads stay blocked, so another part there still fails DRC.
- Nothing had been routed there, but the GND pours covered about 50 mm² of the band on each layer. After the refill, no pour is left inside either area.
- Checks on Tiam's PC (KiCad 10, `kicad-cli pcb drc --schematic-parity --severity-all --refill-zones`): 0 errors, 0 unconnected items, 215 warnings, 7 excluded, the same as before.

### PCB: pre-order review fixes and fab export (2026-10-26)

- Pre-order review of the mainboard, USB-C daughterboard and dongle (KiCad 10.0.7 in the official Docker image). ERC: 0 errors on all three. DRC with schematic parity: 0 errors and 0 unconnected items on all three, plus the plate. Every footprint's pads match its library copy, once back-side parts are un-mirrored, so the "footprint differs from library" warnings are text and 3D-model fields only.
- ST6 (the right Mod's stabilizer) moved 0.038 mm left, so its right-hand holes land exactly on the 6.25u stabilizer's (ST4). They were 0.038 mm apart, a near-overlap that drills a sliver and can break a bit; an exact duplicate is merged by the fab's CAM. DRC now reports "holes co-located" there instead of "hole to hole".
- Mainboard back silkscreen: `+` and `-` beside J2's pins (VBAT on pin 1), and "Dachshund60F / mainboard v1.0 2026-10 / JLCJLCJLCJLC" in a free area under the encoder (the last marks where JLCPCB prints its order number). No new DRC warnings.
- `hardware/export_fab.py` writes Gerbers and Excellon drill files (plated and non-plated separate, with drill maps) for all four boards as one zip each.
- Assembly isn't settled yet: the mainboard's BOM has no MPN or LCSC numbers, and the daughterboard (9 mm wide) is under JLCPCB's 10 mm minimum for assembly, so it needs edge rails or a panel if JLC assembles it.

### Licence (2026-10-23)

- Code (firmware, host-lib, Doghouse, protocol, scripts): MIT, in `LICENSE`. host-lib and Doghouse carry `"license": "MIT"`.
- Hardware (`hardware/`: the three PCBs and the plate): CERN-OHL-P-2.0, in `hardware/LICENSE`.
- Third-party files keep their own licences: the vendored keyswitch footprints are CC-BY-SA-4.0 (see `hardware/Dachshund60F/lib/README.md`), and the UF2 bootloader build fetches Adafruit's MIT-licensed source.

## Tested

- host-lib `npm test`: passes locally (Node 24.12, Windows), including board and devicetree validation.
- Core C tests (core, hold, screen, version): pass in CI. Not run locally; there's no C compiler or CMake here.
- Firmware builds: debug and release for `dachshund60f/nrf52840` with no screen and with each of the four panels, and the dongle, pass in CI (NCS v3.4.1, Zephyr 4.4.2), run 39862064737. Not built locally; NCS isn't installed here.
- Bring-up plan for all three boards (inspection, power, flashing, functional tests and measurements with methods and equipment): `docs/bringup-test-plan.md` (2026-10-23).
- UF2 bootloader: builds cleanly (`-Werror`) on Linux with arm-none-eabi-gcc 13.2.1. The merged hex puts the SoftDevice at 0x1000–0x25DE8, the bootloader at 0xF4000–0xFC3AC, and the UICR words at 0x10001014/18. Not yet run on a chip.
- Core C tests for phase 3 (debounce, engine, keycodes, keymap): pass locally on Linux with GCC, also under ASan and UBSan.
- Phase 4: all core tests (including `test_hid` and `test_indicators`) pass on Linux with GCC, also under ASan and UBSan. The firmware builds locally (NCS v3.4.1, Zephyr SDK 1.0.1) in debug, and in release with the 128×32 OLED, with no new warnings. host-lib (57 tests) and Doghouse (check, 14 tests, build) pass.
- Phase 5: core tests (12, including `test_protocol` and `test_protocol_fuzz`) pass locally with GCC under ASan and UBSan; the libFuzzer target runs only in CI (the local clang has no compiler-rt). The firmware builds locally with no new warnings. host-lib (62 tests) passes.
- Phase 6: core tests (14, including `test_store` and `test_settings`) pass locally with GCC under ASan and UBSan. The firmware builds locally with no new warnings.
- Phase 7: host-lib 90 tests pass locally (Node 24), including the round trip against `kb_host_device`. Doghouse still checks, tests and builds.
- Phase 8: Doghouse checks, tests (14) and builds. Checked by hand in headless Chromium against the host-built firmware through a stand-in `navigator.hid`: the board appears with its keymap, a key change shows as unsaved, Save works and survives a page reload, Identify works, and the settings and About tabs show what the firmware supports. A stand-in board on protocol v2 shows the update message. Demo mode and the no-WebHID message checked the same way. host-lib 91 tests pass.
- Phase 9c: host-lib 104 tests pass with `KB_HOST_DEVICE` (the advanced-key and macro checks run against both the mock and the host-built firmware and agree). Doghouse checks, tests (14) and builds. Checked by hand in headless Chromium in demo mode: adding a combo, a tap dance and a macro works, unsaved changes show, and the browser console is clean.
- Phase 14: core (20), host-lib (131) and Doghouse (check, 14 tests, build) pass locally. The keyboard firmware (debug and release) and the dongle firmware (debug and release) build locally with no new warnings. The Connection tab's dongle card checked by eye in headless Chromium in demo mode.
- Real hardware: nothing yet. Phase 9 is to be checked on a board: a combo, a tap dance, a key override, a Leader sequence and a macro each typed for real, the encoder turning volume, and mouse keys moving the pointer.
- Phase 8 is to be checked on a board: Connect a board… in Chrome and Edge on Windows, the board coming back after unplugging and replugging, a key change typed for real after saving and power-cycling, and Restart into bootloader mounting the UF2 drive.
- Phase 7 is to be checked on a board: WebHID in Chrome or Edge finds the board after picking it once and follows unplugging; node-hid finds it on Windows (by usage page) and on Linux (by interface number, with a udev rule for hidraw access); the handshake, keymap read and a key change work over both.
- Phase 6 is to be checked on a board: a saved keymap survives unplugging, revert and factory reset work, and a save cut short by unplugging leaves the previous settings.
- Phase 5 is to be checked on a board: `lsusb -v` shows the vendor interface, and the handshake, `get_serial` and `identify` work once host-lib can talk to it (phase 7).
- Phase 4 is to be checked on a board:
  - `lsusb -v`: two HID interfaces, the first with the boot keyboard subclass, the second with report IDs 1–3; both endpoints at bInterval 1; remote wakeup set in bmAttributes
  - seven or more keys held at once all reach the host; Fn+N switches to 6KRO and back
  - in a BIOS setup screen or through a KVM, keys work (boot protocol), and NKRO comes back in the OS
  - media, volume and brightness keys on the Fn layer work
  - the Caps LED follows Caps Lock, the layer LED turns amber on Fn, and LED_VDD drops to 0 V when every LED is dark
  - sleep the host, press a key: the host wakes, and nothing sticks afterwards
- Phase 3 is to be checked on a board:
  - every key types what the README matrix table says, with no ghosting when three keys in a rectangle are held
  - Fn layer: media keys, Insert, Print Screen, Page Up/Down, Home/End; the status screen shows the layer
  - Fn+Esc mounts the UF2 drive at once; holding Esc for 3 s still does
  - scan rate is 1 kHz and a scan takes well under 1 ms (toggle a spare GPIO and use a logic analyser)
  - no chatter with the default debounce (eager press, 5 ms deferred release); try other algorithms through Kconfig if needed
- Phase 2 is still to be checked on a board:
  - flash `dachshund60f_bootloader_s140_6.1.1.hex` over SWD with a chip erase; check the DACHSHUND drive appears and `INFO_UF2.TXT` says `nRF52840-Dachshund60F-v1`; then copy `zephyr.uf2`
  - check that it enumerates as 1209:0001 with a unique serial number (USB Device Tree Viewer or `lsusb -v`)
  - check that it works as a boot keyboard in the BIOS
  - check that the LEDs walk once in chain order with correct colours at power-up
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

- Registered pid.codes PID before distributing boards; the licence is now set (MIT and CERN-OHL-P), so this needs only a public repo
- PCB design: the pin map is proposed and must be confirmed (or edited) once the schematic is routed
- Case (sets the gasket pockets from the plate's sock outlines, the roller support and the daughterboard position). The mainboard and plate have no mounting holes. The daughterboard is designed for M3; if the case uses M2, swap H1/H2 to `MountingHole_2.2mm_M2_Pad_TopBottom` for a snug fit.
- When to add the Tauri shell (needs Rust and the MSVC build tools); WebHID stays possible for a hosted version
- Display panel: OLED (which size) or e-paper; also its orientation in the case, since the overlays assume the panel's default rotation
- Dongle case: it sets the M2 hole positions, the light pipe over D1 and the button caps. Use plastic, with nothing metal over the antenna strip.
- Dongle bootloader: reuse Adafruit's `pca10059` build or make a dongle-specific one. The DFU button matches (P1.06), but that build's LEDs are P0.06 and P0.12, so only the blue LED would light on this dongle. A dongle board can reuse the files in `boards/Dachshund60F/bootloader/` as a template. Try the existing build on the first board.

## Known bugs

- The 2.4 GHz link isn't encrypted: keystrokes over the dongle can be read by anyone nearby with an nRF52 and the address. Encryption is a scheduled task.
- Encoder part number checked against Bourns' PEC12R datasheet (2026-10-23): the BOM's PEC12R-4220F-S0024 is right. The first digit of the ordering code is the mounting: 2 = vertical (radial pins, shaft up from the board), 3 = horizontal with a threaded bushing, 4 = horizontal without a bushing. So -4220F-S0024 is horizontal, no bushing, 24 detents, 20 mm shaft, push switch, 24 pulses, and Mouser lists it as right angle. Don't order PEC12R-2217F-S0024: it is the vertical part. KiCad's footprint name `Horizontal_PEC12R-2x17F` uses the wrong series digit; its geometry is the horizontal part's. Still open: SW3's 3D model is a stand-in (see `hardware/3dmodels/README.md`), and the footprint's hole pattern hasn't been checked dimension by dimension against the 4xxx drawing.
- Release builds print Kconfig warnings, because `prj.conf` sets logging options that `release.conf` turns off. PRINTK also stays on in release, since something selects it; nothing prints, because no console or RTT backend exists. This is harmless, but the warnings are noise.
- E-paper builds warn that Zephyr's MIPI-DBI driver is "experimental". It is what Zephyr's own e-paper shields use; this is a notice, not a fault.
- Doghouse's palette has no layer-tap, mod-tap, one-shot mod, default-layer or modded keys yet, though the firmware supports them. Add them with the phase 9 editors.
- The matrix is scanned at 1 kHz all the time, which costs battery. Phase 13 adds idle scanning with GPIO sense wake-up.
- In boot protocol (BIOS, KVM) or with NKRO off, a seventh held key is dropped from the 6-key report rather than sending the ErrorRollOver code.
- A host that reads the boot report without ever sending SET_PROTOCOL (a few BIOSes and KVMs) sees no keys while NKRO is on, because keys then go to the NKRO interface. Fn+N switches to 6KRO for such hosts.
- Doghouse doesn't yet show key labels in the user's OS keyboard layout (AZERTY and so on), edit the status screen layout, or update firmware. The SPEC lists these for later phases.
- An OLED left on all the time can burn in. It gets an idle timeout with power management in phase 13.
