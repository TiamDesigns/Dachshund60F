"""Cherry-profile keycaps for the Dachshund60F, placed on the board in the case's desk frame.

    python hardware/case/keycaps.py            # default layout (split spacebar)
    python hardware/case/keycaps.py --full     # the 6.25 u spacebar instead

Needs CadQuery. Writes hardware/case/step/Dachshund60F-Keycaps.step, one named part per key, in the same
frame as Dachshund60F-Case-Assembly.step, so the two import on top of each other. The keys come from
boards/Dachshund60F/keyboard.json; the case's own numbers (key grid, tilt, keycap bottom) from gen_case.py.

The profile is Cherry as GMK makes it, with the numbers from KeyV2's cherry.scad (rsheldiii/KeyV2):
18.16 mm at the bottom, a top 6.31 mm narrower and 3.52 mm shallower, set 2 mm back, a 0.65 mm
cylindrical dish, and per row a height and a top tilt. Rows are as in a GMK kit: the F row and the
number row are R1, then R2, R3 (home), and R4 for Shift and the bottom row. They're stand-ins to look at
and check clearances with, not a model to mould from: no legends, no draft, and a plain cross stem.
"""

import json
import math
import os
import sys

import cadquery as cq
from OCP.Interface import Interface_Static

import gen_case as g

BOARD_JSON = os.path.join(os.path.dirname(g.HW), "boards", "Dachshund60F", "keyboard.json")
OUT = os.path.join(g.OUT, "Dachshund60F-Keycaps.step")

U = 19.05
BOTTOM = 18.16                    # 1 u at the skirt
TOP_DW, TOP_DD = BOTTOM - 11.85, BOTTOM - 14.64  # top is this much narrower, shallower
TOP_SKEW = 2.0                    # top centre this far behind the bottom's
DISH_DEPTH = 0.65                 # cylindrical, across the key
R_BOTTOM, R_TOP = 1.0, 1.5        # plan corner radii
WALL, ROOF = 1.2, 1.5             # wall and top thickness of the hollow cap
# Row: (height at the top's centre above the skirt, top tilt in degrees; positive raises the front edge).
ROWS = {"R1": (9.8, 0.0), "R2": (7.45, 2.5), "R3": (6.55, 5.0), "R4": (7.35, 11.5)}
ROW_OF = {0: "R1", 1: "R1", 2: "R2", 3: "R3", 4: "R4", 5: "R4"}  # keyboard.json row -> profile row

# MX stem: a 5.5 mm post with a cross for the switch's stem, stopping STEM_INSET above the skirt.
STEM_D, CROSS, CROSS_T, STEM_INSET = 5.5, 4.1, 1.31, 0.6
# Stabiliser stems either side of the centre, by key width (Cherry spacing).
STAB_SPACING = {2.0: 11.938, 2.25: 11.938, 2.75: 11.938, 6.25: 50.0}

ALPHA = (0.93, 0.90, 0.83)        # cream
MOD = (0.42, 0.33, 0.27)          # walnut
ACCENT = (0.90, 0.55, 0.15)       # amber, as the wheel's cap
ACCENT_KEYS = {"Esc", "Enter"}


def _wire(w, d, r, origin, ydir):
    """A rounded w x d rectangle centred on origin, in the plane spanned by X and ydir."""
    tilt = math.degrees(math.atan2(ydir.z, ydir.y))
    wire = cq.Workplane("XY").rect(w, d).val().fillet(r)
    return wire.rotate((0, 0, 0), (1, 0, 0), tilt).translate(origin)


def _top_frame(h, tilt):
    a = math.radians(tilt)
    ydir = cq.Vector(0, math.cos(a), -math.sin(a))  # positive tilt: the front edge is the high one
    return cq.Vector(0, TOP_SKEW, h), ydir


def _loft(bw, bd, br, tw, td, tr, top_origin, ydir, z0=0.0):
    lo = _wire(bw, bd, br, cq.Vector(0, 0, z0), cq.Vector(0, 1, 0))
    return cq.Solid.makeLoft([lo, _wire(tw, td, tr, top_origin, ydir)], True)


