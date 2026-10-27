"""Generate the Dachshund60F case, gaskets, weight and plate foam, and the full assembly.

    python hardware/case/gen_case.py            # parts and assembly
    python hardware/case/gen_case.py --parts    # parts only (skips the slow board import)
    python hardware/case/gen_case.py --renders  # also renders PNGs to hardware/case/renders/

Needs CadQuery (pip install cadquery). Writes to hardware/case/step/:

- Dachshund60F-Case-Top.step      top case: bezel frame, upper half of the gasket pockets
- Dachshund60F-Case-Bottom.step   bottom case: tub, lower gasket seats, USB-C, battery tray
- Dachshund60F-Weight.step        brass dog-bone weight, shows through the bottom
- Dachshund60F-Plate-Foam.dxf     plate-PCB midlayer foam outline, to cut from 3.5 mm Poron
                                  (the plate's own holes, grown 0.3 mm; board-centred origin)
- Dachshund60F-Gaskets.step       the eight silicone socks on the plate tabs
- Dachshund60F-Case-Assembly.step everything above plus the mainboard, plate,
                                  USB-C daughterboard, a battery and a roller stand-in

Frames. "Local" is the board frame of hardware/step/: X along the board, Y to the back, origin at
the centre of the mainboard outline, PCB bottom at Z = 0, plate bottom at Z = 5.0. "World" is the
desk frame: the case bottom is Z = 0, and the board is tilted TILT degrees about X (back up). Every
dimension below is in mm. The plate and PCB files are read, never written.
"""

import math
import os
import re
import sys

import cadquery as cq

HERE = os.path.dirname(os.path.abspath(__file__))
HW = os.path.dirname(HERE)
OUT = os.path.join(HERE, "step")
PLATE_PCB = os.path.join(HW, "Dachshund60F-Plate", "Dachshund60F-Plate.kicad_pcb")
BOARD_STEP = os.path.join(HW, "step", "Dachshund60F-Assembly.step")
USB_STEP = os.path.join(HW, "step", "Dachshund60F-USB.step")

# KiCad coordinates of the mainboard outline centre (the STEP origin).
KICAD_ORIGIN = (178.25, 99.55)

# --- Board stack (local frame, from hardware/step/) -----------------------------------------
BOARD_W, BOARD_H, BOARD_R = 318.5, 121.1, 5.0  # mainboard outline
PCB_T = 1.6
PLATE_Z0, PLATE_T = 5.0, 1.6                    # plate bottom, thickness
PLATE_MID = PLATE_Z0 + PLATE_T / 2

# --- Case ------------------------------------------------------------------------------------
TILT = 7.0             # typing angle, degrees
GAP = 0.75             # board/plate edge to case wall, all round
BEZEL_SIDE = 9.0       # wall outside the outline on the ends (no gaskets there)
BEZEL_FB = 12.0        # wall outside the outline front and back (the socks reach 8 mm out)
OUTER_R = 9.0          # plan-view corner radius
TOP_FILLET = 4.0       # rounded top edge
SEAM_REVEAL = 0.8      # chamfer on both halves at the seam, so it reads as a groove
INNER_CHAMFER = 0.8    # chamfer on the bezel's inside edge
FOOT_CHAMFER = 1.0     # bottom edge, so the case seems to float
BEZEL_ABOVE_PLATE = 3.5  # bezel top above the plate top (keycaps clear it: they sit inside it)
FLOOR = 3.0            # bottom thickness
UNDER_PCB_FRONT = 3.0  # space under the PCB at the front edge (hot-swap sockets are 1.85 mm)

