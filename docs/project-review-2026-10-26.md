# Dachshund60F project review

> **Follow-up (2026-10-26).** This review was run on a phase-3 checkout. Each finding was re-checked against main (phase 15) before acting on it. H1: R1 is 3.3 kΩ now (about 300 mA charging). H2, M4 and M5 are fixed in the firmware, and H3, M1 and M2 in Doghouse. M3 had already been fixed in phase 4. M6: the mainboard now has keep-out rule areas under U1's antenna, sized to Raytac's spec. M7: the bring-up plan no longer plugs in USB with a bench supply on J2. The other follow-ups are open tasks in `SCHEDULED_TASKS.md`. The details are in `PROGRESS.md`. The "planned features" section below is out of date: the protocol, persistence, real transports, BLE, power management and the dongle have all been built since.

Date: 2026-10-26  
Reviewed revision: `34931f32098ace86cd1515fe928e87731812e785`  
Scope: firmware/core, Zephyr integration, board and bootloader definitions, dongle scaffold, host library, Doghouse app, CI, schematics, PCB projects, plate generator, mechanical models, and bring-up instructions.

This is a review, not a repair. No existing project file was changed. Commands that install development dependencies or emit software build products ran against temporary copies; hardware reports and reproduction programs were also written outside the repository. No board was flashed, zone fill saved, STEP export regenerated, commit created, or hardware modified.

## Executive assessment

The project has a useful wired-first foundation: hardware-independent C logic, meaningful host tests, cross-checks between board definitions and firmware, a functioning mock configurator, and routed PCB designs with no active DRC errors or unconnected items under their current rules and exclusions.

It is not yet a complete tri-mode keyboard or a hardware-validated design. Current code implements the phase-3 wired matrix/keymap path; protocol, persistence, real host transports, BLE, power management, and radio relay remain scheduled work. Those omissions are not classified as defects in this review.

The most important issues are:

1. A fixed 500 mA charger leaves no USB current allowance for the operating keyboard and has no enumeration/suspend-aware input limiting.
2. A failed final USB release report is discarded without retry, which can leave a key logically held on the host.
3. Concurrent app operations and an unawaited revert refresh can lose intended edits or corrupt undo behavior.
4. Implemented layer and one-shot behavior has reproducible edge-case defects despite the existing suites passing.
5. Mainboard antenna clearance is not enforced by an actual KiCad rule area in the reviewed file; the dongle does have one.

Recommendation: address the high-priority electrical/transport/data-integrity issues before treating this revision as a release candidate. Prototype fabrication is a separate engineering decision; passing DRC is not electrical, RF, battery-safety, or mechanical sign-off.

## Severity and evidence

- **High:** plausible stuck input, lost configuration, unsafe test procedure, or exceeding a power-source budget in ordinary supported use.
- **Medium:** incorrect implemented behavior, missing lifecycle synchronization, or a significant design safeguard/validation gap.
- **Low / advisory:** maintenance or release-readiness work without an established immediate functional failure.
- **Reproduced:** executed against the actual core or app/session implementation in a temporary harness.
- **Source-confirmed:** a concrete code path or electrical connection establishes the issue, but it has not been observed on physical hardware.
- **Validation risk:** requires measurements, manufacturer information, or a further scheduling/circuit analysis; not asserted to be a demonstrated hardware failure.

## Checks performed

### Software

| Check | Result | Qualification |
|---|---|---|
| Eight C core test executables | All passed | Compiled directly with installed GCC using C11, `-Wall -Wextra -Wpedantic -Werror`; CMake/CTest was not available in the checked locations. |
| Additional layer/one-shot reproductions | Both issues reproduced | `TO` retained a held higher layer; an intervening bare modifier consumed one-shot Shift. |
| Host library: `npm ci --ignore-scripts`, then `npm test` | 56 tests passed | Temporary project copy; includes TypeScript compilation and board/bootloader/default-keymap consistency tests. |
| App: `npm ci --ignore-scripts` | Completed | Temporary copy. Both npm installs reported zero known vulnerabilities at execution time; this is not a comprehensive security audit. |
| App: `npm run check -- --fail-on-warnings` | Zero errors, zero warnings | Svelte/TypeScript check. |
| App: `npm test` | 14 tests passed | Existing geometry, history, and keymap helper tests. |
| App: `npm run build` | Passed | 150 modules transformed. |
| Additional session reproductions | Three issues reproduced | Compiled the actual Svelte state module with Svelte's compiler in a temporary copy; exercised concurrent edits, failed undo, and immediate re-edit after revert. No browser rendering asserted. |
| Zephyr keyboard/dongle and Adafruit bootloader builds | Not run in this review | No configured NCS/west/ARM build environment established. The workflow was inspected, but past CI success was not independently fetched. |

