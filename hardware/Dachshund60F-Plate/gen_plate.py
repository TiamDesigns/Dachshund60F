"""Generate the Dachshund60F FR4 switch plate from the mainboard PCB.

Run with KiCad's own Python (it ships the pcbnew module), from the repo root:

    "C:/Program Files/KiCad/10.0/bin/python.exe" hardware/Dachshund60F-Plate/gen_plate.py

Everything is read from hardware/Dachshund60F/Dachshund60F.kicad_pcb, so rerun
this after moving a switch, stabilizer, the encoder, J4 or the indicator LEDs.
The output overwrites Dachshund60F-Plate.kicad_pcb; edit the parameters below,
not the generated board.
"""

import math
import os

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
MAINBOARD = os.path.join(HERE, "..", "Dachshund60F", "Dachshund60F.kicad_pcb")
OUT = os.path.join(HERE, "Dachshund60F-Plate.kicad_pcb")

# Plate stock. MX switches clip into 1.5 mm plates; 1.6 mm FR4 is the usual
# substitute and the top end of what the gasket socks take (1.2-1.6 mm).
THICKNESS = 1.6

# MX switch cutout: 14 x 14 mm. The router can't cut a sharp inside corner,
# so the corners get an explicit 0.5 mm radius.
SWITCH_CUTOUT = 14.0
CUTOUT_RADIUS = 0.5

# Exposed-copper accent ring around every switch and stabilizer cutout (top
# side only), following the cutout 0.45 mm out so the router never touches
# copper. A stabilizer cutout is only 1.56 mm from its switch cutout, too close
# for two separate rings, so the copper fills that web and one ring outlines
# the switch and both stabilizers together.
ACCENT_GAP = 0.45
ACCENT_WIDTH = 0.7
COPPER_EDGE_CLEARANCE = 0.4
# The solder mask opening sits 0.05 mm inside the copper, so the mask edge,
# not the etched copper edge, draws the accent.
MASK_INSET = 0.05
MIN_ACCENT_AREA = 2.0  # mm²

# Gasket tabs for silicone socks: 6.5 mm deep, 2.8 mm wide pocket, 8 x 5 x 5 mm
# outside. A tab exactly fills the pocket, so the sock's open end sits against
# the plate edge. Four on the top edge, four on the bottom, none on the ends.
TAB_WIDTH = 2.8
TAB_LENGTH = 6.5
SOCK_LENGTH = 8.0
SOCK_WIDTH = 5.0
TAB_FRACTIONS = (1 / 8, 3 / 8, 5 / 8, 7 / 8)

# Every inside corner of the outline gets at least this radius (tab roots).
OUTLINE_INSIDE_RADIUS = 0.5
# Every outside corner gets at least this radius (tab tips, notch ends).
OUTLINE_OUTSIDE_RADIUS = 1.0

# Encoder notch: the plate stops short of the horizontal roller encoder (SW3).
# Its left edge leaves a 2.8 mm web next to the Delete cutout and leaves the
# whole 20 mm shaft and a wheel on it clear.
ENCODER_REF = "SW3"
NOTCH_LEFT = 306.0
NOTCH_BELOW_COURTYARD = 1.3
NOTCH_RADIUS = 2.0

# Pass-through slot for the display header J4 (pins stand ~8.5 mm tall).
DISPLAY_REF = "J4"
DISPLAY_SLOT_MARGIN = 0.2

# Light windows over the reverse-mount indicator LEDs.
LED_REFS = ("D2", "D3", "D4", "D5", "D6", "D7")
LED_WINDOW = 4.0

MAX_ERROR = pcbnew.FromMM(0.002)
ROUND = pcbnew.CORNER_STRATEGY_ROUND_ALL_CORNERS


def mm(v):
    return pcbnew.FromMM(v)


def rect(x0, y0, x1, y1):
    p = pcbnew.SHAPE_POLY_SET()
    p.NewOutline()
    for x, y in ((x0, y0), (x1, y0), (x1, y1), (x0, y1)):
        p.Append(mm(x), mm(y))
    return p


def rounded_rect(x0, y0, x1, y1, r):
    """Axis-aligned rectangle with all four corners rounded to radius r."""
    p = rect(x0 + r, y0 + r, x1 - r, y1 - r)
    p.Inflate(mm(r), ROUND, MAX_ERROR)
    return p


def union(polys):
    out = pcbnew.SHAPE_POLY_SET()
    for p in polys:
        out.BooleanAdd(p)
    out.Simplify()
    return out