# Gasket socks (silicone, 8 x 5 x 5 mm outside; pocket 6.5 x 2.8 x 1.2 mm, takes 1.2-1.6 mm plates).
SOCK_L, SOCK_W, SOCK_H = 8.0, 5.0, 5.0
SOCK_POCKET_L, SOCK_POCKET_W = 6.5, 2.8
SOCK_SQUEEZE = 0.4     # the case pocket is this much shorter than a free sock
POCKET_CLEAR = 0.15    # sideways clearance around a sock in its case pocket
TAB_X = [BOARD_W * f - BOARD_W / 2 for f in (1 / 8, 3 / 8, 5 / 8, 7 / 8)]  # plate tab centres
SOCK_Z0 = PLATE_MID - (SOCK_H - SOCK_SQUEEZE) / 2  # compressed sock bottom
SPLIT = SOCK_Z0        # top/bottom case seam (local Z): the bottom case is the socks' floor
BEZEL_TOP = PLATE_Z0 + PLATE_T + BEZEL_ABOVE_PLATE

# Top case screws: M3 x 10 socket head from below into M3 x 4 heat-set inserts (OD 4.2).
SCREWS = [(x, s * 66.6) for x in (-79.6, 0.0, 79.6) for s in (-1, 1)] + [(-163.9, 15.0), (163.9, 15.0)]
SCREW_HOLE, SCREW_CBORE, SCREW_GRIP = 3.4, 5.8, 6.0
INSERT_D, INSERT_DEPTH = 4.2, 4.8

# Plate foam (Poron, 3.5 mm sheet, 3.4 mm installed between PCB top and plate bottom).
FOAM_HOLE_GROW = 0.3   # each plate hole, grown by this much
FOAM_EDGE_INSET = 0.3  # in from the board outline

# Brass weight: a dog bone. The bone shows through a window in the floor; a flange around it
# (the bone grown by BONE_FLANGE) sits on the floor inside and takes the screws.
BONE_X0, BONE_Y0 = -60.0, 19.0   # centre (world XY)
BONE_L = 150.0                   # between lobe centres
BONE_SHAFT_W = 24.0
BONE_LOBE_R, BONE_LOBE_DY = 14.0, 11.0
BONE_FLANGE = 5.0
BONE_FLANGE_T = 6.0
BONE_RECESS = 0.4                # bone face this far up from the case bottom
BRASS_DENSITY = 8.5e-3           # g/mm^3

# USB-C daughterboard (hardware/step/Dachshund60F-USB.step, connector facing +Y): in the back wall,
# flat on a ledge, receptacle face USB_FACE_INSET inside the back face.
USB_X = 110.0          # world X of the connector centre
USB_Z = 6.0            # world Z of the daughterboard's bottom
USB_FACE_INSET = 1.0
USB_KICAD_CONNECTOR_X, USB_KICAD_FRONT_Y = 121.0, 98.7  # connector centre and shell face (KiCad)
USB_BOARD_X = (100.0, 142.0)
USB_BOARD_FRONT_Y = 100.0
USB_HOLES_KICAD = [(104.5, 104.5), (137.5, 104.5)]
USB_SHELL = (8.94, 3.26)

# Battery stand-in: 803450 LiPo (8 x 34 x 50 mm, ~1500 mAh), in a tray of low ribs on the floor.
BATTERY = (50.0, 34.0, 8.0)
BATTERY_XY = (70.0, 31.0)        # centre, world
TRAY_RIB_T, TRAY_RIB_H, TRAY_CLEAR = 1.6, 3.0, 0.75

# Parts reached through the case (local frame, from the mainboard; KiCad refs).
SW1_LOCAL = (157.25, -12.45)     # power slide switch, actuator out of the right edge
SW2_LOCAL = (136.75, -6.45)      # reset button on the back
RESET_PIN_D = 2.2

# Roller on the encoder shaft (stand-in). The shaft axis is ~6.7 mm above the PCB top (assumed in
# hardware/3dmodels), so a roller bigger than ~12 mm across would hit the PCB.
ROLLER_X = (135.25, 143.25)
ROLLER_Y, ROLLER_Z, ROLLER_D = 50.025, PCB_T + 6.7, 12.0