Tools used included Node 24.14.0 and KiCad 10.0.6. The repository records some earlier board checks with KiCad 10.0.7; library/version differences can affect warning counts.

### KiCad

Fresh checks used `kicad-cli pcb drc --schematic-parity --severity-all --format json`, schematic ERC with all severities, schematic XML netlist exports, and read-only `pcbnew` loading. Checked all four boards and all three schematics. Reports were written to `%TEMP%`, not committed.

| Board | Active DRC errors | Unconnected items | Active DRC warnings | Excluded error-level entries | Schematic parity warnings | ERC errors / warnings |
|---|---:|---:|---:|---:|---:|---|
| Mainboard | 0 | 0 | 215 | 7 | 199 | 0 / 1 |
| USB daughterboard | 0 | 0 | 2 | 0 | 0 | 0 / 0 |
| Dongle | 0 | 0 | 2 | 0 | 0 | 0 / 4 |
| Plate | 0 | 0 | 0 | 0 | 0 | Not applicable |

Mainboard DRC warning breakdown: 197 library-footprint mismatches, 8 silkscreen-over-copper, 6 silkscreen-overlap, 2 silkscreen-edge-clearance, and 2 hole-to-hole. Parity warnings are field mismatches, not reported missing electrical connections. Mainboard ERC warns that U3's embedded TPS22917 symbol differs from the installed library; dongle ERC similarly reports four library-symbol mismatches.

These are checks of the committed board geometry and saved fills under the configured rules. No fresh zone refill was saved. DRC does not establish USB impedance, RF performance, regulator/charger suitability, battery safety, or actual switch/case fit.

## PCB exclusions and intentional overlaps

**The seven excluded error-level entries are accepted alternate-layout geometry, not seven unresolved PCB errors.** The exclusions are present in `hardware/Dachshund60F/Dachshund60F.kicad_pro:73-103`, and every corresponding entry in the fresh DRC JSON has `excluded: true`.

| Excluded check | Count | Objects | Review disposition |
|---|---:|---|---|
| Courtyards overlap | 1 | SW615 and SW606 | Intentional split-space/6.25u alternate switch positions. |
| PTH inside courtyard | 2 | SW606 pad 1 inside SW615's courtyard, and vice versa | Same mutually exclusive layout geometry. |
| NPTH inside courtyard | 4 | ST5 holes inside SW606; ST4 holes inside SW604 | Alternate stabilizer/switch population geometry. |

This agrees with `PROGRESS.md:155-171`, the plate notes in `hardware/Dachshund60F-Plate/README.md:27-31`, and the user's clarification. SW604, SW606, and SW615 use soldered footprints rather than overlapping hot-swap sockets. Both layout variants also appear in the mechanical model; a 3D collision between mutually exclusive parts is not, by itself, an assembly defect.

The two **active warning-level** hole-to-hole entries are a separate known consequence of the layouts: ST4/ST6 stabilizer hole centres are only 0.038 mm apart, at x = 200.969/201.007 mm, y = 142.553 and 157.763 mm. Do not describe them as accidental footprint placement. Obtain the fabricator's acceptance of how the overlapping NPTH drill geometry is manufactured, and validate each intended stabilizer population. The exclusion acceptance does not imply both alternate layouts should be populated simultaneously.

Custom rules were also considered, not ignored: `hardware/Dachshund60F/Dachshund60F.kicad_dru:9-18` sets 0.3 mm edge clearance specifically for the six reverse-mount LED cutouts and permits SW1's intentional edge-mount tabs at the board edge. These scoped exceptions are not general clearance failures. The nominal LED body/cutout fit still deserves a real-part check.

## Findings

### H1 — High: USB input-current budget is exceeded by charger plus system load

**Evidence:** source-confirmed circuit/budget issue; current draw not measured.

References: `hardware/Dachshund60F/Dachshund60F.kicad_sch:12730-12820` (MCP73831 and 2 kΩ PROG resistor); `firmware/Kconfig:26-29` (500 mA USB declaration); `firmware/src/kb_usb.c:37-40`; `docs/bringup-test-plan.md:185-193`.