def round_inside_corners(p, r):
    """Morphological closing: concave corners get radius r."""
    p.Inflate(mm(r), ROUND, MAX_ERROR)
    p.Inflate(-mm(r), ROUND, MAX_ERROR)


def round_outside_corners(p, r):
    """Morphological opening: convex corners get radius r."""
    p.Inflate(-mm(r), ROUND, MAX_ERROR)
    p.Inflate(mm(r), ROUND, MAX_ERROR)


def bbox_mm(box):
    return (pcbnew.ToMM(box.GetX()), pcbnew.ToMM(box.GetY()),
            pcbnew.ToMM(box.GetRight()), pcbnew.ToMM(box.GetBottom()))


def read_mainboard():
    board = pcbnew.LoadBoard(MAINBOARD)

    x0, y0, x1, y1 = bbox_mm(board.GetBoardEdgesBoundingBox())
    radii = {round(pcbnew.ToMM(d.GetRadius()), 3) for d in board.GetDrawings()
             if d.GetLayer() == pcbnew.Edge_Cuts and d.GetShape() == pcbnew.SHAPE_T_ARC}
    if len(radii) != 1:
        raise SystemExit(f"expected one corner radius on the mainboard outline, got {radii}")
    # The bounding box includes the line width; the outline itself is 0.025 mm in.
    edge = (round(x0 + 0.025, 3), round(y0 + 0.025, 3), round(x1 - 0.025, 3), round(y1 - 0.025, 3))

    switches, stabs = [], []
    for fp in board.GetFootprints():
        ref = fp.GetReference()
        if ref.startswith("SW"):
            # Every MX footprint has the 4 mm centre-post hole at the key centre.
            # The stock plate-mount footprints have their origin on pin 1.
            posts = [p for p in fp.Pads() if abs(pcbnew.ToMM(p.GetDrillSize().x) - 4.0) < 0.01]
            if posts:
                c = posts[0].GetPosition()
                switches.append((ref, pcbnew.ToMM(c.x), pcbnew.ToMM(c.y)))
        elif ref.startswith("ST"):
            if fp.GetOrientationDegrees() % 360:
                raise SystemExit(f"{ref}: rotated stabilizers aren't handled")
            # The perigoso stabilizer footprints carry their Cherry plate
            # cutouts on Eco1.User: one rectangle each side of the key centre.
            cx = pcbnew.ToMM(fp.GetPosition().x)
            sides = {}
            for g in fp.GraphicalItems():
                if g.GetLayer() != pcbnew.Eco1_User:
                    continue
                for pt in (g.GetStart(), g.GetEnd()):
                    x, y = pcbnew.ToMM(pt.x), pcbnew.ToMM(pt.y)
                    b = sides.setdefault(x < cx, [x, y, x, y])
                    b[0], b[1] = min(b[0], x), min(b[1], y)
                    b[2], b[3] = max(b[2], x), max(b[3], y)
            if len(sides) != 2:
                raise SystemExit(f"{ref}: no Eco1.User plate cutouts found")
            stabs.extend((ref, *b) for b in sides.values())

    def courtyard(ref):
        fp = board.FindFootprintByReference(ref)
        layer = pcbnew.B_CrtYd if fp.IsFlipped() else pcbnew.F_CrtYd
        return bbox_mm(fp.GetCourtyard(layer).BBox())

    def position(ref):
        p = board.FindFootprintByReference(ref).GetPosition()
        return pcbnew.ToMM(p.x), pcbnew.ToMM(p.y)

    return {
        "edge": edge,
        "corner_radius": radii.pop(),
        "switches": sorted(switches),
        "stabs": sorted(stabs),
        "encoder": courtyard(ENCODER_REF),
        "display": courtyard(DISPLAY_REF),
        "leds": [(ref, *position(ref)) for ref in LED_REFS],
    }


