"""Generate the dimensioned 3D models in this folder that KiCad has no model for.

Run with FreeCAD's own Python (it ships the Part module), from the repo root:

    "C:/Program Files/FreeCAD 1.1/bin/python.exe" hardware/3dmodels/gen_models.py

Writes:

- USB_C_Receptacle_HRO_TYPE-C-31-M-12.step
- JST_PH_S2B-PH-SM4-TB_standin.step

Each model is built in its KiCad footprint's frame, so the footprints place it
with zero offset and rotation: X is the footprint's x, Y is the footprint's
-y (KiCad's 3D Y points up the screen), and Z = 0 is the board's top face.
Dimensions come from the footprints' pads and fab outlines plus the
manufacturers' drawings; see README.md for what is approximate.

FreeCAD can't write colours without its GUI, so the script adds the STEP
colour entities itself after exporting.
"""

import os
import re
import sys

FREECAD = os.environ.get("FREECAD_DIR", r"C:\Program Files\FreeCAD 1.1")
sys.path += [os.path.join(FREECAD, "lib"), os.path.join(FREECAD, "bin")]

import FreeCAD  # noqa: E402
import Part  # noqa: E402

V = FreeCAD.Vector
HERE = os.path.dirname(os.path.abspath(__file__))

SHELL = (0.80, 0.80, 0.82)   # nickel-plated steel
TIN = (0.84, 0.84, 0.86)
GOLD = (0.86, 0.69, 0.30)
BLACK = (0.10, 0.10, 0.10)   # LCP insulator
IVORY = (0.93, 0.90, 0.80)   # natural nylon (JST PH housing)


# --- primitives, in footprint coordinates (y grows towards the front) -------

def box(x0, x1, y0, y1, z0, z1):
    """Box from footprint-coordinate ranges; flips y into KiCad's 3D frame."""
    return Part.makeBox(x1 - x0, y1 - y0, z1 - z0, V(x0, -y1, z0))


def obround(w, h, y0, y1, zc):
    """Stadium-section bar along y: w wide, h tall (h < w), centred on x = 0."""
    r = h / 2
    body = box(-w / 2 + r, w / 2 - r, y0, y1, zc - r, zc + r)
    for x in (-w / 2 + r, w / 2 - r):
        body = body.fuse(Part.makeCylinder(r, y1 - y0, V(x, -y1, zc), V(0, 1, 0)))
    return body.removeSplitter()


def post(x, y, d, z0, z1):
    return Part.makeCylinder(d / 2, z1 - z0, V(x, -y, z0), V(0, 0, 1))


def fuse(shapes):
    out = shapes[0]
    for s in shapes[1:]:
        out = out.fuse(s)
    return out.removeSplitter()


# --- HRO TYPE-C-31-M-12 ------------------------------------------------------

