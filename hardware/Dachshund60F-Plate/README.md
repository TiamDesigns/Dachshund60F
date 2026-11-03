# Dachshund60F switch plate

A 1.6 mm FR4 plate for gasket mounting. It is ordered as an ordinary 2-layer PCB with no copper on
B.Cu. The top has an exposed-copper accent ring around every switch cutout.

`Dachshund60F-Plate.kicad_pcb` is generated. Don't edit it by hand: change the parameters at the top of
`gen_plate.py` and rerun it with KiCad's Python, from the repo root:

```
"C:/Program Files/KiCad/10.0/bin/python.exe" hardware/Dachshund60F-Plate/gen_plate.py
```

The script reads every switch, stabilizer, the encoder, J4 and the indicator LEDs from
`hardware/Dachshund60F/Dachshund60F.kicad_pcb`. Rerun it after the mainboard changes, so the two boards
can't drift apart.

## Geometry

- **Outline:** the mainboard outline, 318.5 × 121.1 mm with 5 mm corners. The plate has no mounting
  holes, and neither does the PCB: the PCB hangs from the switches.
- **Switches:** 80 cutouts, 14 × 14 mm with 0.5 mm corner radii (a router can't cut a sharp inside
  corner). The cutouts have no side notches for opening switches in the plate.
- **Stabilizers:** 7.0 × 14.7 mm, from 6.5 mm above the key centre to 8.2 mm below, centred on the
  footprints' stabilizer positions (11.938 mm out for 2u). These are for PCB-mount (screw-in) stabilizers
  of any brand: Cherry's 6.65–6.75 × 12.3 mm cutout plus its bottom notch, with 0.125–0.175 mm extra on
  each side, about 1 mm at the top and 0.23 mm below the notch. Plate-mount (clip-in) stabilizers won't
  latch in these, and Costar stabilizers aren't supported. The size is `STAB_CUTOUT_*` in the script.
- **Spacebar row:** both layouts are cut, so either one can be built. Where they overlap, the holes
  merge into one slot: the split 2.75u and the 6.25u switch cutouts become one long slot, and the 6.25u
  stabilizer's left cutout sits inside the 1.25u Mod cutout. The switches in the merged slot (SW604,
  SW606 and SW615) are the soldered ones, so the PCB holds them. The hot-swap 2.25u Mod (SW608) keeps a
  full cutout.
- **Encoder:** the plate stops short of the roller. A notch takes out the top-right corner,
  x ≥ 306.0 mm and y ≤ 59.0 mm, with a 2 mm inside radius. That clears the body (its left face at
  x ≈ 308.15 mm), the 20 mm shaft pointing right (tip at x ≈ 331.3 mm) and a wheel on it, and leaves a 2.8 mm web next to the Delete cutout. Supporting the roller is up to the
  case. Since mainboard Rev 1.1 the encoder is on its own module board, so `gen_plate.py` no longer reads
  it from the mainboard: the notch keeps the bottom edge the old on-board encoder gave it (`ENCODER_BOX`),
  and the output is unchanged.
- **Display header:** a slot over J4 (the courtyard plus 0.2 mm), for the 8.5 mm-tall pin header.
- **Indicator LEDs:** 4 × 4 mm windows over D3–D7, the layer and battery LEDs. The Caps LED (D2) sits
  inside its switch cutout, so it needs no window.

## Gasket nubs

Six branched nubs, three on the top edge (the keyboard's back) and three on the bottom (its front), none on the
ends. Each nub is one 4 mm neck that leaves the plate edge and splits into two arms running back along the edge in
opposite directions (a T). A 2.0 mm slot separates each arm from the plate edge, so an arm is a cantilever held only
through the neck and the material flexes. An arm is 1.5 mm wide (in the plane, the direction it bends) and 12 mm long
from the neck's side face; with 1.6 mm FR4 that is roughly 15 N/mm at the prong (3EI/L³, E ≈ 20 GPa), and the
polycarbonate plate is softer. The free end of each arm carries one prong, 2.8 mm wide and 6.5 mm long, the size of
the silicone sock's pocket (8 × 5 × 5 mm outside, 6.5 × 2.8 × 1.2 mm inside, fits 1.2–1.6 mm plates). The prongs run
on along the plate edge, parallel to it and to the arms (they no longer point outward), so every gasket lies along the
edge and a nub reaches only 4.15 mm past it (the socks, centred on the arm, reach 5.25 mm). That is what fits the case's
bezel. The sock's open end sits against the arm's end, 14 mm from the nub centre. Twelve gaskets on six nubs.
Everything is outside the plate outline, so no switch cutout is touched. Inside corners (neck to slot) have a 0.5 mm
radius and outside corners 0.7 mm (the rounding would erase an arm thinner than twice the outside radius).

Nub centres (mm from the middle of the plate, x = 0 at the USB-C connector on the back edge), prong centres at ±(2 + 12 + 3.25)
= ±17.25 mm from each:

| Edge | Nub centres |
| --- | --- |
| Bottom (front) | −106.17, 0, +106.17 (even thirds, symmetric) |
| Top (back) | −107.7, −15.0, +77.7 |

The back can't be symmetric: the encoder notch takes out the right-hand end of the top edge (plate edge to
x ≈ +125), and the USB-C connector is at x = 0, so the three nubs are spread evenly over the edge that is left,
with the middle one 15 mm left of the connector. In mainboard coordinates the nubs are at x = 70.55, 163.25, 255.95
on the top edge (y = 39.0), with prongs at 53.30, 87.80, 146.00, 180.50, 238.70, 273.20, and at x = 72.08, 178.25,
284.42 on the bottom edge (y = 160.1), with prongs at 54.83, 89.33, 161.00, 195.50, 267.17, 301.67.