The exported netlist puts MCP73831 VDD directly on VBUS, its PROG pin through R1 = 2 kΩ to ground, and its output on VBAT. This sets nominal fast charging to 500 mA. The same VBUS powers VSYS through D8, so the operating module, indicators, and optional display draw additional input current. A 500 mA configuration descriptor does not make 500 mA charging plus system current fit within a 500 mA source budget.

The resistor-only charger arrangement also does not wait for USB configuration, detect Type-C advertised current, or reduce charging on USB suspend. CC pull-downs identify a sink; they do not themselves grant an arbitrary current budget. The 750 mA-hold PTC is fault protection, not a negotiated-current limiter.

**Effect:** legacy/default-current hosts can see excess draw, voltage droop, charging interruptions, or a disabled port. The problem exists even if the circuit works reliably on a generous wall charger.

**Recommendation:** specify supported USB source conditions and implement an input-current policy that reserves current for the system, including pre-configuration and suspend behavior. Options include suitably reduced charging or a charger/power-path solution with controlled input limiting; choose against measured worst-case load. Verify charge current, total VBUS current, cable/PTC voltage drop, and charger temperature with a depleted protected cell. Do not rely on thermal regulation to meet the USB budget.

### H2 — High: failed USB release reports are discarded without retry

**Evidence:** source-confirmed; USB stack fault injection and on-device reproduction remain needed.

References: `firmware/src/kb_usb.c:59-82`; `firmware/src/keyboard.c:65-72`; `firmware/core/src/engine.c:119-125`.

`kbd_submit_next()` dequeues a report before submission. If `hid_device_submit_report()` returns an error, that report is logged as dropped and not retained. The engine has already copied its HID state into `reported_hid`; the report callback ignores the transport result.

**Sequence:** deliver A-down, then fail submission of the final all-up report. With no further HID state transition there is nothing to retry, so the host can retain A-down indefinitely. This is not merely the planned lack of advanced HID features.

**Recommendation:** retain latest desired state and retry failed transfers from a scheduled transport owner, preserving press/release ordering where needed. Test failure of the final release, recovery after a transient endpoint error, and disconnect/reconnect behavior. Also define what GET_REPORT represents: `kbd_last_report` currently changes before submission success.

### H3 — High: overlapping app operations corrupt edit ordering and undo history

**Evidence:** reproduced with the actual compiled session module and delayed mock board.

References: `app/src/lib/app-state.svelte.ts:122-153,176-185`; `app/src/lib/components/KeymapEditor.svelte:62-79,120-124,140-143`.

`run()` sets `busy` but does not serialize or reject concurrent calls. `setKey()` reads its `before` value before awaiting the write. Several controls and shortcuts bypass the palette's busy protection.

**Executed sequence:** concurrently assign `KC_A`, then `KC_B`, to default Esc. After both finish, undo once. Expected `KC_A`; actual `KC_ESC`. Both history entries captured the original value. The first completed operation can also clear `busy` while another is still outstanding.

**Recommendation:** serialize all session mutations, including key/encoder edits, undo/redo, save, revert, and reset. Capture the old value inside that serialization boundary. Test two overlapping edits and overlapping edit/save/reset operations with nonzero and variable transport latency.

### M1 — Medium: revert/reset completes before refreshed session data is usable

**Evidence:** revert data-loss sequence reproduced; reset shares the same event/reload path.

References: `app/src/lib/app-state.svelte.ts:76-77,96-107,202-218`.

The `reverted` event starts `void this.load()`. `revert()` and `factoryReset()` clear history, notify success, and release busy without waiting for that reload.

**Executed sequence:** change Esc to `KC_A`; await revert; immediately assign `KC_A` again. Cached session state still says `KC_A`, so `setKey()` treats the requested assignment as a no-op. The actual board remains `KC_ESC`: the user's intended reassignment was silently lost.

The fire-and-forget reload also lacks a rejection handler. A disconnect while its reads are pending can produce an unhandled rejection. An independent delayed-mock reproduction observed `disconnected` from that path.

**Recommendation:** await a coordinated refresh before announcing completion or accepting edits; invalidate obsolete loads and catch expected disconnection failures. Cover immediate re-edit after revert/reset and disconnect during refresh.

### M2 — Medium: failed undo/redo moves history despite a failed board write

**Evidence:** reproduced with an injected write failure.

References: `app/src/lib/app-state.svelte.ts:176-185`; `app/src/lib/history.ts:35-45`.

Undo/redo changes the history stacks before applying the board write. The shared error handler reports failure but does not restore the stacks.

