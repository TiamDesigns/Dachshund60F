# Dachshund70 case BOM (rev 2, Fusion model v33)

Everything that goes into the case and holds it together, for the default layout (split spacebar). The PCBs
are line items only; their parts are in each board's own BOM (`hardware/<board>/`). Sizes are in mm, from the
Fusion model; masses use 2.70 g/cm³ for aluminium. Finish and anodise colours are Tiam's choice.

## Made parts

| Qty | Part | Material | Size, volume | Process | File |
|---:|---|---|---|---|---|
| 1 | Top block | 6061-T6 aluminium | 347.9 × 155.2 × 24.5, 173.9 cm³ (about 470 g) | CNC 3-axis (5-axis or a ball-end for the slopes), bead-blast, anodise | `step/Dachshund70-Case-Top.step` |
| 1 | Bottom block | 6061-T6 aluminium | 347.9 × 155.2 × 15.1, 310.1 cm³ (about 837 g) | CNC, bead-blast, anodise | `step/Dachshund70-Case-Bottom.step` |
| 1 | Accent ring | 6061 or 5052 aluminium sheet, 1.6 thick | outline 347.9 × 155.2, opening 320 × 122.6, 23.1 cm³ (about 62 g) | Laser cut from flat sheet, anodise in the accent colour | `step/Dachshund70-Case-Ring.step`, `dxf/Dachshund70-Case-Ring.dxf` |
| 1 | Weight | C360 brass (steel, aluminium or titanium possible) | face 185.0 × 30.6 (R4), flange 216.0 × 46.5 (R5), 5.1 / 6.4 / 7.7 thick, 38.95 cm³: brass 331 g, steel 306 g, aluminium 105 g, titanium 173 g | CNC; polished face with a 0.4 chamfer round the edge; dachshund silhouette 0.3 deep (flat floor bead-blasted), 0.4 wide detail grooves 0.5 deep, "Dachshund70" text 0.3 deep (Fredoka SemiBold, SIL OFL) | `step/Dachshund70-Case-Weight.step`, `dxf/Dachshund70-Weight-Engraving.dxf` |
| 1 | Knob | 6061 aluminium | Ø19.2 × 15.1, 3.7 cm³ (10 g) | CNC, knurl (64 flutes, 14 rings), anodise | `step/Dachshund70-Case-Knob.step` |
| 1 | Encoder cover | 6061 aluminium | 13.3 × 19.2 × about 17, 1.3 cm³ (3.6 g) | CNC, anodise in the accent colour | `step/Dachshund70-Case-Encoder-Cover.step` |
| 1 | Power switch slider cap | 6061 aluminium, anodised accent colour | 3.4 × 2.0 stadium, 2.2 long | CNC (or Swiss turn), anodise | `step/Dachshund70-Case-Switch-Cap.step` |
| 1 | LED light guide | Opaque black SLA or SLS resin | 30.8 × 6.6 × 10.4, 0.8 cm³ | 3D print | `step/Dachshund70-Case-LED-Guide.step` |

Notes on the made parts:

- Weight: it drops in from inside through a window in the bottom block, its 1.5 mm flange resting on a stepped
  ledge pocket (ledges at z 3.68 front, 5.01 middle, 6.34 back, matching the floor's steps), and sits 0.1 under
  the floor. Four M3 × 5 countersunk screws from inside, at x ±103.5 and y 2.5 / 22.5 (local frame), go into
  taps Ø2.5 × 3.8 deep in the ledges. No screw heads show on the bottom.
- Knob: blind shaft bore Ø6.3 (ends 0.3 past the 20 mm shaft's tip) behind a Ø7.6 × 4.9 hub bore; fixed with a
  set screw or a press fit on the shaft.
- Encoder cover: walls tangent to the knob, end corners R1.5, cavity 14 wide for the Bourns PEC12R body.
- Switch cap: a light press fit on the PCM12 lever (pocket 0.05 under its size) plus a drop of two-part epoxy
  or cyanoacrylate; it stands 0.3 proud of the back face and rides in a stadium slot 5.3 × 2.4 (0.2 clear all
  round, 1.5 travel). Three ribs 0.3 wide × 0.3 deep.
- Light guide: one bore per LED (five), sealing spigots into the plate's LED windows.
- Ring: every wall square to the sheet; 8 × Ø3.4 screw holes and 12 joint slots 12 × 1.5.

## Bought parts

| Qty | Part | Notes |
|---:|---|---|
| 4 | M3 × 12 countersunk, DIN 7991, stainless | Bottom block to top block, along the front, flush with the bottom face; taps in the top block 7.3 deep |
| 4 | M3 × 20 countersunk, DIN 7991, stainless | The same along the back (the block is 23 mm tall there); taps 6.2 deep |
| 4 | M3 × 5 countersunk, stainless | The weight, from inside, into the ledge taps (above) |
| 2 | M3 × 6 button head (ISO 7380) | USB-C daughterboard into the two taps on its ledge |
| 2 | M2 pan head | Encoder module into the two posts in the top block (length to be chosen with the module: boss tap 5.0 deep); the heads clear the board through the notch and the lobe |
| 2 | Rubber or TPU rail with adhesive, 300 × 5.8 × 2.0 | Near the front and back edges, in 1.0 pockets, 1.0 proud |
| 12 | Gasket socks, silicone, 8 × 5 × 5 outside, pocket 6.5 × 2.8 × 1.2 | Moulded (stock). One on each of the plate's prongs, lying along the plate edge |
| 1 | LiPo cell EEMB LP803450, 52.0 × 34.5 × 8.3 plus a 4.5 protection board | Flat in the battery bay (pocket 35.1 × 57.1) under the screen and knob, long side front to back. JST-PH polarity varies between vendors: check it against J2 |
| 1 | E-paper panel, GoodDisplay GDEY0213B74 (2.13", 250 × 122) with driver board | Taped under the roof's window |
| 1 | Double-sided tape, 0.2 mm (3M 9448A or tesa 4965) | Screen glass to the roof |
| 1 | JST-SH 4-pin cable, about 100 mm, both ends plugged | USB-C daughterboard to J1 |
| 1 | Power slide switch PCM12SMTR (on the USB-C daughterboard) | Its lever takes the slider cap |
| 79 | MX-style hot-swap switches | 77 with the 6.25 u spacebar |
| 5 | 2 u screw-in (PCB-mount) stabilizers | Backspace, Enter, left Shift, the 2.75 u space and the 2.25 u right Mod. With the 6.25 u spacebar: one 6.25 u stabilizer for the space, plus Backspace, Enter and left Shift |
| 1 | Keycap set, Cherry profile | Needs the split spacebar's 1.25 u Mod, 2.75 u space and 2.25 u Mod (or a 6.25 u space) |

## PCBs

| Qty | Board | Folder |
|---:|---|---|
| 1 | Mainboard | `hardware/Dachshund70/` |
| 1 | USB-C daughterboard | `hardware/Dachshund70-USB/` |
| 1 | Encoder module | `hardware/Dachshund70-Encoder/` |
| 1 | Switch plate (FR4) | `hardware/Dachshund70-Plate/` |
| 1 | 2.4 GHz dongle (separate, not in the case) | `hardware/Dachshund70-Dongle/` |
