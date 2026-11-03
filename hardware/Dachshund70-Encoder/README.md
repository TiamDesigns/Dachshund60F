# Dachshund70 encoder module

The roller encoder on its own small board, cabled to the mainboard. Introduced with mainboard Rev 1.1,
which no longer carries the encoder. KiCad 10 project, 2 layers, 1.6 mm FR4.

- **Encoder:** SW1, Bourns PEC12R-2220F-S0024 (shaft parallel to the board, 24 detents, 24 pulses per turn,
  20 mm shaft, push switch pressed along the shaft; LCSC C49673168, which showed no stock on 2026-10-28). Same
  body and footprint as the old SW3 (which was the 30 mm PEC12R-2230F-S0024), with the shorter shaft
  (`../3dmodels/Bourns_PEC12R-2x20F-S_standin.step`). Through-hole; its two frame tabs are on GND.
- **Cable port:** J1, JST SH 1.0 mm, 5 pins, side entry (SM05B-SRSS-TB), on the **bottom** side with the cable
  leaving at the -u edge. It plugs into the mainboard's J5 with a straight (pin 1 to pin 1) cable: a pre-made
  5-pin 1.0 mm SH cable, or SHR-05V-S housings with SSH-003T contacts.
- **Nothing else.** The 10 nF debounce capacitors on A and B and the push-switch diode D114 stay on the
  mainboard, so the pin map and the firmware did not change. No expansion ports on either board.

| J1 / J5 pin | Net | Encoder pad | MCU pin |
| --- | --- | --- | --- |
| 1 | COL14 | S1 (push switch) | matrix column 14, with diode D114 on the mainboard |
| 2 | ENC_A | A | P0.11 |
| 3 | GND | C (common) | |
| 4 | ENC_B | B | P0.12 |
| 5 | ENC_SW | S2 (push switch) | matrix row 0 through D114 |

## Geometry

Module coordinates: u runs along the shaft, toward the hub end of the board (+x of the mainboard's old
encoder); v runs across it (+y); (0, 0) is the board's top-left corner seen from above. Z is measured up
from the PCB's top face.

- Outline 22.5 (u) × 22.0 (v) mm, 1.5 mm corner radius. The right edge is the hub end of the shaft; the wheel
  starts there and the board has nothing under it.
- Encoder pad C at (15.0, 11.0). Body rear face u = 11.5, front face u = 17.5, hub to u = 22.5, shaft tip at
  u = 31.5 (20 mm shaft). Shaft axis at v = 11.0 and 10.0 mm above the PCB top.
- Two mounting holes, 2.2 mm (M2), at (2.7, 2.7) and (2.7, 19.3). The 12.5 mm-wide encoder body spans
  v 4.75 to 17.25.
- J1's pad row at u = 5.3, v = 9 to 13 (pin 1 at v = 13); its courtyard is u 0.0 to 6.6, v 6.6 to 15.4.
  The cable leaves toward -u.
- On the mainboard, J5 is on the back to the right of the corner notch: pad row at x = 331.8 mm, y = 44.5
  (pin 5) to 48.5 (pin 1), body toward -x (courtyard about x 326.9 to 333.1, y 41.9 to 51.1), cable leaving
  toward -x into the notch. The notch is open to the mainboard's top edge, x 304.0 to 326.4 mm and y 39.0 to
  57.0 mm, so the module (left edge at x 303.905) sits in it, with a 4.2 mm lobe at (306.6, 57.8) for the
  head of its lower M2 screw (its post is at y 57.82); the upper screw head is inside the notch.

## Status

DRC (kicad-cli, with schematic parity): 0 errors and 0 unconnected items. The only messages are the
library-footprint mismatch on SW1 (the board copy of the footprint is the project's own, like the mainboard's)
and the missing MPN fields that the mainboard also shows. ERC: 0. Not ordered, made or fitted.

LCSC part numbers are blank: the encoder has no LCSC listing (see PROGRESS.md, 2026-10-28), and J1's number
is still to be confirmed, so JLC would source both globally or you solder the encoder by hand.