**Executed sequence:** successfully assign `KC_A`, then make the next write reject. Undo leaves the key at `KC_A`, but reports `canUndo=false`, `canRedo=true`. The app has recorded a reversal that never happened.

**Recommendation:** commit stack movement after successful application, or restore the original stack state on failure. Test both undo and redo failures, including future disconnected/version-mismatch responses.

### M3 — Medium: USB readiness does not synchronize already-held keys

**Evidence:** source-confirmed; not exercised on hardware.

References: `firmware/src/kb_usb.c:94-104,181-185`; `firmware/src/keyboard.c:65-72`; `firmware/core/src/engine.c:119-125`.

Scanning starts before USB configuration. A key pressed while the interface is unavailable produces `-EAGAIN`, which is ignored, while the engine marks the HID state reported. When `kbd_iface_ready(true)` runs, it does not send a current-state snapshot. A held key therefore remains invisible until another state change triggers a report. Similar behavior applies after reconfiguration.

**Recommendation:** retain current desired state while unavailable and synchronize it on readiness. Do not replay historical disconnected taps. Add tests for configure-while-held, release-while-disconnected, and reconnect-while-held.

### M4 — Medium: `TO(layer)` does not turn off held higher layers

**Evidence:** reproduced in a C host program linked to the actual core.

References: `firmware/core/include/kb/keycodes.h:64-68`; `firmware/core/src/engine.c:79-90,182-184`.

The documented TO contract turns other layers off, but the implementation changes only `layers_on`. The layer mask separately includes `layer_holds` and one-shot layer state.

**Executed sequence:** hold `MO(2)`, press `TO(1)` through a transparent position, then press a key mapped to B on layer 1 and C on layer 2. Actual output is C, not B.

**Recommendation:** define TO's interaction with held MO/LT/OSL sources and implement that contract without underflowing or misattributing subsequent releases. Extend the existing TO test beyond toggled layers.

### M5 — Medium: a bare modifier consumes pending one-shot modifiers

**Evidence:** reproduced in the same C host harness; intended semantics should be made explicit.

References: `firmware/core/src/engine.c:138-158,271-273`.

All BASIC keycodes consume pending one-shot modifiers, including bare Ctrl/Alt/Shift usages. This conflicts with the helper's description of consumption by a key that types something.

**Executed sequence:** tap `OSM(MOD_LSFT)`, tap ordinary `KC_LCTL`, then press A. Actual output has no Shift modifier. One-shot Shift was attached to Ctrl and released before A.

**Recommendation:** preserve pending one-shot modifiers across bare modifiers, or explicitly choose and document another behavior. Add tests for intervening bare modifiers and mod-tap holds.

### M6 — Medium: mainboard antenna keep-out is not enforced as a KiCad rule area

**Evidence:** read-only PCB structure inspection; this is a missing safeguard, not a claim of demonstrated RF failure.

References: `hardware/Dachshund60F/Dachshund60F.kicad_pcb:83054-83090,112039,121722`; compare `hardware/Dachshund60F-Dongle/Dachshund60F-Dongle.kicad_pcb:8751-8772`.

The mainboard contains footprint text describing a keep-out, but read-only `pcbnew` inspection found only its two GND pour zones and no board- or footprint-level rule areas. The custom `.kicad_dru` contains LED and switch edge exceptions, not an antenna restriction. Consequently, its clean DRC result cannot certify antenna clearance or prevent future routing/fills there.

By contrast, the dongle contains an explicit two-layer antenna rule area that forbids tracks, vias, pads, and zone fills, extending across the antenna-side board strip.

**Recommendation:** encode the manufacturer's required mainboard clearance as an actual rule area, then inspect/refill/check before manufacturing. Separately verify existing copper and the final case/display/roller/battery envelope against Raytac's placement guidance. No trace reroute is justified solely by the absence of a rule area.

### M7 — Medium: bring-up instructions allow charger backfeed into a non-sinking bench supply

**Evidence:** source-confirmed instruction; equipment-dependent safety risk.

Reference: `docs/bringup-test-plan.md:174-180`.

The plan connects a normal bench supply to the battery input, then adds USB and describes current flowing back into the supply as expected. Whether that is safe depends on the supply's explicit reverse-current/sinking rating. A generic current-limited source is not a battery simulator, and its source current limit does not guarantee protection against reverse energy.

**Recommendation:** do not perform this step with an ordinary unqualified supply. Use a protected real cell after earlier checks, a rated four-quadrant battery simulator, or a documented reverse-current-safe fixture. Define the fixture before the test rather than relying on unplugging after its output rises.