# Rubber feet: 10 mm bumpons in shallow pockets.
FEET_X, FEET_D, FEET_DEPTH = 150.0, 10.5, 0.6

COLORS = {
    "Case top": (0.42, 0.27, 0.17),
    "Case bottom": (0.85, 0.76, 0.61),
    "Weight": (0.80, 0.63, 0.30),
    "Plate foam": (0.93, 0.88, 0.80),
    "Gasket": (0.22, 0.22, 0.22),
    "Battery": (0.55, 0.62, 0.70),
    "Roller": (0.15, 0.13, 0.12),
    "Feet": (0.10, 0.10, 0.10),
}

S, C = math.sin(math.radians(TILT)), math.cos(math.radians(TILT))
# Lift so the space under the PCB's front edge is UNDER_PCB_FRONT, above a FLOOR-thick bottom.
LIFT = FLOOR + UNDER_PCB_FRONT * C + (BOARD_H / 2 + GAP) * S


def to_world(y, z):
    """World (y, z) of a local (y, z)."""
    return y * C - z * S, y * S + z * C + LIFT


def to_local(y, z):
    z -= LIFT
    return y * C + z * S, -y * S + z * C


def place(shape):
    """Move a local-frame shape (cq Shape or Workplane) into the world frame."""
    if isinstance(shape, cq.Workplane):
        shape = shape.val()
    return shape.rotate((0, 0, 0), (1, 0, 0), TILT).translate((0, 0, LIFT))


def rrect(w, h, r, z0, z1, x=0.0, y=0.0):
    return (cq.Workplane("XY").workplane(offset=z0).center(x, y).rect(w, h)
            .extrude(z1 - z0).edges("|Z").fillet(r).val())


def box(x0, x1, y0, y1, z0, z1):
    return cq.Solid.makeBox(x1 - x0, y1 - y0, z1 - z0, cq.Vector(x0, y0, z0))


def cyl_z(x, y, d, z0, z1):
    return cq.Solid.makeCylinder(d / 2, z1 - z0, cq.Vector(x, y, z0))


def half_space_local(z, above, size=1000.0):
    """Local half-space z' > z (above) or z' < z, as a big box in the world frame."""
    return place(box(-size, size, -size, size, z, z + size) if above else
                 box(-size, size, -size, size, z - size, z))


# --- Outer shell -------------------------------------------------------------------------------

OUT_X = BOARD_W / 2 + BEZEL_SIDE
OUT_Y_FRONT = to_world(-(BOARD_H / 2 + BEZEL_FB), BEZEL_TOP)[0]
OUT_Y_BACK = to_world(BOARD_H / 2 + BEZEL_FB, BEZEL_TOP)[0]
INNER_W, INNER_H, INNER_R = BOARD_W + 2 * GAP, BOARD_H + 2 * GAP, BOARD_R + GAP


def _plane_face(shape, normal, pick=max):
    """The largest planar face of shape whose normal is `normal`."""
    faces = [f for f in shape.Faces() if f.geomType() == "PLANE" and f.normalAt().dot(normal) > 0.999]
    return pick(faces, key=lambda f: f.Area())


def _seam_reveal(shape, normal):
    return shape.chamfer(SEAM_REVEAL, None, _plane_face(shape, normal).outerWire().Edges())


def engraving():
    """Back face of the bottom case: a bone and the board's name, 0.6 mm deep."""
    plane = cq.Plane(origin=(-110, OUT_Y_BACK + 0.01, 11.5), xDir=(-1, 0, 0), normal=(0, 1, 0))
    wp = cq.Workplane(plane)
    text = wp.text("DACHSHUND60F", 6.0, -0.6, kind="bold", halign="center", valign="center").val()
    bone = (cq.Workplane(plane).center(-52, 0).rect(12, 2.6).extrude(-0.6)
            .union(cq.Workplane(plane).pushPoints([(-58, 1.5), (-58, -1.5), (-46, 1.5), (-46, -1.5)])
                   .circle(1.7).extrude(-0.6))).val()
    return text.fuse(bone)


