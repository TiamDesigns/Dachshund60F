# Dachshund60F 2.4 GHz dongle

The receiver for the keyboard's 2.4 GHz mode (SPEC phase 14). It is a small desk puck: a USB-C cable
runs to the computer, and the puck sits on the desk near the keyboard. KiCad 10, 2 layers, 1.6 mm.

Pins come from the `dongle` section of `boards/Dachshund60F/board.json`, as for the keyboard.

## Board

- 40 × 24 mm, 2 mm corners. Two unplated M2 holes, both at the USB end, so no screws sit near the antenna.
- **U2, Raytac MDBT50Q-1MV2:** the same nRF52840 module as the keyboard, with a certified chip antenna.
  The antenna end sits 0.4 mm inside the right edge. A keep-out covers the whole strip from the antenna
  to the edge, across the full board width, on both layers: no copper, pour or vias there.
- **J1, USB-C (HRO TYPE-C-31-M-12):** the same part as the keyboard's daughterboard. It is on the left
  edge, at the opposite end from the antenna. USB 2.0 sink: 5.1 kΩ Rd on CC1 and CC2, no PD.
- **Protection:** U1 USBLC6-2SC6 ESD clamp on D+, D− and VBUS; F1 750 mA PTC on VBUS. FB1 ties the
  receptacle shell to GND (the daughterboard does the same).
- **Power:** VBUS feeds both the module's VBUS and VDDH pins (high-voltage mode). VDD (REGOUT0) is +3V3.
  DCCH is unconnected, so REG0 runs as an LDO. There is no 32.768 kHz crystal, so firmware uses the
  internal RC oscillator. Decoupling: 4.7 µF + 100 nF on VBUS/VDDH (C1, C2) and on VDD (C3, C4).
- **User parts:**
  - SW1 PAIR: P1.06, active low.
  - SW2 RESET: P0.18.
  - D1: RGB status LED, common anode on +3V3. Red P0.08 through 1 kΩ; green P1.09 and blue P0.12
    through 330 Ω. All active low, about 1–2 mA each at standard drive.
  - The buttons sit along the top edge, with RESET on the left.
- **J2, SWD:** a 1×5 1.27 mm header in the keyboard's pin order: 1 VDD, 2 SWDIO, 3 SWDCLK, 4 nRESET,
  5 GND. Pin 1 is the probe's voltage reference. Power the dongle from USB while debugging.
- **PCA10059 compatibility:** the button and LED pins match Nordic's nRF52840 Dongle. The dongle
  firmware can be developed on a PCA10059, and the existing `nrf52840dongle/nrf52840` build target
  drives the same button and LEDs. Adafruit's `pca10059` UF2 bootloader uses P1.06 as its DFU button
  and P0.12 as an LED, so it should run here unchanged: hold PAIR while plugging in to get the UF2
  drive. This hasn't been tried yet.

### Routing

- **D+/D−:** routed by hand from U1 to the module as a side-by-side pair, about 23.5 mm each, on F.Cu
  over the B.Cu ground plane. The pair approaches the module pads from below.
- **USB-C fan-out:** no vias on the data lines. The two D+ pads are joined by a short loop under the
  receptacle body, and the two D− pads just behind the pads. U1 is flow-through: each line runs
  straight under the package between its two I/O pins.
- **Supplies:** short straight tracks, hand-routed and locked: +3V3 from C3/C4 into VDD; VDDH down to
  the VBUS row from C1/C2.
- **Everything else:** Freerouting 2.5.0, then GND pours on both layers and stitching vias. B.Cu is
  almost entirely ground.
- **Checks (kicad-cli):** ERC 0. DRC: 0 errors, 0 unconnected items, 0 schematic parity issues. Two
  warnings remain: the stock receptacle footprint's silkscreen runs past the board edge, the same as
  on the daughterboard.
- **Pin check:** a script compared every module pad with `board.json` and followed each net to its
  button, LED, resistor, SWD or USB pin: 0 mismatches.
- One +3V3 track runs on B.Cu under the D+/D− pair on its way to J2. At 12 Mbit/s this doesn't
  matter, but a re-spin could move it.

## Polling rate: 1000 Hz

The nRF52840's USB peripheral is **full speed only (12 Mbit/s)**. A full-speed interrupt endpoint can
be polled at most once per 1 ms frame (bInterval = 1), so **1000 Hz is the ceiling** for this dongle,
and for the keyboard over its own USB cable. SPEC already asks for a 1 ms interval.