## Additional risks and follow-up checks

These are not newly demonstrated PCB faults or reasons to remove intentional exclusions.

### USB transport overload and lifecycle

- `firmware/src/kb_usb.c:187-193` drops the oldest reports when the 16-entry queue fills. A whole-state report helps eventual state convergence but cannot preserve a tap if both its down/up transitions are discarded. Define the loss/backpressure contract and test slow/stalled hosts and tap-hold bursts. Current return value reports success even when older transitions were lost.
- `firmware/src/kb_usb.c:94-101` clears `kbd_busy` during interface teardown while the transmit path and completion callback also manipulate it. Buffer ownership across shutdown/reconfiguration should be tested with deliberate interleavings before claiming a concrete race under Zephyr's actual callback context. One worker owning submit/completion/lifecycle would simplify the contract.
- SET_IDLE stores the requested duration (`firmware/src/kb_usb.c:138-149`), but this module has no periodic idle-report resend. Verify what the pinned Zephyr class handles, then add a compliance test or application timer as required. Do not assume storing the value alone implements idle behavior.
- No GPIO/USB integration tests establish real scan timing, column settling, row release, BIOS/KVM boot behavior, or endpoint recovery. These remain hardware bring-up gates, distinct from passing core tests.

### Power, charging, indicators, and display

- Netlist and PCB pads agree on the new power path: Q1 drain = VBAT_SW, source = VSYS, gate = VBUS; D8 anode = VBUS and cathode = VSYS. This is not a reversed-MOSFET finding.
- U4 is **SN74LV1T34DBV**, not an assumed AHCT buffer. Its PCB pads and schematic agree: A = LED_DIN, Y = LED_DIN_BUF, VCC = LED_VDD, GND = GND. The switched-supply level translator addresses the documented USB-fed LED input-level concern; validate rise/fall edges and power-off leakage on a real board.
- DCCH is unconnected; preserve LDO-mode REG0 operation in both app and bootloader. The early mainboard initialization sets default REGOUT0 to 3.3 V. Verify cold battery boot and USB/battery transitions, including low-cell dropout.
- J4 uses the module's VDD/REGOUT0 output. Check the actual external-load allowance against the selected display and all other 3.3 V loads; a generic OLED/e-paper family name is not a worst-case current specification.
- The firmware still runs a low-brightness LED bring-up walk, not production layer/battery/lock indication (`firmware/src/led_bringup.c:28-55`). Battery voltage measurement, charging state interpretation, suspend current, and sleep behavior are future phase work.
- The bring-up plan correctly calls for a protected LiPo and polarity verification. Keep that requirement: this review does not establish on-board cell protection or safe use of an arbitrary unprotected cell.

### Mechanical and fabrication

- Plate generation reads switch/stabilizer/header/LED positions from the mainboard rather than maintaining a second manual placement map. Its fresh DRC is clean. Both spacebar layouts and their shared soldered-switch slot are intentional.
- Validate real MX clip engagement at 1.6 mm, the merged-slot support, stabilizer travel, gasket retention, J4 clearance, and LED windows. The mainboard has no mounting holes by design; support comes from the switches/plate/case, so absence of mounting holes is not itself a defect.
- `hardware/3dmodels/README.md:6-29` explicitly identifies approximate connector features and assumed encoder body/axis dimensions. Those models are useful for layout, not final tolerance approval. Check actual manufacturer drawings/parts before locking the case or roller geometry.
- STEP freshness documentation is inconsistent with observable content: `PROGRESS.md:288` says the exports predate the power-path change, but the committed mainboard STEP contains Q1, D8, and U4 assembly occurrences. Therefore this review does **not** assert those parts are missing. Verify a reproducible export against the intended revision rather than treating either the note or component-name presence as full geometric proof. No export was run here.
- Review footprint/symbol mismatch warnings selectively. Embedded footprints can intentionally differ because of model, text, or project-specific geometry; do not bulk-update them merely to suppress warnings and risk undoing custom cutouts or placement work.
- The daughterboard and dongle each retain two stock USB-connector silkscreen/edge warnings. Review the fab preview, but do not classify the intentional connector overhang as a copper-routing defect.
- The dongle notes a ground-reference interruption beneath part of the USB pair (`hardware/Dachshund60F-Dongle/README.md:51-52`). Validate enumeration and signal quality with both cable orientations and realistic cables. No signal-integrity failure was demonstrated here.

## Architecture, implementation status, and release readiness