def outer_shell():
    """Vertical-walled body, cut by the tilted bezel plane, with the rounded top and chamfered foot."""
    yc, h = (OUT_Y_FRONT + OUT_Y_BACK) / 2, OUT_Y_BACK - OUT_Y_FRONT
    body = rrect(2 * OUT_X, h, OUTER_R, 0, 60, y=yc).cut(half_space_local(BEZEL_TOP, True))
    n = cq.Vector(0, -S, C)
    top = max(body.Faces(), key=lambda f: f.Center().dot(n) if f.normalAt().dot(n) > 0.99 else -1e9)
    body = body.fillet(TOP_FILLET, top.outerWire().Edges())
    bottom = min(body.Faces(), key=lambda f: f.Center().z)
    return body.chamfer(FOOT_CHAMFER, None, bottom.outerWire().Edges())


def inner_prism(z0, z1, grow=0.0):
    """The case's inside outline (local frame, walls square to the plate), local z0..z1."""
    return place(rrect(INNER_W + 2 * grow, INNER_H + 2 * grow, INNER_R + grow, z0, z1))


def cavity():
    """Inside of the bottom case: square to the board above local z = -UNDER_PCB_FRONT, vertical below."""
    upper = inner_prism(-UNDER_PCB_FRONT, 50)
    yf = to_world(-INNER_H / 2, -UNDER_PCB_FRONT)[0]
    yb = to_world(INNER_H / 2, -UNDER_PCB_FRONT)[0]
    lower = rrect(INNER_W, yb - yf, INNER_R, FLOOR, 80, y=(yf + yb) / 2)
    lower = lower.cut(half_space_local(-UNDER_PCB_FRONT + 0.01, True))
    return upper.fuse(lower)


def sock_pockets():
    """Pockets for the socks (local frame), both halves; each case half keeps its own side."""
    cuts = []
    w = SOCK_W + 2 * POCKET_CLEAR
    edge = BOARD_H / 2
    for x in TAB_X:
        for s in (-1, 1):
            y0, y1 = sorted((s * (edge - 0.5), s * (edge + SOCK_L + POCKET_CLEAR)))
            cuts.append(box(x - w / 2, x + w / 2, y0, y1, SOCK_Z0, SOCK_Z0 + SOCK_H - SOCK_SQUEEZE))
    return [place(c) for c in cuts]


def screw_points():
    pts = []
    for x, y in SCREWS:
        yw, zw = to_world(y, SPLIT)
        pts.append((x, yw, zw))
    return pts


# --- Weight ------------------------------------------------------------------------------------

def bone_face(grow=0.0):
    w = (cq.Workplane("XY").center(BONE_X0, BONE_Y0)
         .rect(BONE_L, BONE_SHAFT_W + 2 * grow).extrude(1))
    for dx in (-BONE_L / 2, BONE_L / 2):
        for dy in (-BONE_LOBE_DY, BONE_LOBE_DY):
            w = w.union(cq.Workplane("XY").center(BONE_X0 + dx, BONE_Y0 + dy)
                        .circle(BONE_LOBE_R + grow).extrude(1))
    face = min(w.val().Faces(), key=lambda f: f.Center().z)
    return face


def extrude_face(face, z0, z1):
    f = face.translate((0, 0, z0 - face.Center().z))
    return cq.Solid.extrudeLinear(f, cq.Vector(0, 0, z1 - z0))


def weight_holes():
    dy = BONE_SHAFT_W / 2 + BONE_FLANGE / 2
    return [(BONE_X0 + dx, BONE_Y0 + sy * dy) for dx in (-40, 40) for sy in (-1, 1)]