def hro_type_c_31_m_12():
    """16-pin top-mount USB-C receptacle (LCSC C165948).

    Footprint frame: the mating face is at y = +3.65, the SMD tails at
    y = -4.045, and the shell is the fab outline, 8.94 x 7.3 mm.
    """
    w, h, zc = 8.94, 3.26, 1.63
    front, back = 3.65, -3.65
    insulator_face = -1.9  # front face of the plastic body inside the shell

    # Shell: a stadium tube, open at the front, with the USB-C opening
    # (8.34 x 2.56 mm) and a cut-away at the rear bottom where the tails exit.
    shell = obround(w, h, back, front, zc).cut(obround(8.34, 2.56, back - 1, front + 1, zc))
    shell = shell.cut(box(-3.7, 3.7, back - 0.1, back + 0.65, -0.1, 0.4))
    # Through-hole shell legs on the sides, in the footprint's slots.
    legs = []
    for sx in (-4.32, 4.32):
        legs.append(box(sx - 0.15, sx + 0.15, 0.6, 1.5, -1.0, 0.4))     # front, 1.0 x 1.6 slot
        legs.append(box(sx - 0.15, sx + 0.15, -3.78, -2.48, -1.0, 0.4))  # rear, 1.0 x 2.1 slot
    shell = fuse([shell] + legs)

    # Insulator: fills the shell behind the cavity, carries the tongue and
    # the two locating posts for the 0.65 mm NPTH holes.
    insulator = fuse([
        obround(8.34, 2.56, back, insulator_face, zc),
        box(-3.345, 3.345, insulator_face - 0.01, 2.8, zc - 0.3, zc + 0.3),  # tongue, 6.69 x 0.6
        post(-2.89, -2.6, 0.55, -0.6, 0.4),
        post(2.89, -2.6, 0.55, -0.6, 0.4),
    ])

    # 12 contacts on each face of the tongue, 0.5 mm pitch; the outer GND
    # pair is longer so it mates first.
    contacts = []
    for i in range(12):
        x = -2.75 + 0.5 * i
        tip = 2.5 if i in (0, 11) else 2.3
        contacts.append(box(x - 0.125, x + 0.125, insulator_face, tip, zc + 0.3, zc + 0.35))
        contacts.append(box(x - 0.125, x + 0.125, insulator_face, tip, zc - 0.35, zc - 0.3))

    # SMD tails, one per pad (paired contacts share a pad).
    tails = []
    for x, wide in ((-3.25, 1), (-2.45, 1), (-1.75, 0), (-1.25, 0), (-0.75, 0), (-0.25, 0),
                    (0.25, 0), (0.75, 0), (1.25, 0), (1.75, 0), (2.45, 1), (3.25, 1)):
        hw = 0.2 if wide else 0.1
        tails.append(fuse([box(x - hw, x + hw, -4.5, -3.3, 0, 0.12),
                           box(x - hw, x + hw, -3.42, -3.3, 0, 0.4)]))

    return [(shell, SHELL), (insulator, BLACK)] + [(c, GOLD) for c in contacts + tails]


# --- JST PH S2B-PH-SM4-TB ----------------------------------------------------

def jst_ph_s2b_ph_sm4_tb():
    """2-pin JST PH side-entry SMD header.

    Footprint frame: the plug enters from +y (opening at y = 4.4), the signal
    tails run back to the pads at y = -2.85, and the metal fittings solder to
    the MP pads at x = +-3.35.
    """
    standoff = 0.15  # the fittings' feet sit under the housing
    top = 4.8

    housing = fuse([
        box(-3.95, 3.95, -1.6, 4.4, standoff, top),
        # Rear side guides, the fab outline's ears beside the signal tails.
        box(-3.95, -3.15, -3.2, -1.6, standoff, 2.5),
        box(3.15, 3.95, -3.2, -1.6, standoff, 2.5),
    ])
    housing = housing.cut(box(-2.3, 2.3, 0.0, 4.5, 0.8, 4.1))       # plug cavity
    housing = housing.cut(box(-0.8, 0.8, 2.6, 3.6, 4.0, top + 0.1))  # latch window
    for sx in (-3.65, 3.65):
        housing = housing.cut(box(sx - 0.1, sx + 0.1, 1.5, 4.1, standoff - 0.1, 3.9))  # fitting slots

    contacts = []
    for x in (-1.0, 1.0):
        contacts.append(fuse([
            box(x - 0.25, x + 0.25, -1.7, 3.4, 2.2, 2.7),      # 0.5 mm square post
            box(x - 0.32, x + 0.32, -2.0, -1.6, 2.35, 2.55),   # exit through the back wall
            box(x - 0.32, x + 0.32, -2.0, -1.8, 0.0, 2.55),    # bend down
            box(x - 0.32, x + 0.32, -4.3, -1.8, 0.0, 0.2),     # SMD tail
        ]))

    fittings = []
    for sx, foot in ((-3.65, (-4.0, -2.75)), (3.65, (2.75, 4.0))):
        fittings.append(fuse([
            box(sx - 0.1, sx + 0.1, 1.6, 4.0, 0.0, 3.8),
            box(foot[0], foot[1], 1.5, 4.3, 0.0, standoff),
        ]))

    return [(housing, IVORY)] + [(c, TIN) for c in contacts + fittings]


# --- STEP output -------------------------------------------------------------