def build(mb):
    x0, y0, x1, y1 = mb["edge"]
    half = SWITCH_CUTOUT / 2

    # --- cutouts -----------------------------------------------------------
    key_holes = [rect(x - half, y - half, x + half, y + half) for _, x, y in mb["switches"]]
    key_holes += [rect(*b) for _, *b in mb["stabs"]]
    key_holes = union(key_holes)
    round_outside_corners(key_holes, CUTOUT_RADIUS)

    holes = [key_holes]
    dx0, dy0, dx1, dy1 = mb["display"]
    m = DISPLAY_SLOT_MARGIN
    holes.append(rect(dx0 - m, dy0 - m, dx1 + m, dy1 + m))

    def in_switch_cutout(x, y):
        return any(abs(x - sx) < half and abs(y - sy) < half for _, sx, sy in mb["switches"])

    w = LED_WINDOW / 2
    led_windows = []
    for ref, x, y in mb["leds"]:
        if in_switch_cutout(x, y):
            continue  # under a key: the switch cutout already lets the light through
        holes.append(rect(x - w, y - w, x + w, y + w))
        led_windows.append(ref)

    # Overlapping layouts (the split and 6.25u spacebars) merge into one hole.
    holes = union(holes)
    round_outside_corners(holes, CUTOUT_RADIUS)

    # --- outline -----------------------------------------------------------
    outline = rounded_rect(x0, y0, x1, y1, mb["corner_radius"])

    tab_xs = [x0 + f * (x1 - x0) for f in TAB_FRACTIONS]
    tabs = []
    for tx in tab_xs:
        # Each tab overlaps the plate by 1 mm so the union has no seam.
        tabs.append(rect(tx - TAB_WIDTH / 2, y0 - TAB_LENGTH, tx + TAB_WIDTH / 2, y0 + 1))
        tabs.append(rect(tx - TAB_WIDTH / 2, y1 - 1, tx + TAB_WIDTH / 2, y1 + TAB_LENGTH))
    outline = union([outline] + tabs)

    ex0, ey0, ex1, ey1 = mb["encoder"]
    notch_bottom = round(ey1 + NOTCH_BELOW_COURTYARD, 2)
    if not NOTCH_LEFT < ex0:
        raise SystemExit("encoder courtyard is left of the notch")
    # Oversize the notch past the outline so only its inside corner is real.
    notch = rect(NOTCH_LEFT, y0 - 10, x1 + 10, notch_bottom)
    round_outside_corners(notch, NOTCH_RADIUS)
    outline.BooleanSubtract(notch)
    outline.Simplify()

    round_inside_corners(outline, OUTLINE_INSIDE_RADIUS)
    round_outside_corners(outline, OUTLINE_OUTSIDE_RADIUS)

    plate = pcbnew.SHAPE_POLY_SET(outline)
    plate.BooleanSubtract(holes)
    plate.Simplify()

    # --- copper accents ----------------------------------------------------
    copper = pcbnew.SHAPE_POLY_SET(key_holes)
    copper.Inflate(mm(ACCENT_GAP + ACCENT_WIDTH), ROUND, MAX_ERROR)
    inner = pcbnew.SHAPE_POLY_SET(key_holes)
    inner.Inflate(mm(ACCENT_GAP), ROUND, MAX_ERROR)
    copper.BooleanSubtract(inner)
    # Keep copper clear of every routed edge, including the merged spacebar holes.
    # The extra 0.01 mm covers the arc-to-polygon error, so DRC sees >= 0.4 mm.
    keepout = pcbnew.SHAPE_POLY_SET(holes)
    keepout.Inflate(mm(COPPER_EDGE_CLEARANCE + 0.01), ROUND, MAX_ERROR)
    copper.BooleanSubtract(keepout)
    inside = pcbnew.SHAPE_POLY_SET(outline)
    inside.Inflate(-mm(COPPER_EDGE_CLEARANCE + 0.01), ROUND, MAX_ERROR)
    copper.BooleanIntersection(inside)
    copper.Simplify()
    # Clipping the overlapping spacebar rings leaves a few slivers that would
    # peel off; drop any copper island smaller than MIN_ACCENT_AREA.
    kept = pcbnew.SHAPE_POLY_SET()
    for i in range(copper.OutlineCount()):
        if abs(copper.Outline(i).Area()) < mm(1) * mm(1) * MIN_ACCENT_AREA:
            continue
        n = kept.AddOutline(copper.Outline(i))
        for j in range(copper.HoleCount(i)):
            kept.AddHole(copper.Hole(i, j), n)
    copper = kept

    mask = pcbnew.SHAPE_POLY_SET(copper)
    mask.Inflate(-mm(MASK_INSET), ROUND, MAX_ERROR)
    mask.Simplify()

    socks = []
    for tx in tab_xs:
        socks.append((tx, y0 - SOCK_LENGTH, y0))
        socks.append((tx, y1, y1 + SOCK_LENGTH))

    return {
        "plate": plate, "copper": copper, "mask": mask, "socks": socks,
        "tab_xs": tab_xs, "notch_bottom": notch_bottom, "led_windows": led_windows,
    }