def weight():
    flange = extrude_face(bone_face(BONE_FLANGE), FLOOR, FLOOR + BONE_FLANGE_T)
    boss = extrude_face(bone_face(), BONE_RECESS, FLOOR + 0.5)
    wt = flange.fuse(boss).clean()
    for x, y in weight_holes():  # M3 tapped, 5 mm deep
        wt = wt.cut(cyl_z(x, y, 2.5, FLOOR - 0.1, FLOOR + 5.0))
    return wt


# --- Case halves ------------------------------------------------------------------------------

def usb_world(kx, ky):
    """World XY of a point on the daughterboard, given in its KiCad coordinates."""
    return (kx - USB_KICAD_CONNECTOR_X + USB_X,
            (USB_KICAD_FRONT_Y - ky) + OUT_Y_BACK - USB_FACE_INSET)


def usb_cuts():
    x0, _ = usb_world(USB_BOARD_X[0], 0)
    x1, _ = usb_world(USB_BOARD_X[1], 0)
    _, y_edge = usb_world(0, USB_BOARD_FRONT_Y)
    _, y_face = usb_world(0, USB_KICAD_FRONT_Y)
    zc = USB_Z + PCB_T + USB_SHELL[1] / 2
    cuts = [
        box(x0 - 0.4, x1 + 0.4, 55.0, y_edge + 0.2, USB_Z, USB_Z + PCB_T + 4.0),     # board pocket
        box(USB_X - 5.5, USB_X + 5.5, y_edge - 6.0, y_edge + 0.2, USB_Z - 1.0, USB_Z),  # leg relief
    ]
    # Receptacle opening and a shallow lead-in for the plug's overmould.
    for (w, h), y0 in (((USB_SHELL[0] + 0.9, USB_SHELL[1] + 0.8), y_face - 1.0),
                       ((12.4, 6.6), y_face)):
        plane = cq.Plane(origin=(USB_X, y0, zc), xDir=(1, 0, 0), normal=(0, 1, 0))
        slot = cq.Workplane(plane).slot2D(w, h).extrude(OUT_Y_BACK + 5 - y0).val()
        cuts.append(slot)
    for kx, ky in USB_HOLES_KICAD:  # M3 x 4 heat-set inserts
        x, y = usb_world(kx, ky)
        cuts.append(cyl_z(x, y, INSERT_D, USB_Z - INSERT_DEPTH, USB_Z + 0.1))
    return cuts


def bottom_case(shell):
    b = shell.cut(half_space_local(SPLIT, True))
    b = _seam_reveal(b, cq.Vector(0, -S, C))
    b = b.cut(cavity())
    b = b.cut(engraving())
    for p in sock_pockets():
        b = b.cut(p)
    # Battery tray ribs.
    bw, bh = BATTERY[0] + 2 * TRAY_CLEAR, BATTERY[1] + 2 * TRAY_CLEAR
    ring = rrect(bw + 2 * TRAY_RIB_T, bh + 2 * TRAY_RIB_T, 1.5, FLOOR - 0.1, FLOOR + TRAY_RIB_H, *BATTERY_XY)
    ring = ring.cut(box(BATTERY_XY[0] - bw / 2, BATTERY_XY[0] + bw / 2,
                        BATTERY_XY[1] - bh / 2, BATTERY_XY[1] + bh / 2, 0, 50))
    ring = ring.cut(box(BATTERY_XY[0] + bw / 2 - 1, BATTERY_XY[0] + bw / 2 + 5,  # lead exit
                        BATTERY_XY[1] - 5, BATTERY_XY[1] + 5, 0, 50))
    b = b.fuse(ring)
    # Weight: window through the floor, plus countersunk M3 holes into the flange.
    b = b.cut(extrude_face(bone_face(0.15), -1, FLOOR + 0.1))
    for x, y in weight_holes():
        b = b.cut(cyl_z(x, y, 3.4, -1, FLOOR + 0.1))
        b = b.cut(cq.Solid.makeCone(3.3, 1.7, 1.6, cq.Vector(x, y, -0.01)))
    # USB-C daughterboard pocket, opening and inserts.
    for c in usb_cuts():
        b = b.cut(c)
    # Power switch: a slot to the actuator and a finger scoop outside it.
    x, y = SW1_LOCAL
    b = b.cut(place(box(x + 1.5, x + 12, y - 4.0, y + 4.0, -2.8, 0.3)))
    b = b.cut(place(cyl_z(OUT_X + 5.5, y, 19.0, -7.0, 0.5)))
    # Reset: a pin hole straight up to SW2.
    b = b.cut(place(cyl_z(*SW2_LOCAL, RESET_PIN_D, -60, -0.5)))
    # Screws to the top case.
    for x, y, z in screw_points():
        b = b.cut(cyl_z(x, y, SCREW_HOLE, -1, z + 1))
        b = b.cut(cyl_z(x, y, SCREW_CBORE, -1, z - SCREW_GRIP))
    # Feet.
    for x in (-FEET_X, FEET_X):
        for y in (OUT_Y_FRONT + 11, OUT_Y_BACK - 11):
            b = b.cut(cyl_z(x, y, FEET_D, -1, FEET_DEPTH))
    return b.clean()