The sock outlines (8 × 5 mm) are drawn on Cmts.User, so the case pockets can be placed from them. Each sock starts at
the arm's end and lies 8 mm along the plate edge, centred 2.75 mm past it (0.25 to 5.25 mm past the edge). The case needs a clearance pocket round
each nub (about 0.5 mm round the neck and arms) so the arms can move.

Feel: set by the arm width and length. To soften it, make `ARM_WIDTH` smaller or `ARM_LENGTH` longer; to firm it up,
the opposite. The nub centres are `FRONT_NUBS` and `BACK_NUBS`, the neck `NECK_WIDTH`, the slot `SLOT`, in the script.
If the socks feel loose, widen `TAB_WIDTH` to 3.0 mm.

## Polycarbonate plate

A polycarbonate plate is cut from the same outline as the FR4 one: `Dachshund60F-Plate-outline.dxf` is the plate's
Edge.Cuts (outline with the six branched nubs, their twelve prongs and the post reliefs, and every switch, stabilizer, LED and display
cutout), in mm, exported with `kicad-cli pcb export dxf --layers Edge.Cuts --output-units mm --mode-single -o Dachshund60F-Plate-outline.dxf Dachshund60F-Plate.kicad_pcb`. Regenerate it after any change to the
plate (rerun `gen_plate.py` first). The copper accents exist only on the FR4 plate.

## Encoder module post reliefs

The encoder module's two M2 posts stand at mainboard (307.2, 41.7) and (307.2, 58.3), only 1.2 mm from the
notch edge. Each has a Ø5.2 mm relief in the plate (`POST_RELIEFS`). The one by the top edge is a slot open to
the edge, because a plain circle would leave a 0.1 mm sliver of FR4 between it and the edge. The web left between
the relief and the Delete cutout is 1.4 mm.

## Copper accents

An accent ring follows every switch and stabilizer cutout: it starts 0.45 mm outside the cutout and is
0.7 mm wide. A stabilizer cutout is only 1.44 mm from its switch cutout, which is too close for two
separate rings. Instead, copper fills that web, and one outline wraps the switch and both stabilizers.
The merged spacebar slot gets one ring around the whole slot. The solder mask opening is 0.05 mm inside
the copper, so the mask edge draws the line. All copper stays at least 0.4 mm from any routed edge (the
board's copper-to-edge rule), and any copper island under 2 mm² is dropped.

The sidebar (encoder, display, LEDs) has no copper, so nothing on the plate sits near the module's antenna.

## Ordering (JLCPCB)

- FR4, 2 layers, 1.6 mm. Pick the mask colour you want. **ENIG** gives gold accents that stay bright.
  Lead-free HASL is cheaper but silver and slightly lumpy.
- No PCBA. Silkscreen is on the bottom only.
- `JLCJLCJLCJLC` on the bottom silkscreen marks where JLCPCB prints its order number, so it doesn't
  land on the top.
- Expect the design check to ask about the big cutouts and the merged spacebar slot. That's normal for
  plates.

## Checks

- kicad-cli DRC: 0 violations, 0 unconnected items.
- Not checked against real switches yet. On the first plate, check that MX switches clip in firmly at
  1.6 mm, that screw-in stabilizers (Swagkeys Knight, plus whatever else is to hand) move freely in their cutouts, that the socks fit the tabs snugly, and that the J4 slot and LED
  windows line up with the PCB.