def add_contour(board, chain, layer, width=0.05):
    pts = [chain.CPoint(i) for i in range(chain.PointCount())]
    s = pcbnew.PCB_SHAPE(board, pcbnew.SHAPE_T_POLY)
    s.SetPolyPoints([pcbnew.VECTOR2I(p.x, p.y) for p in pts])
    s.SetLayer(layer)
    s.SetWidth(mm(width))
    s.SetFilled(False)
    board.Add(s)


def add_fill(board, poly, layer):
    poly = pcbnew.SHAPE_POLY_SET(poly)
    poly.Fracture()
    for i in range(poly.OutlineCount()):
        s = pcbnew.PCB_SHAPE(board, pcbnew.SHAPE_T_POLY)
        one = pcbnew.SHAPE_POLY_SET()
        one.AddOutline(poly.Outline(i))
        s.SetPolyShape(one)
        s.SetLayer(layer)
        s.SetWidth(0)
        s.SetFilled(True)
        board.Add(s)


def add_text(board, text, x, y, layer, size=1.2, mirror=False):
    t = pcbnew.PCB_TEXT(board)
    t.SetText(text)
    t.SetPosition(pcbnew.VECTOR2I(mm(x), mm(y)))
    t.SetLayer(layer)
    t.SetTextSize(pcbnew.VECTOR2I(mm(size), mm(size)))
    t.SetTextThickness(mm(size * 0.15))
    t.SetMirrored(mirror)
    board.Add(t)


def write(mb, geo):
    board = pcbnew.CreateEmptyBoard()
    ds = board.GetDesignSettings()
    ds.SetBoardThickness(mm(THICKNESS))
    ds.m_CopperEdgeClearance = mm(COPPER_EDGE_CLEARANCE)

    tb = board.GetTitleBlock()
    tb.SetTitle("Dachshund60F plate")
    tb.SetRevision("A")
    tb.SetCompany("Dachshund60F")
    tb.SetComment(0, f"FR4 {THICKNESS} mm, 2 layers (B.Cu empty), exposed-copper accents on F.Cu")
    tb.SetComment(1, "Generated by gen_plate.py from the mainboard; don't edit by hand")

    plate = geo["plate"]
    for i in range(plate.OutlineCount()):
        add_contour(board, plate.Outline(i), pcbnew.Edge_Cuts)
        for j in range(plate.HoleCount(i)):
            add_contour(board, plate.Hole(i, j), pcbnew.Edge_Cuts)

    add_fill(board, geo["copper"], pcbnew.F_Cu)
    add_fill(board, geo["mask"], pcbnew.F_Mask)

    # Sock outlines on Cmts.User, for designing the case pockets.
    for x, ya, yb in geo["socks"]:
        add_contour(board, rect(x - SOCK_WIDTH / 2, ya, x + SOCK_WIDTH / 2, yb).Outline(0),
                    pcbnew.Cmts_User, 0.1)

    # Bottom-side text faces the PCB, so it's never seen. "JLCJLCJLCJLC" tells
    # JLCPCB where to print its order number instead of somewhere on the top.
    x0, y0, x1, _ = mb["edge"]
    add_text(board, "Dachshund60F plate rev A  FR4 1.6 mm", 90.0, 61.4, pcbnew.B_SilkS, mirror=True)
    add_text(board, "JLCJLCJLCJLC", 160.0, 61.4, pcbnew.B_SilkS, mirror=True)

    pcbnew.SaveBoard(OUT, board)


def main():
    mb = read_mainboard()
    geo = build(mb)
    write(mb, geo)
    x0, y0, x1, y1 = mb["edge"]
    print(f"plate outline {x1 - x0:.2f} x {y1 - y0:.2f} mm, corner radius {mb['corner_radius']} mm")
    print(f"{len(mb['switches'])} switch cutouts, {len(mb['stabs'])} stabilizer cutouts, "
          f"LED windows over {', '.join(geo['led_windows'])}")
    print("gasket tabs at x = " + ", ".join(f"{x:.2f}" for x in geo["tab_xs"])
          + f" (top edge y = {y0}, bottom edge y = {y1})")
    print(f"encoder notch: x >= {NOTCH_LEFT}, y <= {geo['notch_bottom']}")
    print(f"wrote {os.path.relpath(OUT)}")


if __name__ == "__main__":
    main()
