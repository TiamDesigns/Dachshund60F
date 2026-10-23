# Dachshund60F bring-up and test plan

As of 2026-10-23. The editable copy, with the results log, is the [Claude Doc](https://claude.ai/artifact/4cqRPnLR6nG84q44rr7kXN); this file is a snapshot of it.

## Scope and order

Bring up the three boards in five stages, and don't move a board to the next stage until it passes the current one: inspect, power, flash, function, then measure. Each stage catches faults cheaply before the next one can make them expensive (a short found with an ohmmeter costs nothing; the same short found with USB plugged in can cost a module).

The boards, as designed on main (2026-10-23):

| Board | KiCad project | What it carries | Test access |
| --- | --- | --- | --- |
| Mainboard | `hardware/Dachshund60F/` | MDBT50Q-1MV2, MCP73831 charger (500 mA), JST-PH battery J2, slide switch, TPS22917 LED switch, 6× SK6812MINI-E, PEC12R encoder, 80 switch positions, display header J4, SWD J3 | Test pads TP1–TP9 on the back (see below), plus the decoupling-cap pads, J3, J4 and J1 |
| USB-C daughterboard | `hardware/Dachshund60F-USB/` | HRO USB-C, 5.1 kΩ Rd on CC1/CC2, USBLC6-2SC6, 750 mA PTC, FB1 shield ferrite, JST-SH J2 | Connector pins and part pads |
| Dongle | `hardware/Dachshund60F-Dongle/` | MDBT50Q-1MV2, HRO USB-C, USBLC6-2SC6, PTC, PAIR and RESET buttons, RGB LED, SWD J2 | No test points. Probe C1–C4 and J2 |

Recommended order:

1. Daughterboard first. It is passive and quick to check, and the mainboard needs it for USB.
2. Dongle second. It runs from USB alone, so it is the easiest board to get flashing over SWD. It proves the probe, the tools and the module before the mainboard.
3. Mainboard last, powered from a current-limited bench supply on the battery connector before it ever sees a real cell or USB.

Build at least two of each board if the fab's minimum allows it. A second board tells a design fault (both fail the same way) from an assembly fault (one fails). Give each board a serial (for example MB-01, DB-01, DG-01) and log every result against it in the results table at the end.

Pass limits are given in each table. Where a limit comes from a datasheet it is rounded and should be checked against the part's sheet when you test; where it is a design target from SPEC.md or the dongle README it says so.

## Equipment

The first six rows are enough to bring every board up. The rest unlock the measurements in Stage 4; each test says which it needs.

| Item | Used for | Notes |
| --- | --- | --- |
| Multimeter with a µA range and continuity beep | Shorts, rails, charge voltage, resistances | Any decent DMM; 4½ digits helps for the 4.20 V charge check |
| Bench supply, 0–6 V, adjustable current limit | First power-up of every board; battery emulation | Set the limit to 50 mA for first power, then raise it |
| SWD probe | Flashing the bootloader and RTT logs | J-Link (EDU Mini is fine), an nRF52840 DK used as a probe through its Debug Out header, or a CMSIS-DAP probe such as the Raspberry Pi Debug Probe with pyOCD |
| 1.27 mm 1×5 cable or pogo clip for J3 (mainboard) and J2 (dongle) | Connecting the probe | Pin order on both: 1 VDD, 2 SWDIO, 3 SWDCLK, 4 nRESET, 5 GND. Most probes have a 2×5 1.27 mm Cortex connector, so make or buy an adapter and label it |
| USB-C cables: C-to-C, A-to-C, and a known-good short one | Enumeration in both plug orientations, Rd check | A USB-C charger too, to prove the 5.1 kΩ Rd resistors |
| 1:1 JST-SH 4-pin cable, daughterboard to mainboard | USB to the mainboard | Pin 1 to pin 1. A reversed cable puts VBUS on GND. Check it with the DMM before first use |
| Nordic Power Profiler Kit II (PPK2) | Current in every power state, battery life | Source mode powers the board at battery voltage and measures from about 200 nA to 1 A. The best single purchase for this plan |
| Oscilloscope, 2+ channels, 100 MHz | Rail ripple, switch bounce, encoder quadrature, LED data edges | 10× probes with short ground springs for ripple |
| Logic analyser, 8+ channels, 24 MHz or faster | Scan rate, latency markers, LED data timing | A Saleae, or a cheap FX2 clone with sigrok/PulseView. 24 MHz resolves the 800 kHz LED protocol |
| Second nRF52840 Dongle (Nordic PCA10059) | nRF Sniffer for Bluetooth LE in Wireshark; reference for the dongle firmware | About $10. The Dachshund dongle's pins match it |
| Phone with nRF Connect for Mobile | BLE advertising, RSSI, connection parameters, range walks | Free |
| 1S LiPo with a protection circuit, JST-PH 2-pin, 1000–2000 mAh | Charging and battery-life tests | JST-PH battery polarity is not standard between vendors. Check it before plugging in |
| Inline USB-C power meter | VBUS voltage and current, charging draw | For example a ChargerLAB POWER-Z or similar |
| Thermocouple or IR thermometer (a thermal camera is better) | Charger IC temperature at 500 mA | The MCP73831 dissipates about 0.6 W early in a charge |
| Optional: tinySA Ultra or HackRF | 2.4 GHz spectrum, carrier frequency, relative TX power | Only for the radio section |
| Optional: Cynthion USB analyser | USB packet timing on the wire, poll interval, report-to-host latency | Only if the host-side methods in Stage 4 aren't precise enough |
| Microscope or 10× loupe, hot-air station, flux | Inspection and rework | |

Software: nRF Connect for Desktop (Programmer, Power Profiler, RSSI apps), `nrfutil` with the `device` command, SEGGER J-Link tools (RTT Viewer), pyOCD, PulseView or Saleae Logic, Wireshark with the nRF Sniffer plugin, and on Linux `lsusb`, `usbhid-dump` and `evtest`. On Windows, USB Device Tree Viewer replaces `lsusb -v`.

## Stage 0: inspection and unpowered checks

Every board passes this stage with no power applied. Measure resistances with the DMM on its lowest range, red lead on the rail, black on GND; then swap the leads, because the module and ESD diodes read differently in each direction. A reading under about 10 Ω on any rail is a short: stop and find it before powering.

### All boards

1. Look at every board under the microscope against the KiCad 3D view: missing parts, tombstones, bridges on the module castellations, the USBLC6 and the SOT-23 parts.
2. Check orientation: the module (pin 1), USBLC6-2SC6 (U1 on the daughterboard and dongle), MCP73831 (U2) and TPS22917 (U3) on the mainboard, the BAT54WS (D1), and the SK6812s.
3. Photograph both sides of each board for the log.

### Daughterboard

| Check | Between | Expect |
| --- | --- | --- |
| VBUS to GND at J2 | J2-1 to J2-4 | High (kΩ to MΩ, the USBLC6 leakage); never near 0 |
| D+ and D− to GND | J2-3, J2-2 to J2-4 | High in both directions, diode-like one way |
| D+ to D− | J2-3 to J2-2 | High |
| CC1 and CC2 to GND | Receptacle CC pins (A5, B5) to GND | 5.1 kΩ ±1% each |
| PTC F1 | USB-C VBUS pin to J2-1 | Under 0.5 Ω (the MF-NSMF075 is a few hundred mΩ cold) |
| Shield to GND | Receptacle shell, H1, H2 to GND | Close to 0 Ω through FB1 (a ferrite bead is well under 1 Ω at DC) |
| D+, D− to shield | J2-3, J2-2 to shell | Open |
| Pin-for-pin continuity | USB-C D+ (A6/B6) to J2-3, D− (A7/B7) to J2-2 | Under 1 Ω, both rows of the receptacle |

Then check the JST-SH cable itself: J2-1 on one end beeps to pin 1 on the other, and so on for all four. Mark pin 1 on both plugs.

### Dongle

| Check | Between | Expect |
| --- | --- | --- |
| VBUS/VDDH to GND | C1 or C2 pads | High, not a short |
| VDD (+3V3) to GND | C3 or C4 pads, or J2-1 to J2-5 | High, not a short |
| CC1, CC2 to GND | Receptacle CC pins | 5.1 kΩ each |
| PTC, shield, D+/D− | As for the daughterboard | Same limits |
| PAIR | P1.06 net to GND, button pressed and released | Open, then under 1 Ω |
| LED resistors | R3, R4, R5 | 1 kΩ, 330 Ω, 330 Ω |

### Mainboard

| Check | Between | Expect |
| --- | --- | --- |
| VBUS to GND | J1-1 to J1-4, or C2 | High |
| VBAT to GND | J2 battery pins | High |
| VSYS to GND, switch on | C4 or C5 pads | High |
| VBAT_SW to VBAT | SW1 pin 2 (or Q1 drain) to J2-1, switch off, then on | Open, then under 1 Ω |
| Q1 body diode | Diode test, red on VBAT_SW (Q1 drain), black on VSYS (C4) | About 0.5–0.7 V forward, open the other way |
| D8 power-path Schottky | Diode test, red on VBUS (C1), black on VSYS (C4) | About 0.15–0.3 V forward, open the other way |
| Gate pull-down R3 | Across R3 | 100 kΩ |
| LED data buffer U4 | U1 P0.06 to U4 pin 2; U4 pin 4 to D3 DIN (pin 2) | Under 1 Ω each, and P0.06 not connected straight to D3 |
| +3V3 to GND | C6/C7 pads, J3-1 to J3-5, J4-1 to J4-2 | High |
| LED_VDD to GND | C12 or any of C13–C18 | High |
| PROG resistor R1 | Across R1 | 2.0 kΩ (sets 500 mA charge current) |
| LED_EN pull-down R2 | Across R2 | 100 kΩ |
| USB data | J1-2 and J1-3 to the module's D− and D+ pads | Under 1 Ω, and not to each other |
| SWD | J3-2, J3-3, J3-4 to the module's SWDIO, SWDCLK and nRESET pads | Under 1 Ω |
| Matrix spot check | A switch socket's two pads, diode in each direction | 1N4148W forward drop (about 0.5–0.7 V on diode test) one way, open the other |
| Encoder | SW3 A, B and common, unpowered, while turning slowly | A and B close and open in turn at each detent |

Battery polarity: look at J2's silkscreen, then at your cell's red lead. Many JST-PH cells from hobby suppliers are wired the opposite way from others. Fix the cable if they don't match; never plug a cell in to find out.

Solder the three plate-mount switches (SW604, SW606, SW615) only after Stage 3, so you can still rework underneath. Leave the hot-swap switches and the plate out until Stage 3 too: a bare board is easier to probe, and you can test each matrix position by shorting its socket pads with tweezers.

### Mainboard test pads

Nine bare 1.5 mm round pads on the back, each labelled with its net on the silkscreen. They're flush and fit a standard meter probe tip.

| Pad | Net | Where (back side, x, y mm) |
| --- | --- | --- |
| TP1 | VBUS | Next to C1, by the charger (325.6, 89.2) |
| TP2 | VBAT | Below the charger, above J2 (322.0, 93.2) |
| TP8 | GND | Just below TP2 (321.5, 96.5) |
| TP7 | CHG_STAT | Left of the module, near C8/C9 (308.5, 71.5) |
| TP3 | VBAT_SW (after the switch, before Q1) | Next to SW1 (331.5, 110.5) |
| TP4 | VSYS | Above U3, below the J4 header (323.1, 116.0) |
| TP5 | +3V3 | Beside SW2 (reset), above J4 (318.2, 110.4) |
| TP6 | LED_VDD | Below U3, near D3/D4 (314.6, 123.4) |
| TP9 | GND | Between U3 and TP4 (319.5, 120.5) |

The tables below name capacitor pads from before the test pads existed; the matching TP gives the same reading.

## Stage 1: first power and rails

Power each board from the bench supply with the current limit at 50 mA, watch the current for 5 seconds, then measure the rails. A blank module draws a few mA; anything that sits at the limit is a fault. Only after the rails pass do you plug in USB or a battery.

A blank nRF52840 runs its regulator at the factory REGOUT0 setting, so VDD reads about 1.8 V until firmware or the bootloader writes 3.3 V into UICR. Both boards' firmware and the bootloader do that on first boot, then reset.

### How the mainboard is powered

VSYS has two sources, in Microchip's load-sharing arrangement for the MCP73831 (AN1149):

- **USB:** VBUS through the B5819W Schottky D8, so VSYS sits about 0.2–0.3 V below VBUS (around 4.7 V).
- **Battery:** VBAT through the slide switch SW1 (net VBAT_SW), then the AO3401A P-MOSFET Q1 into VSYS. Q1's gate is on VBUS with a 100 kΩ pull-down (R3). With USB unplugged the gate sits at 0 V, so Q1 is fully on. With USB in, the gate is above the source, so Q1 is off and the battery only charges.

So:

- On USB the keyboard runs whatever the switch says, and with no battery fitted. The switch only disconnects the battery.
- On battery it runs only with the switch on.
- The charger sits on VBAT, before the switch, so USB charges the battery with the switch off too.
- With USB in, the SAADC's VDDH/5 reading shows the USB-fed VSYS, not the cell. Firmware has to read the battery with USB unplugged, or show "charging" from CHG_STAT instead.

### Mainboard

Use the bench supply on the battery connector J2 as a stand-in for the cell, switch on.

| Step | Measure | Pass |
| --- | --- | --- |
| 3.7 V on J2, limit 50 mA | Supply current | Under 10 mA on a blank module, steady |
| | VSYS at C4/C5 | Equal to the supply, within 20 mV |
| | +3V3 at C6/C7, or J3-1 | About 1.8 V blank; 3.3 V ±3% after the first flash |
| | LED_VDD at C12 | Under 0.1 V (TPS22917 off; EN held low by R2) |
| Sweep J2 from 3.0 V to 4.2 V | +3V3 | Stays 3.3 V ±3% once REGOUT0 is set. At 3.3 V in and below, the LDO drops out and VDD follows VDDH less about 0.1 V |
| Ripple | +3V3 and VSYS on the scope, AC coupled, 20 MHz limit, short ground spring | Under 20 mV peak to peak idle, under 50 mV while the radio transmits |
| Switch off | Supply current | Under 1 µA (only the charger's reverse leakage) |

Then USB alone, through the daughterboard and the checked cable, with nothing on J2 and the current-limited supply on the USB side if you have a USB power meter or breakout:

| Step | Measure | Pass |
| --- | --- | --- |
| USB in, no battery, switch off | VSYS at C4/C5 | VBUS less 0.15–0.35 V (about 4.7 V); the board runs |
| | VBAT_SW (Q1 drain) | Near 0 V: Q1 is off and the switch is open |
| | Q1 gate (R3) | Equal to VBUS |
| Same, switch on | VSYS | Unchanged. VBAT_SW follows VBAT (the charger output, cycling with no cell; that's harmless now that VSYS doesn't depend on it) |

Then add USB through the daughterboard and the checked cable, with the bench supply still on J2:

| Step | Measure | Pass |
| --- | --- | --- |
| USB plugged in | VBUS at C2 or J1-1 | 4.75–5.25 V |
| | Supply current into J2 | Goes negative or to zero: the charger now pushes current into the supply. Most bench supplies can't sink, so the voltage rises to 4.2 V. That is expected; unplug USB to continue |
| | CHG_STAT at P0.15 (the module side of D1) | Low while charging |

### Charger, with a real cell

Use a partly discharged 1S LiPo (3.6–3.8 V), checked for polarity, with the DMM in series on its 10 A range, or the PPK2 in ammeter mode (limit 1 A).

| Measure | Method | Pass (MCP73831-2, PROG = 2 kΩ) |
| --- | --- | --- |
| Fast-charge current | Ammeter in the battery lead, USB in | 500 mA ±10% (I = 1000 V / R_PROG) |
| Charge voltage | DMM across J2 at the end of the CV phase | 4.20 V ±0.75% (4.168–4.232 V) |
| Termination current | Ammeter, at the moment STAT goes high | About 7.5% of 500 mA (around 38 mA); check against the datasheet's table for the -2 variant |
| Charger temperature | Thermocouple or thermal camera on U2 in the first 10 minutes | Under 85 °C on the case; note if thermal regulation cuts the current |
| Charge time | Log current every minute, 3.0 V to full | Record it; about 2.5–4.5 h for 1000–2000 mAh at 500 mA |
| STAT on P0.15 | Logic analyser or DMM | Low while charging, released when done. The BAT54WS should block the 5 V high from reaching the GPIO: check P0.15 never exceeds 3.6 V |

### Dongle

Power it from USB through the inline power meter, or from the bench supply on VBUS (5 V, 50 mA limit).

| Measure | Where | Pass |
| --- | --- | --- |
| Current | Power meter | Under 10 mA blank |
| VBUS/VDDH | C1/C2 | 4.75–5.25 V |
| VDD | C3/C4 or J2-1 | About 1.8 V blank, 3.3 V ±3% after REGOUT0 is set |
| LED | D1 | Off on a blank module (pins float); green and blue barely glow at 1.8 V |

### Daughterboard

Plug it into a charger with a C-to-C cable, with nothing on J2: J2-1 reads 5 V. With an A-to-C cable it reads 5 V regardless of Rd. If the C-to-C charger gives 0 V, an Rd resistor is missing or open.

## Stage 2: flashing

Each board gets its bootloader once over SWD; after that, firmware goes on by copying a `.uf2` file to the drive it mounts. SWD stays useful for RTT logs and for recovery.

### Connecting the probe

1. Wire the probe to J3 (mainboard) or J2 (dongle): 1 VDD (the probe's voltage reference, not a supply), 2 SWDIO, 3 SWDCLK, 4 nRESET, 5 GND. Keep the wires under 15 cm.
2. Power the board itself: the bench supply on J2 for the mainboard, USB for the dongle. The probe must not power it.
3. Check the probe sees the chip: `nrfutil device list`, or `pyocd list` then `pyocd commander -t nrf52840`.
4. If the probe connects but can't read memory, the chip has access port protection on. Newer nRF52840 silicon (build code F and later) enables it at every reset unless UICR says otherwise. Run `nrfutil device recover` (or `pyocd erase -t nrf52840 --chip`); it erases everything and opens the port. Note which silicon revision the modules carry; the marking is on the chip under the shield, so read it from `nrfutil device device-info` instead.

### Mainboard: bootloader, then firmware

The bootloader board files are in `boards/Dachshund60F/bootloader/` (merged in PR #7). Its README (`boards/Dachshund60F/bootloader/README.md`) has the full procedure; in short:

1. Build it with `boards/Dachshund60F/bootloader/build.sh` on Linux or WSL, or download the `bootloader-<sha>` artifact from a CI run.
2. Flash `dachshund60f_bootloader_s140_6.1.1.hex` with a full erase:
    - `nrfutil device program --firmware dachshund60f_bootloader_s140_6.1.1.hex --options chip_erase_mode=ERASE_ALL,verify=VERIFY_READ`
    - or `pyocd flash -t nrf52840 --erase chip dachshund60f_bootloader_s140_6.1.1.hex`
3. Reset. With no app, the DACHSHUND drive mounts (1209:0003) and the six LEDs show the bootloader colours.
4. Check `INFO_UF2.TXT` reads `Board-ID: nRF52840-Dachshund60F-v1`.
5. Check +3V3 now reads 3.3 V (the bootloader set REGOUT0).
6. Build the firmware (`west build -p -b dachshund60f/nrf52840 -d build/firmware firmware -- -DBOARD_ROOT="$PWD"`, or take `zephyr.uf2` from CI) and copy `build/firmware/firmware/zephyr/zephyr.uf2` to the drive. The board resets into the firmware and enumerates as 1209:0001.
7. Check every way back into the bootloader: hold Esc 3 s in the firmware; hold Esc while pressing reset; double-tap reset. The drive must come back each time.
8. Brick test: copy a deliberately bad UF2 (for example the dongle's) and check the bootloader still comes up on double-tap reset. A UF2 with the wrong family ID should be refused outright.

If the probe can't attach once the app runs, that is the protection from step 4 again. `nrfutil device recover` fixes it but wipes the bootloader, so reflash. RTT logging needs the debug port open, so find out whether the bootloader and Zephyr leave it open on your silicon, and record it.

RTT logs: `JLinkRTTViewer` (device NRF52840_XXAA, SWD, 4000 kHz), or `pyocd rtt -t nrf52840`. Debug builds log over RTT; release builds have none.

### Dongle

The dongle's firmware is the phase-1 stub: it logs its version over RTT and does nothing else (no USB, no LED, no REGOUT0 write). So for bring-up, use Adafruit's prebuilt bootloader, which exercises USB, the LED and PAIR without any of our code.

1. Download `pca10059_bootloader-<version>_s140_6.1.1.hex` from the Adafruit nRF52 bootloader releases. This is the stock PCA10059 build; the dongle's PAIR button (P1.06) and blue LED (P0.12) match it.
2. Flash it over J2 with a full erase, as for the mainboard.
3. Hold PAIR and press RESET. A UF2 drive should mount. Record whether it does; this hasn't been tried on this board.
4. To run the stub app over SWD instead (for RTT), build it with `west build -p -b nrf52840dongle/nrf52840 -d build/dongle dongle -- -DCONFIG_BOARD_HAS_NRF5_BOOTLOADER=n`, so it links at address 0 rather than after Nordic's bootloader, then `west flash -d build/dongle -r nrfutil`.

Until the phase 14 dongle board target exists, a Zephyr USB sample built for `nrf52840dongle/nrf52840` (for example `samples/subsys/usb/hid-keyboard`) is the quickest way to test enumeration through our own stack.

### Daughterboard

Nothing to flash. Its tests run through the mainboard.

## Stage 3: functional tests

This stage answers "does every part work at all". The "Firmware" column says what has to be on the board; where it names a phase that isn't built yet, that test waits.

### Mainboard

| Area | Test | Pass | Firmware |
| --- | --- | --- | --- |
| USB enumeration | `lsusb -v` (or USB Device Tree Viewer) | 1209:0001, "Dachshund" / "Dachshund60F", a serial unique to each board, bcdDevice from `firmware/VERSION`, bMaxPower 500 mA, HID interrupt endpoint with bInterval 1 | Phase 2 (main) |
| USB-C orientation | Enumerate with the plug both ways round, and through A-to-C and C-to-C cables | Enumerates every time | Phase 2 |
| Hot-plug | Unplug and replug 20 times | Enumerates every time; no hang (check RTT) | Phase 2 |
| Boot keyboard | Type in a BIOS/UEFI setup screen; through a KVM if you have one | Keys work | Phase 3 (main) |
| Every key | Short each socket's pads with tweezers on the bare board, then press every fitted switch; watch `evtest` or an online key tester, or the RTT log | All 80 positions report, each as the right key, with no extra key. Both spacebar layouts | Phase 3 |
| Ghosting | Hold three corners of any rectangle (for example Q, W, A) and press the fourth (S) | Only the pressed keys report. Each key has its own diode, so there should be no ghost | Phase 3 |
| Rollover | Hold 6 keys, then more, in an online key tester; then Fn+N (NK_TOGG) and repeat | Every held key reports with NKRO on; six with it off | Phase 4 |
| Boot fallback | In a BIOS/UEFI setup screen or through a KVM, hold 7 keys | Six keys report, no stuck keys; back in the OS, NKRO works again | Phase 4 |
| Media and system keys | Fn+F-row media keys, volume, mute; Sleep if mapped | The host reacts; `lsusb -v` shows a second HID interface with report IDs 1–3 | Phase 4 |
| Suspend and wake | Put the computer to sleep with the keyboard plugged in, then press a key | The LEDs go dark while the host sleeps; a key press wakes it (if the host allows USB wake); after waking, no key is stuck | Phase 4 |
| Config interface | `lsusb -v`; then the handshake from host-lib (phase 7) or Doghouse (phase 8) | A third HID interface, usage page 0xFF00, usage 0x44, 64-byte reports in and out. The handshake reports protocol 1, the firmware version, 6×15×4 and "Dachshund60F"; `get_serial` returns the same text as the USB serial; `identify` blinks the LEDs | Phase 5 |
| Settings survive power-off | Change a key and save (Doghouse, phase 8, or host-lib), unplug, replug; then revert and factory reset | The change is kept after replugging; revert brings back the saved keymap and factory reset the default one; an NKRO choice made with Fn+N and then saved is kept | Phase 6 |
| Lock LEDs | Toggle Caps, Num, Scroll on the host | Caps LED (D2) lights warm white while Caps Lock is on; the screen shows all three; the layer LED (D3) shows amber on Fn | Phase 4; colours settable in phase 10 |
| LED chain | Bring-up walk on power-up (once, 3.6 s) | One LED at a time in chain order D3 (layer), D4–D7 (battery 1–4), D2 (Caps), each red then green then blue for 200 ms. Wrong order means a routing fault; wrong colours means the GRB order is off | Phase 2; one pass since phase 4 |
| LED power switch | LED_VDD at C12 during the walk, then after it with every LED dark (no Caps Lock, base layer) | VSYS when on; under 0.1 V when off, since the firmware cuts LED power whenever no LED is lit | Phase 4 |
| Encoder | Turn slowly one detent at a time, then fast; press | One step per detent, the right direction, push reports as (row 0, col 14) | Push: phase 3. Rotation: phase 9 (pins are only in `zephyr,user`) |
| Reset button | Press it | Board resets; a double tap enters UF2 | Bootloader |
| Power switch | Off and on while on battery | Board dies and restarts cleanly | Any |
| 32.768 kHz crystal | RTT log of the low-frequency clock source, or BLE connecting reliably | LFXO starts (not LFRC fallback) | Phase 2 with a log line, or phase 12 |
| Display | Build with each panel overlay you own and connect it to J4 | Text, layer, locks and "USB" show; orientation fits the case | Main, with `-DEXTRA_DTC_OVERLAY_FILE=displays/<panel>.overlay` |
| Battery reading | Firmware reads VDDH/5 via the SAADC with USB unplugged; compare against the DMM on J2 at 3.0, 3.7, 4.2 V | Within ±50 mV after calibration (Q1 drops a few mV) | Phase 13 |
| Power path | Plug and unplug USB on battery while typing; then USB with no battery, switch off | No reset or dropped keys at either transition; runs from USB alone | Any |
| LEDs on USB | LED walk with USB in and no battery (LED_VDD about 4.7 V); scope U4 pin 4 | Correct colours, no flicker; U4 output swings to LED_VDD | Phase 2 |
| Charge status | P0.15 read in firmware, with and without USB | Charging shown while STAT is low | Phase 13 |
| BLE | Pair with a phone and a PC; type; switch between 3 profiles | Connects, types, switches, reconnects after power cycle | Phase 12 |
| Config protocol | Doghouse connects over USB, reads and writes a key, saves, power-cycles, reads back | SPEC's hardware-in-the-loop smoke test passes | Phases 5–8 |

### Daughterboard

Run these with the mainboard flashed:

- The mainboard enumerates through it in both plug orientations, with A-to-C and C-to-C cables.
- A C-to-C cable from a USB-C charger (no data) charges the battery: the Rd resistors work.
- Wiggle the cable in the receptacle while typing for 30 s: no disconnects.
- With a metal case later: the case reads close to 0 Ω to GND through FB1 and the screws.

### Dongle

| Test | Pass | Firmware |
| --- | --- | --- |
| UF2 drive mounts with PAIR held at reset | Drive appears | Adafruit pca10059 bootloader |
| Both plug orientations, A-to-C and C-to-C | Enumerates each time | Bootloader, then phase 14 |
| RGB LED | Red, green and blue each light; no colour stuck on | Bootloader shows blue; full test in phase 14, or a Zephyr blinky for `nrf52840dongle/nrf52840` (same LED pins) |
| PAIR and RESET | Each works | Bootloader |
| Enumerates as 1209:0002 "Dachshund60F Receiver" | Shown in `lsusb -v` | Phase 14 |
| Pairs with the keyboard and relays keys | Keys type through it | Phase 14 |

## Stage 4: performance measurements

This stage puts numbers on the boards. Most timing tests need a marker: a spare GPIO that the firmware toggles at a known moment, captured on the logic analyser next to the real signal. The board has no spare pins, but J4's display pins are free when no display is fitted. Use J4-8 (BUSY, P0.24), J4-5 (CS, P0.20) and J4-6 (DC, P0.23) as markers in a test build; this needs a small test option in the firmware (see the missing-firmware section).

### Current draw and battery life

Method: the PPK2 in source mode at 3.70 V on battery connector J2 (in place of the cell), USB unplugged, switch on. Record 60 s per state, longer for idle states; note average, peak and the trace shape. Repeat at 3.3 V and 4.2 V for the main states.

| State | What to capture | Rough expectation (to replace with your numbers) | Firmware |
| --- | --- | --- | --- |
| System OFF (deep sleep), LEDs off | Average over 60 s | Single-digit µA for the whole board: nRF52840 System OFF is about 0.4 µA, plus charger and switch leakage | Phase 13 |
| BLE connected, idle | Average; the spikes at each connection event | Tens of µA with peripheral latency; mostly set by the connection interval and latency | Phases 12, 13 |
| BLE connected, typing at about 5 keys/s | Average | Hundreds of µA to about 1 mA, with matrix scanning at 1 kHz | Phases 12, 13 |
| 2.4 GHz mode, idle and typing | Average | Higher than BLE if the HFXO stays on, as the dongle README proposes for latency. Measure the trade-off | Phase 14 |
| LEDs powered, all black | Average with LED_VDD on and all LEDs set to 0 | SK6812s draw current even when dark (around 0.5–1 mA each, check the datasheet). This is why the TPS22917 exists | Phase 2 walk, phase 10 |
| Each LED, each colour, at the brightness you'll use | Difference from the all-black state | Record per colour | Phase 10 |
| Display idle and updating | Per panel | OLED full white must stay well inside the about 25 mA available on +3V3; e-paper should be near zero between refreshes | Main (status screen) |
| Matrix idle (no key) vs one key held | Average | A held key costs current through its row while its column is driven; quantify it | Phase 3 |

Battery life for a usage profile:

```latex
T_{hours} = \frac{0.8 \times C_{mAh}}{I_{avg,mA}}, \quad I_{avg} = \sum_i f_i \, I_i
```

where f is the fraction of time in each state. For example, 4 h typing, 4 h connected idle and 16 h in deep sleep a day. The 0.8 allows for cell ageing and the cut-off voltage. Cross-check the estimate once with a real cell: charge it full, run the profile until the board shuts down, and log the time.

Also measure wake-up: the PPK2's digital input or the logic analyser on a marker, from a key press in System OFF to the first report on the host (phase 13).

### Matrix scan rate and debounce

| Measure | Method | Target |
| --- | --- | --- |
| Scan period and jitter | Marker toggled at the start of each scan; logic analyser over 10 s; histogram of periods | 1000 µs (`CONFIG_KB_MATRIX_SCAN_PERIOD_US`), jitter under 50 µs, none skipped while USB and BLE are busy |
| Scan duration | Marker high for the whole scan | Well under the period; record it, because it sets how fast you can scan |
| Column settle | Scope on a row line while its column is driven, with a key held | The row reaches a valid high within the firmware's settle wait, and falls back low before the next column (the firmware waits for it, bounded) |
| Switch bounce | Scope on a loose switch: 3.3 V through 10 kΩ to one pin, other pin to GND; trigger on the first edge; 50 presses and 50 releases per switch type you'll use | Record the worst bounce. The 5 ms default (`CONFIG_KB_DEBOUNCE_MS`) must exceed it |
| Chatter | Press every key 100 times at a steady rate while a key tester counts events | Exactly 100 presses and 100 releases per key |
| Debounce algorithm effect | Latency rig (below) with eager-press versus deferred debounce | Eager: report on the first edge. Deferred: about the debounce time later |

### USB polling and latency

| Measure | Method | Target |
| --- | --- | --- |
| Declared interval | `lsusb -v`: the HID interrupt IN endpoint | bInterval 1 (1 ms at full speed) |
| Actual polling | A Cynthion (or any USB analyser) shows the host's IN tokens directly. Without one: Linux `usbmon` in Wireshark with a test build that sends a report every frame, then look at the gaps between completions | 1000 Hz. This is the full-speed ceiling for both the keyboard and the dongle |
| Firmware latency | Latency rig, t0 to t1 | Under 1.5 ms with eager debounce at 1 kHz scanning |
| End-to-end latency | Latency rig, t0 to t2 | Record min, median, 99th percentile and max over 1000 presses |
| Boot time | Power on (switch) to enumeration | Record it; the 3.3 V REGOUT0 reset adds one reboot the first time only |

The latency rig:

1. **t0, the press:** a photoMOS relay (for example a Panasonic AQY212) across one switch's socket pads, driven by a microcontroller (a Pi Pico is fine) at random moments. Its drive pin goes to logic analyser channel 0. This gives a bounce-free press at a known time.
2. **t1, the report:** a firmware marker on J4 toggled when the report is handed to the transport (USB, BLE or ESB). Channel 1.
3. **t2, the host:** a small script on the PC that toggles the RTS line of a USB-serial adapter whenever it sees the key event (`evdev` on Linux, raw input on Windows). Channel 2. Measure the RTS path alone once (a loopback test) and subtract it; set the FTDI latency timer to 1 ms.
4. Run 1000 presses for each transport and each debounce setting, and plot the distribution.

### Radio: BLE and 2.4 GHz

| Measure | Method | Target |
| --- | --- | --- |
| BLE connection parameters | nRF Sniffer (PCA10059 + Wireshark) on the connection; note the interval, peripheral latency and supervision timeout each host grants | Interval 7.5–15 ms; record per host OS |
| BLE latency | Latency rig over BLE | Expect about the connection interval plus processing (dongle README: 7.5–15 ms) |
| 2.4 GHz latency | Latency rig over the dongle; puck next to the keyboard, then plugged straight into a USB 3 port | Dongle README estimate: about 1 ms typical, 2.5 ms worst case without retransmits |
| RSSI against distance | nRF Connect for Mobile (BLE) or the dongle's own RSSI (ESB) at 0.5, 1, 3, 5, 10 m line of sight, and through one wall | Record it; compare keyboard bare, in the case, and with a hand on the right edge over the antenna |
| Packet loss and retransmits | Test mode that sends a numbered report every 10 ms for 60 s at each distance; count gaps on the receiver | Zero loss at desk distance (1 m) with Wi-Fi busy |
| Coexistence | Repeat at 1 m with a busy 2.4 GHz Wi-Fi link and a USB 3 drive copying next to the dongle | Record the change in retransmits and latency |
| Carrier frequency and relative TX power | Nordic's `radio_test` sample (in NCS) sending a constant carrier at 2402, 2440 and 2480 MHz; tinySA Ultra or HackRF at a fixed 1 m | Frequency within the BLE limit of ±50 ppm (about ±120 kHz); the module's crystal should do much better. Power: compare keyboard, dongle and a PCA10059 in the same spot |
| Range limit | Walk away typing a repeating test string until characters drop | Record the distance per transport |

`radio_test` uses a UART shell, and neither board has a UART, so it needs the RTT shell backend and a board overlay to build for these boards.

### LEDs

| Measure | Method | Target |
| --- | --- | --- |
| Data timing | Logic analyser on LED_DIN at D3 (the first LED) | SK6812: T0H about 0.3 µs, T1H about 0.6 µs, period about 1.25 µs, reset low over 80 µs (check the exact limits in the datasheet) |
| Signal at the far end | Scope on D2's DIN (Caps, about 310 mm of track away), 10× probe, short ground | Clean edges; ringing doesn't cross the threshold |
| Logic level margin | LED_VDD at full charge (4.2 V) against the 3.3 V data high | The SK6812's VIH is about 0.7 × VDD, so 2.94 V at 4.2 V; 3.3 V clears it by about 0.36 V. Confirm every LED latches correctly with the bench supply at 4.25 V |
| Brightness and colour | Phone camera with locked exposure, or a lux meter, through the plate window | Even across the battery gauge; no visible bleed between windows |

### Encoder

| Measure | Method | Target |
| --- | --- | --- |
| Quadrature and bounce | Scope on A and B (P0.11, P0.12), turning one detent at a time | 90° phase, a full cycle (4 edges) per detent, bounce under about 2 ms per edge |
| Detent position | Where the detent rests in the A/B cycle | At the same state each time, matching `stepsPerDetent: 4` |
| Missed steps | Turn 10 full revolutions (240 detents) slowly, then fast | Exactly 240 counts in each direction (phase 9) |
| Push switch | As a matrix key, (row 0, col 14) | Same chatter test as the keys |

### Display

- OLED: current at full white and at the normal layout (PPK2), time from a lock change to the redraw (target under 20 ms, per the status-screen design).
- E-paper: refresh time and current pulse per update, and that updates combine at the 3 s minimum interval.

### Thermal

Thermal camera or thermocouple on U2 (charger), the module and U3 while charging at 500 mA with LEDs on and the radio busy. Record the hottest part and whether the charger enters thermal regulation inside a closed case.

## Missing firmware and tools

About a third of this plan can run on the day the boards arrive. The rest waits for firmware phases that aren't written yet, or for small test-only features. As of 2026-10-23:

| Item | Status | Unblocks |
| --- | --- | --- |
| Phase 2: USB enumeration, LED walk, Esc-to-UF2, RTT logs, REGOUT0 | On main | Enumeration, LED chain, LED switch, rails |
| Status screen (four panels) | On main | Display tests |
| Dachshund60F UF2 bootloader | On main (PR #7); never run on hardware | First SWD flash and every UF2 step |
| Phase 3: matrix scan at 1 kHz, debounce (eager press, 5 ms), keymap, key reports | On main (PR #8) | Every-key, ghosting, chatter, BIOS, scan and debounce timing |
| Phase 4: NKRO, consumer keys, lock LEDs, suspend and remote wakeup | On main (PR #13) | Rollover, boot fallback, media keys, lock LEDs, suspend and wake |
| Phase 5: config protocol v1 on a vendor HID interface | Done, in review | Config interface |
| Phase 6: persistence (keymap and settings in flash) | Done, in review | Settings survive power-off |
| Phases 7–8: host transports, GUI on hardware | Not started (Doghouse runs against mock boards only) | Smoke test, matrix test mode, Doghouse key tester |
| Phase 9: encoder driver | Not started; pins sit in `zephyr,user` | Encoder rotation tests |
| Phase 10: indicator colours | Not started | Lock LED colours, per-colour LED current |
| Phase 12: BLE | Not started | All BLE tests, LFXO confirmation |
| Phase 13: power management, battery reading, charge status | Not started | Sleep currents, battery life, battery reading, STAT |
| Phase 14: dongle firmware and its board target | Stub only: logs its version over RTT, no USB, no radio, no REGOUT0 write | Every dongle test past the bootloader, 2.4 GHz latency and range |
| Dongle bootloader | Adafruit's stock `pca10059` build is expected to work; untested | UF2 on the dongle |

Small test-only features this plan needs, none of them built yet:

- [ ] Latency and scan markers on J4 pins, behind a Kconfig option that is off in normal builds
- [ ] A log line naming the low-frequency clock source (LFXO or LFRC)
- [ ] A test mode that sends a numbered report every 10 ms, for packet-loss counts over BLE and ESB
- [ ] ESB counters on the dongle (retransmits, lost packets, RSSI), readable over RTT or the config protocol
- [ ] `radio_test` built for both boards with the RTT shell
- [ ] The PC-side latency script and the Pico photoMOS presser

The mainboard's USB power path (Q1, D8, R3) was added before ordering, on 2026-10-23; see "How the mainboard is powered" in Stage 1.

## Results log

One row per test per board; add rows as you go. Keep scope captures, PPK2 exports and photos in a folder named after the board serial, and link them in the Data column. The Claude Doc has the same table with a Result dropdown.

| Date | Board | Stage | Test | Measured | Result | Data | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- |
|  | DB-01 | 0 | CC1, CC2 to GND |  | Not run |  |  |
|  | DB-01 | 0 | JST-SH cable pin 1 to pin 1 |  | Not run |  |  |
|  | DG-01 | 1 | VDD blank, then 3.3 V |  | Not run |  |  |
|  | DG-01 | 2 | pca10059 UF2 drive with PAIR |  | Not run |  |  |
|  | MB-01 | 1 | Current at 3.7 V, blank |  | Not run |  |  |
|  | MB-01 | 1 | Charge current and 4.20 V |  | Not run |  |  |
|  | MB-01 | 2 | Bootloader over SWD, Board-ID |  | Not run |  |  |
|  | MB-01 | 3 | Enumerates 1209:0001, both orientations |  | Not run |  |  |
|  | MB-01 | 3 | LED chain order and colours |  | Not run |  |  |
|  | MB-01 | 3 | All 80 positions |  | Not run |  |  |
|  | MB-01 | 4 | Scan period and jitter |  | Not run |  |  |
|  | MB-01 | 4 | USB end-to-end latency (median, p99) |  | Not run |  |  |

When a stage is done on hardware, copy the headline results into the Tested section of `PROGRESS.md`.