def top_case(shell):
    t = shell.cut(half_space_local(SPLIT, False))
    t = _seam_reveal(t, cq.Vector(0, S, -C))
    t = t.cut(inner_prism(-50, 50))
    for p in sock_pockets():
        t = t.cut(p)
    for x, y, z in screw_points():
        t = t.cut(cyl_z(x, y, INSERT_D, z - 1, z + INSERT_DEPTH))
    # Chamfer the inside edge of the bezel.
    n = cq.Vector(0, -S, C)
    top = max((f for f in t.Faces() if f.normalAt().dot(n) > 0.99), key=lambda f: f.Center().dot(n))
    inner = [e for w in top.innerWires() for e in w.Edges()]
    t = t.chamfer(INNER_CHAMFER, None, inner)
    return t.clean()


# --- Gaskets, foam, stand-ins ------------------------------------------------------------------

def gaskets():
    socks = []
    h = SOCK_H - SOCK_SQUEEZE
    for x in TAB_X:
        for s in (-1, 1):
            edge = s * BOARD_H / 2
            y0, y1 = sorted((edge, edge + s * SOCK_L))
            sock = box(x - SOCK_W / 2, x + SOCK_W / 2, y0, y1, SOCK_Z0, SOCK_Z0 + h)
            sock = sock.fillet(0.8, [e for e in sock.Edges()])
            p0, p1 = sorted((edge, edge + s * SOCK_POCKET_L))
            sock = sock.cut(box(x - SOCK_POCKET_W / 2, x + SOCK_POCKET_W / 2, p0 - 1 if s > 0 else p0,
                                p1 if s > 0 else p1 + 1, PLATE_Z0, PLATE_Z0 + PLATE_T))
            socks.append(place(sock))
    return cq.Compound.makeCompound(socks)


def plate_loops():
    """Closed Edge.Cuts polygons of the plate, in the local frame. The outline is the first."""
    s = open(PLATE_PCB, encoding="utf-8").read()
    loops = []
    for pts, layer in re.findall(r'\(gr_poly\s*\(pts(.*?)\)\s*\(stroke.*?\(layer "([^"]+)"\)', s, re.S):
        if layer != "Edge.Cuts":
            continue
        xy = [(float(a) - KICAD_ORIGIN[0], KICAD_ORIGIN[1] - float(b))
              for a, b in re.findall(r"\(xy ([-\d.]+) ([-\d.]+)\)", pts)]
        loops.append(xy)
    loops.sort(key=lambda p: -(max(q[0] for q in p) - min(q[0] for q in p)))
    return loops


def poly_face(xy, z=0.0):
    pts = [cq.Vector(x, y, z) for x, y in xy]
    if (pts[0] - pts[-1]).Length > 1e-6:
        pts.append(pts[0])
    return cq.Face.makeFromWires(cq.Wire.makePolygon(pts))