The radio isn't the limit. An ESB packet at 2 Mbit/s with a ~16-byte report is about 100 µs on air,
plus 40 µs for the fast radio ramp-up. The link could carry well over 2000 reports per second.

4000 Hz or 8000 Hz would need **High-Speed USB (480 Mbit/s)**, which polls every 125 µs. That means a
different receiver chip, such as Nordic's nRF54LM20A, which has a High-Speed USB peripheral. It's out
of scope for v1, since the stack is pinned to the nRF52840.

## Keeping latency low

Rough budget for one key press over the dongle, at 1000 Hz polling and with no retransmits. These are
estimates to be measured in phase 14; SPEC already lists logic-analyser latency tests.

| Stage | Time | How to keep it low |
| --- | --- | --- |
| Detect the press | ~0.05–1 ms | When idle, drive all columns and wait for a row interrupt (GPIO SENSE), so the first press wakes the scan at once. While keys are held, scan at 1 kHz or faster. |
| Debounce | ~0 ms | Eager debounce on press (report the first edge, then ignore the key for 5 ms), deferred on release. |
| Keymap | µs | Plain keys go out at once. Tap-hold keys wait by design; keep their timeouts short. |
| Radio | ~0.15–0.2 ms | ESB at 2 Mbit/s, fast ramp-up, a short fixed report, and send immediately on change rather than on a timer. Keep the keyboard's HFXO running while keys are active (cold start costs ~0.3–0.4 ms). In dongle mode, keep BLE off so ESB has the radio to itself. |
| Retransmits | +~0.3 ms each | Short retransmit delay, a few retries, then hop to the next channel. Pick channels between the Wi-Fi channels 1, 6 and 11. |
| Dongle to USB | 0–1 ms (avg 0.5 ms) | The dongle listens all the time (it runs on USB power). Queue the report on the IN endpoint from the radio interrupt with bInterval 1. Use a small FIFO so a fast press and release in one frame isn't merged. |

**Typical total: about 1 ms, worst case about 2.5 ms without retransmits.** BLE is slower: its
connection interval is 7.5 ms at best, and many hosts use 11.25–15 ms.

What the hardware does for latency:

- **Puck on a cable, not a stick:** the puck can sit next to the keyboard, away from the computer's
  metal case and from USB 3 ports, which emit noise in the 2.4 GHz band. Fewer lost packets means fewer
  retransmits.
- **Clear antenna:** the antenna is at the board end with a full-width keep-out, and there is no metal
  near it. Use a plastic case, with no screws or labels over the antenna strip.
- **USB-powered receiver:** the dongle can keep its radio in receive and its crystal oscillator running
  all the time, with no wake-up delay.
- **Host to keyboard:** send lock LEDs and config data back as ESB ACK payloads, so the keyboard never
  needs a separate receive window.

## Bring-up checks

1. Power from USB, then check the rails: VBUS ≈ 5 V, and VDD ≈ 1.8 V on a blank module (the factory
   UICR default). Firmware must set REGOUT0 to 3.3 V, as the keyboard's `board.c` does; green and blue
   barely light at 1.8 V.
2. Flash over SWD: either the Adafruit `pca10059` UF2 bootloader (then hold PAIR while plugging in for
   the UF2 drive), or the dongle app directly.
3. Check that it enumerates as 1209:0002 "Dachshund60F Receiver", with both plug orientations and from a
   USB-C to USB-C cable (the Rd resistors).
4. Check that the LED shows red, green and blue, and that PAIR and RESET work.
5. Range and latency: measure them with the keyboard in phase 14, with the puck next to the keyboard
   and then plugged straight into a USB 3 port, to see the difference.

## Not done

- No case design yet. The case needs a light pipe over D1, button caps over SW1 and SW2, and a USB-C
  cutout. The M2 holes are a placeholder.
- 3D models: the HRO receptacle is a generated model, not HRO's (`hardware/3dmodels/`), and D1 uses KiCad's
  Würth PLCC4 3.2 × 2.8 mm RGB LED model, the same package size. Swap in the manufacturers' STEPs before
  final case work.
- A Zephyr board definition for the dongle (with the UF2 flash layout) is phase 14 work. Until then,
  CI builds the dongle app for `nrf52840dongle/nrf52840`.
- BOM: every soldered part except the SWD header has an MPN. LCSC numbers are filled in only where they
  were checked (J1, U1, R1, R2).