### What is working well

- The core separates matrix/keymap/hold/debounce behavior from Zephyr and transport code. Eight executable suites cover substantial logic, boundary validation, and timestamp wraparound.
- Host tests cross-check board JSON, devicetree pins, bootloader identifiers and flash layout, display-header mapping, and default keymaps. All 56 passed in this review.
- Mock edits, persistence simulation, settings validation, and disconnection behavior provide a productive app-development boundary. Mock calls check connectivity after simulated latency.
- Explicit per-board USB test identifiers, reserved pins, SWD recovery, a physical reset, and hold-Esc recovery are documented. The bootloader build is pinned to a commit.
- CI covers core tests, host tests, app check/test/build, all display variants, and debug/release firmware plus bootloader builds (`.github/workflows/ci.yml:16-163`). Release configurations disable logs; typed-event logging was not found in the inspected scan/report path.
- USB daughterboard protection, CC pull-downs, consistent 1:1 connector net order, module decoupling, and mainboard test access make staged bring-up practical.

### Planned features, not current defects

`protocol/README.md:1-14` is a placeholder for phase 5; `protocol.yaml`, generated bindings, a parser, vendor HID, and fuzz tests do not yet exist in the tracked inventory. Host-lib exports the mock API, not real hidapi/WebHID transports. The app intentionally creates mock boards. The dongle app only logs and sleeps (`dongle/src/main.c:1-23`).

Full USB NKRO/consumer/system delivery and suspend behavior, flash persistence/migration, wireless, encoder runtime mapping, and real configuration/version negotiation remain scheduled. Existing UI controls and mock settings must not be presented as capabilities of the physical phase-3 firmware. In particular, mock protocol version 1 is not evidence of a working wire protocol or incompatible-device write protection.

The PCA10059 dongle target is an intentional development placeholder. A production custom-board definition, clock/power configuration, bootloader layout, and real relay enumeration still need their phase-14 implementation and verification.

### Test and automation priorities

1. Add transport fault-injection tests for failed final releases, readiness snapshots, queue overload, idle handling, and teardown/completion ordering.
2. Add app-session integration tests, not just pure-history tests: serialize mutations; reject/fail undo; await revert/reset; cancel stale reloads; unplug during pending reads.
3. Add the reproduced TO/held-layer and one-shot/intervening-modifier cases to the core suite.
4. Add automated KiCad ERC/DRC/parity checks with a controlled library version. Preserve and review exclusions explicitly; report active versus excluded findings separately. A blanket failure on raw error severity would incorrectly reject the intentional spacebar geometry.
5. Make generated plate/STEP freshness verifiable without overwriting the source tree, and keep a documented fabrication revision/BOM/export set.
6. Execute the staged hardware plan after correcting the power-budget/test-fixture concerns: rails first, boot/recovery, USB, matrix, peripherals, then power/latency/RF measurements. Record actual board serials and results rather than inferring hardware success from CI.

The existing CI has no KiCad verification or session/USB integration test job. It builds firmware but cannot by itself exercise electrical or endpoint timing behavior.

## Suggested disposition

| Before next milestone | Recommended action |
|---|---|
| Before unrestricted USB/charging use | Resolve H1's input-current policy; qualify the power-test fixture in M7. |
| Before reliable daily wired typing | Resolve H2/M3 and define report-queue recovery/overload behavior; run real USB/BIOS/KVM tests. |
| Before trusting configurator edits | Resolve H3/M1/M2 and add asynchronous session tests. |
| Before advertising implemented advanced keys | Resolve M4/M5 and document their layer/one-shot contracts. |
| Before a manufacturing/case freeze | Enforce/check mainboard RF clearance, review fab handling of intentional NPTH overlap, verify real-part mechanics, and tie exports to a revision. |
| Before distributing boards | Replace pid.codes test PIDs with registered identifiers; finish applicable firmware phases and hardware validation. |

## Limits of this review

This was static inspection plus host-side execution and fresh KiCad checks, not an exhaustive formal audit. No physical boards, scope traces, USB captures, battery measurements, RF tests, fab CAM output, selected display datasheets, or manufacturer-certified mechanical models were available as verified evidence. No claim is made that all electrical constraints or every protocol/state-machine path have been proven safe.

The actionable evidence is the executed test/check results, specific reproduced sequences, actual net/pad connections, and the distinction between active DRC findings and accepted exclusions. Existing project files, including `PROGRESS.md` and `SCHEDULED_TASKS.md`, were deliberately left untouched as requested.
