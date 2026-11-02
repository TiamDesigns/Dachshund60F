# Dachshund60F encoder module

The roller encoder on its own small board, cabled to the mainboard. Introduced with mainboard Rev 1.1,
which no longer carries the encoder. KiCad 10 project, 2 layers, 1.6 mm FR4.

- **Encoder:** SW1, Bourns PEC12R-2230F-S0024 (shaft parallel to the board, 24 detents, 24 pulses per turn,
  30 mm shaft, push switch pressed along the shaft). Same footprint and 3D model as the old SW3
  (`../3dmodels/Bourns_PEC12R-2x30F-S_standin.step`). Through-hole; its two frame tabs are on GND.
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
  u = 41.5. Shaft axis at v = 11.0 and 10.0 mm above the PCB top.
- Two mounting holes, 2.2 mm (M2), at (2.7, 2.7) and (2.7, 19.3). The 12.5 mm-wide encoder body spans
  v 4.75 to 17.25.
- J1's pad row at u = 5.3, v = 9 to 13 (pin 1 at v = 13); its courtyard is u 0.0 to 6.6, v 6.6 to 15.4.
  The cable leaves toward -u.
- On the mainboard, J5 is on the back at x = 313.4 mm, y = 48.0 (pin 5) to 52.0 (pin 1), body toward -x
  (courtyard x 308.1 to 314.7, y 45.6 to 54.4), cable leaving toward -x. The mainboard has no slot for the
  cable; it reaches the module around the board edge unless the case needs one.

## Status

DRC (kicad-cli, with schematic parity): 0 errors and 0 unconnected items. The only messages are the
library-footprint mismatch on SW1 (the board copy of the footprint is the project's own, like the mainboard's)
and the missing MPN fields that the mainboard also shows. ERC: 0. Not ordered, made or fitted.

LCSC part numbers are blank: the encoder has no LCSC listing (see PROGRESS.md, 2026-10-28), and J1's number
is still to be confirmed, so JLC would source both globally or you solder the encoder by hand.
