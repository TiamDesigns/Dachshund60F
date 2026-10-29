# Dachshund60F case BOM

Everything that goes into the case and holds it together, for the default layout (split spacebar). The PCBs
are line items only; their parts are in each board's own BOM (`hardware/<board>/`). Counts follow
`gen_case.py`: if you change `SCREWS`, `DOWELS`, `FOOT_X` or `WEIGHT_SCREWS`, change them here too.

## Made parts

| Qty | Part | Material, process | File |
|---:|---|---|---|
| 1 | Top case | 6061 aluminium, 3-axis CNC, bead-blast and anodise | `step/Dachshund60F-Case-Top.step` |
| 1 | Bottom case | 6061 aluminium, 3-axis CNC, bead-blast and anodise | `step/Dachshund60F-Case-Bottom.step` |
| 1 | Side panel (right end, over the antenna) | Polycarbonate, CNC | `step/Dachshund60F-Case-Panel.step` |
| 1 | Power switch cap (slider and grip) | Polycarbonate or nylon, CNC or printed | `step/Dachshund60F-Switch-Cap.step` |
| 1 | Roller, 19 mm × 21 mm, blind bore for a 6 mm D-shaft | Aluminium, turned, knurled | `Roller` in `step/Dachshund60F-Case-Assembly.step` |
| 1 | Wheel push cap, 11 mm disc | Amber acrylic or polycarbonate, glued in | `Wheel cap` in the assembly |
| 1 | Plate foam, 3.5 mm | Poron, cut | `step/Dachshund60F-Plate-Foam.dxf` |
| 1 | LED seal, 1.5 mm | Black closed-cell foam (EVA or Poron), cut | `step/Dachshund60F-LED-Seal.dxf` |
| 1 | LED light guide | Opaque PETG or ASA (black, or white for brighter dots), 3D printed | `step/Dachshund60F-LED-Guide.step` |
| 5 | LED plugs (diffusers), 2.9 mm shaft on a 4 mm head | Clear resin, printed, or turned from 4 mm acrylic rod; tops frosted | `step/Dachshund60F-LED-Plugs.step` |
| 1 | Weight, 80 × 64 mm, stepped 3–10 mm thick, about 285 g | Brass (C360), CNC, engraved 0.3 mm (0.6 mm cutter), brushed | `step/Dachshund60F-Weight.step` |

## Bought parts

| Qty | Part | Notes |
|---:|---|---|
| 8 | M3 × 10 socket head cap screw (ISO 4762), stainless or black | Bottom case to top case, four along the front and four along the back, into tapped M3 holes |
| 2 | M3 × 6 button head screw (ISO 7380) | USB-C daughterboard into tapped holes in the bottom case |
| 4 | M3 × 5 button head screw (ISO 7380) | The weight, from inside the bottom case into tapped holes in it |
| 2 | Dowel pin, 3 mm m6 × 8 mm, hardened steel (ISO 8734) | Pressed into the bottom case (2.98 mm hole), sliding into the top (3.05 mm, one slotted) |
| 4 | SKUF silicone feet, 41.8 × 5.8 × 2 mm, 3M 9488A adhesive | In 1 mm pockets, stand 1 mm proud. Keebio (8 for about $3) or splitkb (4 for about €2.75) |
| 8 | Gasket socks, silicone, 8 × 5 × 5 mm, D-shaped, 6.5 × 2.8 mm slot | One on each plate tab, seated 1 mm into the bottom case |
| 1 | LiPo cell 803450, 8 × 34 × 50 mm, about 1500 mAh, with protection board and a JST-PH 2-pin lead | In the tray on the floor. JST-PH polarity varies between vendors: check it against J2 |
| 1 | E-paper panel, GoodDisplay GDEY0213B74 (2.13", 250 × 122) | Taped under the roof's window |
| 1 | Adafruit eInk Breakout Friend (#4224) | Taped to the panel's back |
| 1 | Double-sided tape, 0.2 mm (3M 9448A or tesa 4965) | Screen glass to the roof, and the Friend to the panel |
| 1 | JST-SH 4-pin cable, about 200 mm, both ends plugged | USB-C daughterboard to J1 |
| 79 | MX-style switches | Hot-swap |
| 5 | 2 u screw-in (PCB-mount) stabilizers | Backspace, Enter, left Shift, the 2.75 u space and the 2.25 u right Mod. Full 6.25 u spacebar instead: one 6.25 u stabilizer for the space |
| 1 | Keycap set, Cherry profile | Needs the split spacebar's 1.25 u Mod, 2.75 u space and 2.25 u Mod (or a 6.25 u space) |

## PCBs

| Qty | Board | Folder |
|---:|---|---|
| 1 | Mainboard | `hardware/Dachshund60F/` |
| 1 | USB-C daughterboard | `hardware/Dachshund60F-USB/` |
| 1 | Switch plate (FR4) | `hardware/Dachshund60F-Plate/` |
| 1 | 2.4 GHz dongle (separate, not in the case) | `hardware/Dachshund60F-Dongle/` |
