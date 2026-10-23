# Shared 3D models

Models the stock KiCad 10 library doesn't have, used by more than one board.
Footprints reference them as `${KIPRJMOD}/../3dmodels/<file>`.

- `USB_C_Receptacle_HRO_TYPE-C-31-M-12.step`: a stand-in, not the
  manufacturer's model. It is an 8.94 × 3.26 mm obround shell, 7.3 mm deep,
  sitting on the board surface. These are the receptacle's outside dimensions,
  so it works for the case cutout, but it has no pins or inner detail. The
  footprints place it with a 90° X rotation and a (0, 3.65, 1.63) mm offset.
  To replace it, download HRO's STEP (LCSC C165948), save it under this name,
  and adjust the footprint's offset and rotation to match.
- `Bourns_PEC12R_Horizontal_standin_{body,bushing,shaft}.step`: a stand-in
  for the horizontal-shaft (mouse-wheel) roller encoder SW3. Its plan view
  comes from KiCad's `RotaryEncoder_Bourns_Horizontal_PEC12R-2x17F-Sxxxx`
  footprint outline:
  - body: an 8.4 × 12.5 mm envelope, 13.4 mm tall
  - bushing: Ø7 × 5 mm
  - shaft: Ø6 mm, ending 17 mm from the footprint origin, which is the
    footprint's 17 mm shaft plus 3 mm for the 20 mm part

  The body height and the 6.7 mm shaft-axis height are assumed, not taken
  from the datasheet. The shaft's flat isn't modelled. Replace these with
  Bourns' STEP (componentsearchengine.com, needs a free account).
- `JST_PH_S2B-PH-SM4-TB_standin.step`: a 7.9 × 6.0 × 4.8 mm box over the
  footprint's housing outline, for the battery connector J2. Replace it with
  JST's STEP (jst-mfg.com, emailed after you accept their licence). JST's
  licence forbids passing the file on, so don't commit it to a public repo.