def keycap(width, row):
    """One keycap, skirt centre at the origin, Y to the back."""
    h, tilt = ROWS[row]
    bw, bd = width * U - (U - BOTTOM), BOTTOM
    tw, td = bw - TOP_DW, bd - TOP_DD
    origin, ydir = _top_frame(h, tilt)
    normal = cq.Vector(1, 0, 0).cross(ydir)
    cap = _loft(bw, bd, R_BOTTOM, tw, td, R_TOP, origin, ydir)

    # Dish: a cylinder along the top's Y, DISH_DEPTH deep at the centre line.
    rd = (tw ** 2 / 4 + DISH_DEPTH ** 2) / (2 * DISH_DEPTH)
    axis_pt = origin + normal * (rd - DISH_DEPTH) - ydir * (td + 10)
    cap = cap.cut(cq.Solid.makeCylinder(rd, 2 * td + 20, axis_pt, ydir))

    # Hollow it: the inside is the same loft, WALL in from the sides and ROOF under the top's lowest point.
    inner_origin = origin - normal * (ROOF + DISH_DEPTH)
    inner = _loft(bw - 2 * WALL, bd - 2 * WALL, max(R_BOTTOM - 0.5, 0.3), tw - 2 * WALL, td - 2 * WALL,
                  max(R_TOP - 0.5, 0.3), inner_origin, ydir, z0=-0.01)
    cap = cap.cut(inner)

    # Stems, from the underside of the top down to STEM_INSET above the skirt.
    stems = [0.0] + [s * STAB_SPACING[width] for s in (-1, 1)] if width in STAB_SPACING else [0.0]
    for x in stems:
        top_z = inner_origin.z + inner_origin.y * (-ydir.z / ydir.y) + 0.5  # inside top at y = 0, into the roof
        post = cq.Solid.makeCylinder(STEM_D / 2, top_z - STEM_INSET, cq.Vector(x, 0, STEM_INSET))
        for cw, cd in ((CROSS, CROSS_T), (CROSS_T, CROSS)):
            post = post.cut(cq.Solid.makeBox(cw, cd, 4.0, cq.Vector(x - cw / 2, -cd / 2, STEM_INSET - 0.1)))
        cap = cap.fuse(post)
    return cap.clean()


def keys(full_space):
    """(name, width, row, local x, local y) for each key in the chosen layout."""
    data = json.load(open(BOARD_JSON))
    choice = 1 if full_space else data["layoutOptions"][0]["default"]
    x0, y1 = g.KEY_GRID[0], g.KEY_GRID[3]
    out = []
    for k in data["keys"]:
        if "option" in k and k["option"][1] != choice:
            continue
        w = k.get("w", 1)
        name = k["legend"][0]
        out.append((name, w, ROW_OF[k["row"]], x0 + (k["x"] + w / 2) * U, y1 - (k["y"] + 0.5) * U))
    return out


def colour(name, width):
    if name in ACCENT_KEYS:
        return ACCENT
    return ALPHA if width == 1 and name not in ("Fn", "Mod", "Ctrl", "Win", "Alt", "Delete") else MOD


def main():
    full = "--full" in sys.argv
    cache = {}
    a = cq.Assembly(name="Keycaps")
    used = {}
    for name, w, row, x, y in keys(full):
        if (w, row) not in cache:
            cache[(w, row)] = keycap(w, row)
        shape = g.place(cache[(w, row)].translate((x, y, g.KEYCAP_BOTTOM)))
        used[name] = used.get(name, 0) + 1
        label = name if used[name] == 1 else f"{name} {used[name]}"
        a.add(shape, name=f"{label} ({row}, {w:g}u)", color=cq.Color(*colour(name, w)))
    os.makedirs(g.OUT, exist_ok=True)
    Interface_Static.SetIVal_s("write.surfacecurve.mode", 0)  # no 2D pcurves: a third of the size
    a.export(OUT)
    print(f"{os.path.relpath(OUT, g.HW)}: {len(a.children)} keycaps, {os.path.getsize(OUT) / 1e6:.1f} MB")


if __name__ == "__main__":
    main()