def write_step(parts, name, path):
    solids = [s for shape, _ in parts for s in shape.Solids]
    colours = [c for shape, c in parts for _ in shape.Solids]
    Part.Compound(solids).exportStep(path)
    text = open(path, encoding="utf-8").read()

    # OpenCASCADE writes the solids in compound order, each followed by its
    # own geometry. Check that by testing each brep's first vertex against its
    # solid before colouring it.
    breps = [int(m.group(1)) for m in re.finditer(r"^#(\d+) = MANIFOLD_SOLID_BREP\(", text, re.M)]
    if len(breps) != len(solids):
        raise SystemExit(f"{name}: {len(solids)} solids but {len(breps)} breps in the STEP")
    for sid, solid in zip(breps, solids):
        m = re.search(rf"^#{sid} = MANIFOLD_SOLID_BREP.*?VERTEX_POINT\('',#(\d+)\)", text, re.M | re.S)
        p = re.search(rf"^#{m.group(1)} = CARTESIAN_POINT\('',\(([^)]*)\)\)", text, re.M)
        x, y, z = (float(v) for v in p.group(1).split(","))
        if solid.distToShape(Part.Vertex(V(x, y, z)))[0] > 1e-4:
            raise SystemExit(f"{name}: STEP solid order doesn't match")

    ctx = re.search(r"^#(\d+) = \( GEOMETRIC_REPRESENTATION_CONTEXT", text, re.M).group(1)
    nid = max(int(i) for i in re.findall(r"^#(\d+) =", text, re.M)) + 1
    lines, styled = [], []
    palette = {}
    for c in sorted(set(colours)):
        palette[c] = nid
        lines += [f"#{nid} = COLOUR_RGB('',{c[0]:.3f},{c[1]:.3f},{c[2]:.3f});",
                  f"#{nid + 1} = FILL_AREA_STYLE_COLOUR('',#{nid});",
                  f"#{nid + 2} = FILL_AREA_STYLE('',(#{nid + 1}));",
                  f"#{nid + 3} = SURFACE_STYLE_FILL_AREA(#{nid + 2});",
                  f"#{nid + 4} = SURFACE_SIDE_STYLE('',(#{nid + 3}));",
                  f"#{nid + 5} = SURFACE_STYLE_USAGE(.BOTH.,#{nid + 4});",
                  f"#{nid + 6} = PRESENTATION_STYLE_ASSIGNMENT((#{nid + 5}));"]
        nid += 7
    for sid, c in zip(breps, colours):
        lines.append(f"#{nid} = STYLED_ITEM('color',(#{palette[c] + 6}),#{sid});")
        styled.append(f"#{nid}")
        nid += 1
    lines.append(f"#{nid} = MECHANICAL_DESIGN_GEOMETRIC_PRESENTATION_REPRESENTATION("
                 f"'',({','.join(styled)}),#{ctx});")
    text = text.replace("ENDSEC;\nEND-ISO-10303-21;", "\n".join(lines) + "\nENDSEC;\nEND-ISO-10303-21;")

    # Name the product after the part, and drop the export timestamp so a
    # rerun with unchanged geometry leaves the file unchanged.
    text = re.sub(r"PRODUCT\('[^']*',\s*'[^']*'", f"PRODUCT('{name}','{name}'", text, count=1)
    text = re.sub(r"FILE_NAME\('[^']*','[^']*'", f"FILE_NAME('{name}','2026-01-01T00:00:00'", text, count=1)
    open(path, "w", encoding="utf-8", newline="\n").write(text)
    bb = Part.Compound(solids).BoundBox
    print(f"{os.path.relpath(path)}: {len(solids)} solids, "
          f"x {bb.XMin:.2f}..{bb.XMax:.2f}, y {bb.YMin:.2f}..{bb.YMax:.2f}, z {bb.ZMin:.2f}..{bb.ZMax:.2f}")


def main():
    for name, parts, filename in (
        ("HRO TYPE-C-31-M-12", hro_type_c_31_m_12(), "USB_C_Receptacle_HRO_TYPE-C-31-M-12.step"),
        ("JST S2B-PH-SM4-TB", jst_ph_s2b_ph_sm4_tb(), "JST_PH_S2B-PH-SM4-TB_standin.step"),
    ):
        for shape, _ in parts:
            if not shape.isValid():
                raise SystemExit(f"{name}: invalid solid")
        write_step(parts, name, os.path.join(HERE, filename))


if __name__ == "__main__":
    main()
