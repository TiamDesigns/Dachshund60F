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
- **Encoder:** the plate stops short of the roller (SW3). A notch takes out the top-right corner,
  x ≥ 306.0 mm and y ≤ 59.0 mm, with a 2 mm inside radius. That clears the body (its left face at
  x ≈ 308.15 mm), the 20 mm shaft pointing right (tip at x ≈ 331.3 mm) and a wheel on it, and leaves a 2.8 mm web next to the Delete cutout. Supporting the roller is up to the
  case.
- **Display header:** a slot over J4 (the courtyard plus 0.2 mm), for the 8.5 mm-tall pin header.
- **Indicator LEDs:** 4 × 4 mm windows over D3–D7, the layer and battery LEDs. The Caps LED (D2) sits
  inside its switch cutout, so it needs no window.

## Gasket tabs

Eight tabs, four on the top edge and four on the bottom, at x = 58.81, 138.44, 218.06 and 297.69 mm
(at ⅛, ⅜, ⅝ and ⅞ of the length). There are none on the ends. Each tab is 2.8 mm wide and 6.5 mm long,
the size of the silicone sock's pocket (8 × 5 × 5 mm outside, 6.5 × 2.8 × 1.2 mm inside, fits 1.2–1.6 mm
plates). A tab fills the whole pocket, so the sock's open end sits against the plate edge. Tab roots have
a 0.5 mm radius and tips a 1 mm radius.

The sock outlines (8 × 5 mm) are drawn on Cmts.User, so the case pockets can be placed from them. Each
sock reaches 8 mm past the plate edge, at y = 31.0 mm on the top edge and y = 168.1 mm on the bottom.

Feel: short tabs with plain roots (no relief cuts beside them), on FR4, give a moderate bounce. To soften
it, drop to six tabs (remove the ⅜ and ⅝ pairs) or add flex cuts. To firm it up, add a tab on each end.
The tab count and positions are `TAB_FRACTIONS` in the script. If the socks feel loose, widen
`TAB_WIDTH` to 3.0 mm.

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