def simplify(xy, tol=0.005):
    """Drop points that sit on a straight line between their neighbours."""
    out = []
    n = len(xy)
    for i, (x, y) in enumerate(xy):
        (ax, ay), (bx, by) = xy[i - 1], xy[(i + 1) % n]
        cross = (bx - ax) * (y - ay) - (by - ay) * (x - ax)
        if abs(cross) / max(math.hypot(bx - ax, by - ay), 1e-9) > tol:
            out.append((x, y))
    return out


def grow(xy, d):
    """Offset a polygon outward by d with mitred corners (plate holes have no sharp inside corners)."""
    area = sum(x0 * y1 - x1 * y0 for (x0, y0), (x1, y1) in zip(xy, xy[1:] + xy[:1]))
    d = d if area > 0 else -d  # counter-clockwise: outward is to the right of each edge
    out = []
    n = len(xy)
    for i in range(n):
        p0, p1, p2 = xy[i - 1], xy[i], xy[(i + 1) % n]
        normals = []
        for (ax, ay), (bx, by) in ((p0, p1), (p1, p2)):
            L = math.hypot(bx - ax, by - ay)
            normals.append(((by - ay) / L, -(bx - ax) / L))
        nx, ny = normals[0][0] + normals[1][0], normals[0][1] + normals[1][1]
        k = d / max(1 + normals[0][0] * normals[1][0] + normals[0][1] * normals[1][1], 0.2)
        out.append((p1[0] + nx * k, p1[1] + ny * k))
    return out


def foam_face():
    """The foam sheet as a flat face at Z = 0 (local XY): the plate's outline trimmed to the board
    outline, with every plate hole grown by FOAM_HOLE_GROW."""
    loops = [simplify(xy) for xy in plate_loops()]
    plate = extrude_face(poly_face(loops[0]), -1, 1)
    inset = rrect(BOARD_W - 2 * FOAM_EDGE_INSET, BOARD_H - 2 * FOAM_EDGE_INSET,
                  BOARD_R - FOAM_EDGE_INSET, -2, 2)
    sheet = plate.intersect(inset)
    holes = [extrude_face(poly_face(grow(xy, FOAM_HOLE_GROW)), -2, 2) for xy in loops[1:]]
    sheet = sheet.cut(cq.Compound.makeCompound(holes)).clean()
    bottom = min((f for f in sheet.Faces() if f.normalAt().z < -0.99), key=lambda f: f.Center().z)
    return bottom.translate((0, 0, 1))


def plate_foam():
    return place(extrude_face(foam_face(), PCB_T, PLATE_Z0))


def battery():
    x, y = BATTERY_XY
    b = box(x - BATTERY[0] / 2, x + BATTERY[0] / 2, y - BATTERY[1] / 2, y + BATTERY[1] / 2,
            FLOOR, FLOOR + BATTERY[2])
    return b.fillet(1.0, cq.Workplane().add(b).edges("|X").vals())


def roller():
    w = ROLLER_X[1] - ROLLER_X[0]
    r = cq.Solid.makeCylinder(ROLLER_D / 2, w, cq.Vector(ROLLER_X[0], ROLLER_Y, ROLLER_Z), cq.Vector(1, 0, 0))
    for i in range(1, 6):  # grip grooves
        x = ROLLER_X[0] + i * w / 6
        groove = cq.Solid.makeCylinder(ROLLER_D, 0.5, cq.Vector(x - 0.25, ROLLER_Y, ROLLER_Z), cq.Vector(1, 0, 0))
        core = cq.Solid.makeCylinder(ROLLER_D / 2 - 0.4, 0.5, cq.Vector(x - 0.25, ROLLER_Y, ROLLER_Z),
                                     cq.Vector(1, 0, 0))
        r = r.cut(groove.cut(core))
    return place(r)


def feet():
    fs = []
    for x in (-FEET_X, FEET_X):
        for y in (OUT_Y_FRONT + 11, OUT_Y_BACK - 11):
            fs.append(cq.Solid.makeCylinder(5.0, 2.5, cq.Vector(x, y, FEET_DEPTH - 2.5)))
    return cq.Compound.makeCompound(fs)


# --- Output ------------------------------------------------------------------------------------

def report(parts):
    for name, shape in parts.items():
        bb = shape.BoundingBox()
        print(f"{name:12s} {bb.xlen:6.1f} x {bb.ylen:6.1f} x {bb.zlen:5.1f} mm, {shape.Volume() / 1000:7.1f} cm3")
    print(f"weight mass: {parts['Weight'].Volume() * BRASS_DENSITY:.0f} g (brass)")
    print(f"front height (top of the bezel at the front edge): {to_world(-(BOARD_H / 2 + BEZEL_FB), BEZEL_TOP)[1]:.1f} mm")
    print(f"back height: {to_world(BOARD_H / 2 + BEZEL_FB, BEZEL_TOP)[1]:.1f} mm")
    print(f"plate top at the front row edge: {to_world(-BOARD_H / 2, PLATE_Z0 + PLATE_T)[1]:.1f} mm")


def write_step(shape, path, name, color):
    a = cq.Assembly(name="Dachshund60F")
    a.add(shape, name=name, color=cq.Color(*color))
    a.save(path, "STEP")
    print(f"{os.path.relpath(path, HW)}: {os.path.getsize(path) / 1e6:.1f} MB")


def build_parts():
    shell = outer_shell()
    return {
        "Case top": top_case(shell),
        "Case bottom": bottom_case(shell),
        "Weight": weight(),
        "Plate foam": plate_foam(),
        "Gasket": gaskets(),
        "Battery": battery(),
        "Roller": roller(),
        "Feet": feet(),
    }


FILES = {
    "Case top": "Dachshund60F-Case-Top.step",
    "Case bottom": "Dachshund60F-Case-Bottom.step",
    "Weight": "Dachshund60F-Weight.step",
    "Gasket": "Dachshund60F-Gaskets.step",
}


EXPLODE = {"Case top": 70, "Board": 38, "Plate foam": 38, "Gasket": 38, "Roller": 38,
           "Weight": -45, "Feet": -60}


def main():
    os.makedirs(OUT, exist_ok=True)
    parts = build_parts()
    report(parts)
    for key, fname in FILES.items():
        write_step(parts[key], os.path.join(OUT, fname), key, COLORS[key])
    # The foam is cut from sheet, so it ships as a 2D outline (plate frame, mm) to extrude 3.5 mm.
    dxf = os.path.join(OUT, "Dachshund60F-Plate-Foam.dxf")
    cq.exporters.export(cq.Workplane("XY").add(foam_face()), dxf, "DXF")
    print(f"{os.path.relpath(dxf, HW)}: {os.path.getsize(dxf) / 1e6:.1f} MB")
    if "--parts" in sys.argv:
        return
    import assembly

    doc = assembly.write(parts, COLORS, os.path.join(OUT, "Dachshund60F-Case-Assembly.step"))
    if "--renders" in sys.argv:
        import tempfile

        import render

        with tempfile.TemporaryDirectory() as tmp:
            meshes = [os.path.join(tmp, n + ".npz") for n in ("assembled", "exploded", "inside")]
            assembly.write_meshes(doc, meshes[0])
            assembly.write_meshes(assembly.build(parts, COLORS, EXPLODE), meshes[1])
            assembly.write_meshes(assembly.build(parts, COLORS, skip=("Board", "Case top", "Roller",
                                                                      "Plate foam", "Gasket")), meshes[2])
            render.render(*meshes, os.path.join(HERE, "renders"))


if __name__ == "__main__":
    main()
