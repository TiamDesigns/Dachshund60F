"""Generate the Dachshund60F case, gaskets and plate foam, and the full assembly.

    python hardware/case/gen_case.py            # parts and assembly
    python hardware/case/gen_case.py --parts    # parts only (skips the slow board import)
    python hardware/case/gen_case.py --renders  # also renders PNGs to hardware/case/renders/

Needs CadQuery (pip install cadquery). Writes to hardware/case/step/:

- Dachshund60F-Case-Top.step      top case: bezel frame, upper half of the gasket pockets, and the
                                  sidebar hood (screen window and mount, LED holes, roller slot)
- Dachshund60F-Case-Bottom.step   bottom case: tub with a floor stepped by switch row, lower gasket
                                  seats, USB-C, battery tray, channels for rubber strip feet
- Dachshund60F-Plate-Foam.dxf     plate-PCB midlayer foam outline, to cut from 3.5 mm Poron
                                  (the plate's own holes, grown 0.3 mm; board-centred origin)
- Dachshund60F-Gaskets.step       the eight silicone socks on the plate tabs
- Dachshund60F-Case-Assembly.step everything above plus the mainboard, plate,
                                  USB-C daughterboard, and stand-ins for the battery, the
                                  roller, the screen and the rubber strips

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

# Gasket socks (silicone, 8 x 5 x 5 mm outside, the closed end round in plan; pocket 6.5 x 2.8 x
# 1.2 mm, centred, takes 1.2-1.6 mm plates).
SOCK_L, SOCK_W, SOCK_H = 8.0, 5.0, 5.0
SOCK_POCKET_L, SOCK_POCKET_W = 6.5, 2.8
SOCK_SQUEEZE = 0.4     # the case pocket is this much shorter than a free sock
POCKET_CLEAR = 0.15    # sideways clearance around a sock in its case pocket
TAB_X = [BOARD_W * f - BOARD_W / 2 for f in (1 / 8, 3 / 8, 5 / 8, 7 / 8)]  # plate tab centres
SOCK_Z0 = PLATE_MID - (SOCK_H - SOCK_SQUEEZE) / 2  # compressed sock bottom
SPLIT = SOCK_Z0        # top/bottom case seam (local Z): the bottom case is the socks' floor
BEZEL_TOP = PLATE_Z0 + PLATE_T + BEZEL_ABOVE_PLATE

# Top case screws: M3 x 10 socket head from below into M3 x 4 heat-set inserts (OD 4.2).
SCREWS = ([(x, -66.6) for x in (-79.6, 0.0, 79.6)] + [(x, 66.6) for x in (-79.6, -30.0, 30.0, 79.6)]
          + [(-163.9, 15.0), (163.9, 15.0)])  # the back row clears the USB-C daughterboard
SCREW_HOLE, SCREW_CBORE, SCREW_GRIP = 3.4, 5.8, 6.0
INSERT_D, INSERT_DEPTH = 4.2, 4.8

# Plate foam (Poron, 3.5 mm sheet, 3.4 mm installed between PCB top and plate bottom).
FOAM_HOLE_GROW = 0.3   # each plate hole, grown by this much
FOAM_EDGE_INSET = 0.3  # in from the board outline

# Stepped floor: under each switch row the floor rises to UNDER_PCB_STEP below the PCB at the row's
# front edge (local y of the boundaries between rows, front to back). The sidebar, the battery tray
# and the daughterboard corner stay at FLOOR (world XY boxes: x0, x1, y0, y1).
ROW_EDGES = [-40.45, -21.40, -2.36, 16.69, 38.12]
UNDER_PCB_STEP = 3.5
STEP_KEEPOUTS = [(125.0, 200.0, -200.0, 200.0),   # sidebar: SW1, SW2, J1, J2, the module
                 (41.0, 100.0, 10.0, 52.0),       # battery tray and lead exit
                 (-26.0, 26.0, 40.0, 200.0)]      # USB-C daughterboard

# USB-C daughterboard (hardware/step/Dachshund60F-USB.step, connector facing +Y): centred in the back
# wall, flat on a ledge, receptacle face USB_FACE_INSET inside the back face.
USB_X = 0.0            # world X of the connector centre
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
# hardware/3dmodels), so a roller bigger than ~12 mm across would hit the PCB. The 20 mm shaft ends at
# x = 135.05; the roller's blind bore takes the last 8 mm of it and the wheel carries on past the end.
ROLLER_X = (131.0, 143.25)
ROLLER_Y, ROLLER_Z, ROLLER_D = 50.025, PCB_T + 6.7, 12.0

# Sidebar hood (local frame): a pod over the right end of the board, its top at the height of a
# keycap's bottom edge. It holds the screen under a window and has a light channel over each of D3-D7.
# The encoder body is taller, so it gets a cover of its own behind the pod, and the hood steps down
# to bezel height around the roller, which shows through a slot.
KEYCAP_BOTTOM = PLATE_Z0 + PLATE_T + 6.6  # Cherry-profile skirt at rest (MX top housing: +6.2)
HOOD_X0 = 128.0            # left wall outside face (keycaps end at 127.0)
HOOD_Y0 = -39.75           # front wall outside face (SW614's keycap ends at -40.95)
HOOD_WALL = 1.6            # left wall; the front and step walls are HOOD_WALL_FB
HOOD_WALL_FB = 2.0
HOOD_BOTTOM = 7.6          # walls stop 1 mm above the plate
POD_TOP, POD_ROOF = KEYCAP_BOTTOM, 1.5
ENC_TOP = 17.0             # encoder cover (the body's top is at 15.0)
POD_FILLET = 1.5
STEP_X, STEP_Y = 143.6, 38.55  # the low part: x < STEP_X, y > STEP_Y (roller and bushing)
STEP_ROOF_Z = 8.5          # underside of the low roof; its top is BEZEL_TOP
ROLLER_SLOT = (130.5, 143.6, 43.5, 56.5)  # x0, x1, y0, y1
BUSHING_ARCH_R = 4.0       # clearance arch in the step wall over the encoder's bushing (Ø7)

# Status screen. The glass is taped to the roof's underside (DISPLAY_TAPE) behind a bevelled window
# over its active area. A frame under the roof locates either the module's board (OLED modules, whose
# glass sits on the board) or the glass itself (a bare e-paper panel, whose driver board is taped to the
# panel's back). Sizes are nominal: measure your parts and correct them before printing. Sizes are
# x (along the board), y, thickness; centres are local x, y. aa is the active area and aa_dy its offset
# from the glass centre towards the front; back is the height of the parts under the board. The wires
# (or the panel's flex cable) leave at the back, through a gap in the frame.
DISPLAYS = {
    # A bare 2.13" e-paper panel (GoodDisplay GDEY0213B74, SSD1680, 250 x 122) standing on end, 120 x 248
    # in portrait, flex cable at the back, folded under to an Adafruit eInk Breakout Friend (#4224).
    "eink-2.13": dict(overlay="eink-2in13-v4-portrait", frame="glass",
                      glass=(29.2, 59.2, 1.0), glass_c=(144.8, 3.1), aa=(23.71, 48.55), aa_dy=1.8,
                      pcb=(21.5, 34.7, 1.6), pcb_c=(141.8, 13.35), pcb_gap=0.1, back=1.9),
    # Pimoroni 1.12" Mono OLED (128x128, white/black) Breakout, I2C version: 28 x 42 mm, SH1107.
    "sh1107-1.12": dict(overlay="oled-sh1107-128x128", frame="pcb",
                        glass=(25.0, 27.0, 1.4), glass_c=(144.8, -9.5), aa=(20.1, 20.1), aa_dy=0.0,
                        pcb=(28.0, 42.0, 1.6), pcb_c=(144.8, -5.5), pcb_gap=0.0, back=1.0),
    # Generic 1.3" SH1106 128x64 I2C module (Pololu #3761 sizes). It is wider than the sidebar, so it
    # reaches into the right bezel, and the right-hand screw moves forward out of its way.
    "sh1106-1.3": dict(overlay="oled-sh1106-128x64", frame="pcb",
                       glass=(34.5, 23.0, 1.5), glass_c=(147.925, 0.25), aa=(29.42, 14.70), aa_dy=0.0,
                       pcb=(35.4, 33.5, 1.0), pcb_c=(147.925, 0.25), pcb_gap=0.0, back=1.0,
                       right_screw_y=-26.0),
}
DISPLAY = "eink-2.13"
SCREEN = DISPLAYS[DISPLAY]
SCREWS[-1] = (SCREWS[-1][0], SCREEN.get("right_screw_y", SCREWS[-1][1]))
DISPLAY_TAPE = 0.2           # glass to roof underside
DISPLAY_WINDOW_MARGIN = 0.8  # window edge outside the active area, each side
DISPLAY_FRAME_T, DISPLAY_CLEAR = 1.0, 0.3
DISPLAY_WIRE_GAP = 16.0      # width of the gap at the back of the frame
# The driver board stays in front of and left of the radio module's antenna (KiCad x > 333, y 67-79;
# local x > 154.75, y 20.5-32.5). Only the panel's glass reaches over it.

# LEDs D3-D7 (reverse mount on the back, shining up through the plate's 4 mm windows): a block of
# light channels from the roof down to 1 mm above the plate, sealed to the plate by a strip of black
# foam, so no LED lights its neighbour's hole. A diffuser disc sits in the top of each channel.
LED_X = [131.75, 138.25, 143.75, 149.25, 154.75]
LED_Y = -30.95
LED_HOLE_D = 3.0           # in the roof
LED_BORE_D = 4.0           # the channel, and the diffuser disc (4 mm, 1 mm opal acrylic)
LED_WALL = 1.5             # around the outer channels
LED_SEAL_T, LED_SEAL_HOLE_D = 1.5, 4.4  # black closed-cell foam, squeezed to 1.0 mm

# Feet: two strips of self-adhesive solid rubber (10 mm wide, 3 mm thick) in shallow channels.
STRIP_W, STRIP_T, STRIP_L = 10.0, 3.0, 290.0
STRIP_CHANNEL_W, STRIP_CHANNEL_DEPTH = 10.4, 1.0
STRIP_Y = (-56.0, 56.0)    # world Y of the strip centres (inboard of the screws)

COLORS = {
    "Case top": (0.42, 0.27, 0.17),
    "Case bottom": (0.85, 0.76, 0.61),
    "Plate foam": (0.93, 0.88, 0.80),
    "Gasket": (0.22, 0.22, 0.22),
    "Battery": (0.55, 0.62, 0.70),
    "Roller": (0.15, 0.13, 0.12),
    "Rubber strips": (0.10, 0.10, 0.10),
    "LED seal": (0.08, 0.08, 0.08),
    "Display board": (0.10, 0.25, 0.55),
    "Display glass": (0.06, 0.06, 0.08),
    "Display area": (0.35, 0.75, 0.95),
}
if SCREEN["frame"] == "glass":  # e-paper: a pale panel on a black driver board
    COLORS.update({"Display board": (0.08, 0.08, 0.08), "Display glass": (0.80, 0.80, 0.77),
                   "Display area": (0.90, 0.90, 0.86)})

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


def d_shape(x, y_open, s, length, width, z0, z1):
    """Plan-view D: straight from y_open, running length in direction s (+1/-1), with a round end."""
    r = width / 2
    yc = y_open + s * (length - r)
    y0, y1 = sorted((y_open, yc))
    return box(x - r, x + r, y0, y1, z0, z1).fuse(cyl_z(x, yc, width, z0, z1)).clean()


def sock_pockets():
    """Pockets for the socks (local frame), both halves; each case half keeps its own side."""
    cuts = []
    edge = BOARD_H / 2
    for x in TAB_X:
        for s in (-1, 1):
            cuts.append(d_shape(x, s * (edge - 0.5), s, SOCK_L + 0.5 + POCKET_CLEAR, SOCK_W + 2 * POCKET_CLEAR,
                                SOCK_Z0, SOCK_Z0 + SOCK_H - SOCK_SQUEEZE))
    return [place(c) for c in cuts]


def screw_points():
    pts = []
    for x, y in SCREWS:
        yw, zw = to_world(y, SPLIT)
        pts.append((x, yw, zw))
    return pts


# --- Stepped floor ----------------------------------------------------------------------------

def floor_steps():
    """World-horizontal steps on the floor, one per switch row, each UNDER_PCB_STEP below the PCB at
    the row's front edge (so the gap under the PCB stays about the same from front to back)."""
    steps = None
    for y in ROW_EDGES:
        yw, zw = to_world(y, -UNDER_PCB_STEP)
        if zw < FLOOR + 0.5:
            continue
        blk = box(-OUT_X, OUT_X, yw, OUT_Y_BACK, FLOOR - 0.5, zw)
        steps = blk if steps is None else steps.fuse(blk)
    for x0, x1, y0, y1 in STEP_KEEPOUTS:
        steps = steps.cut(box(x0, x1, y0, y1, 0, 60))
    return steps.intersect(cavity()).clean()


def extrude_face(face, z0, z1):
    f = face.translate((0, 0, z0 - face.Center().z))
    return cq.Solid.extrudeLinear(f, cq.Vector(0, 0, z1 - z0))


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
    b = b.fuse(floor_steps())
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
    # Channels for the rubber strips.
    for y in STRIP_Y:
        w = STRIP_CHANNEL_W
        b = b.cut(box(-STRIP_L / 2 - 1, STRIP_L / 2 + 1, y - w / 2, y + w / 2, -1, STRIP_CHANNEL_DEPTH))
    return b.clean()


ROOF_Z = POD_TOP - POD_ROOF  # roof underside


def outer_prism(z0=0.0, z1=80.0):
    yc, h = (OUT_Y_FRONT + OUT_Y_BACK) / 2, OUT_Y_BACK - OUT_Y_FRONT
    return rrect(2 * OUT_X, h, OUTER_R, z0, z1, y=yc)


def _rounded(solid):
    """Fillet a hood block's corners inside the opening (square to the board), then its top edge."""
    n = cq.Vector(0, -S, C)
    vertical = [e for e in solid.Edges() if e.geomType() == "LINE"
                and abs((e.endPoint() - e.startPoint()).normalized().dot(n)) > 0.999]
    if vertical:
        solid = solid.fillet(POD_FILLET, vertical)
    top = max((f for f in solid.Faces() if f.normalAt().dot(n) > 0.99), key=lambda f: f.Center().dot(n))
    return solid.fillet(POD_FILLET, top.outerWire().Edges())


def hood_solid():
    """The pod, the encoder cover and the low roof around the roller, before the cuts (world)."""
    pod = place(box(HOOD_X0, 300, HOOD_Y0, STEP_Y, HOOD_BOTTOM, POD_TOP)).intersect(outer_prism())
    pod = pod.fuse(place(box(STEP_X, 300, STEP_Y - 1, 300, HOOD_BOTTOM, POD_TOP)).intersect(outer_prism()))
    cover = place(box(STEP_X, 300, STEP_Y - HOOD_WALL_FB, 300, HOOD_BOTTOM, ENC_TOP)).intersect(outer_prism())
    low = place(box(HOOD_X0, STEP_X + 0.5, STEP_Y - 0.5, INNER_H / 2 + 1.5, STEP_ROOF_Z, BEZEL_TOP))
    return _rounded(pod.clean()).fuse(_rounded(cover)).fuse(low)


def led_block_xy():
    r = LED_BORE_D / 2 + LED_WALL
    return HOOD_X0 + HOOD_WALL, LED_X[-1] + r, HOOD_Y0 + HOOD_WALL_FB, LED_Y + r


def _rect(size, centre, grow=0.0):
    (w, h), (cx, cy) = size[:2], centre
    return cx - w / 2 - grow, cx + w / 2 + grow, cy - h / 2 - grow, cy + h / 2 + grow


def display_rect(part, grow=0.0):
    """Outline (local x0, x1, y0, y1) of the screen's "glass" or "pcb", grown by grow."""
    return _rect(SCREEN[part], SCREEN[part + "_c"], grow)


def display_z():
    """Local z of the glass top and bottom, the board top and bottom, and the bottom of the parts under it."""
    glass_top = ROOF_Z - DISPLAY_TAPE
    glass_bottom = glass_top - SCREEN["glass"][2]
    pcb_top = glass_bottom - SCREEN["pcb_gap"]
    pcb_bottom = pcb_top - SCREEN["pcb"][2]
    return glass_top, glass_bottom, pcb_top, pcb_bottom, pcb_bottom - SCREEN["back"]


def hood_inserts():
    """The LED channel block, and the frame that locates the screen."""
    x0, x1, y0, y1 = led_block_xy()
    parts = [box(x0, x1, y0, y1, HOOD_BOTTOM, ROOF_Z + 0.1)]
    _, glass_bottom, _, pcb_bottom, _ = display_z()
    part = SCREEN["frame"]
    z0 = glass_bottom - 1.0 if part == "glass" else pcb_bottom + 0.5
    frame = box(*display_rect(part, DISPLAY_CLEAR + DISPLAY_FRAME_T), z0, ROOF_Z + 0.1)
    frame = frame.cut(box(*display_rect(part, DISPLAY_CLEAR), 0, 40))
    cx = SCREEN[part + "_c"][0]
    frame = frame.cut(box(cx - DISPLAY_WIRE_GAP / 2, cx + DISPLAY_WIRE_GAP / 2, 0, 300, 0, 40))
    return place(parts[0].fuse(frame).clean())


def hood_cavity():
    """Inside of the hood and the roller slot (world)."""
    xi0, xi1 = HOOD_X0 + HOOD_WALL, INNER_W / 2
    yi0 = HOOD_Y0 + HOOD_WALL_FB
    ys = STEP_Y - HOOD_WALL_FB           # inside face of the step's front wall
    xs = STEP_X + HOOD_WALL_FB           # inside face of the step's right wall
    inner = rrect(INNER_W, INNER_H, INNER_R, 0, 40)
    back = display_z()[4]
    cuts = [
        box(xi0, xi1, yi0, ys, 6.0, ROOF_Z),                                   # under the pod
        box(xs, xi1, STEP_Y, INNER_H / 2, 6.0, ENC_TOP - POD_ROOF).intersect(inner),  # the encoder
        box(*display_rect(SCREEN["frame"], DISPLAY_CLEAR), back - 0.3, ROOF_Z),  # the screen (bezel too)
        box(*display_rect("pcb", DISPLAY_CLEAR), back - 0.3, ROOF_Z),
        box(xi0, xs, STEP_Y, INNER_H / 2, 6.0, STEP_ROOF_Z),                   # under the low roof
        cq.Solid.makeCylinder(BUSHING_ARCH_R, xs - STEP_X + 1.0,               # over the bushing
                              cq.Vector(STEP_X - 0.5, ROLLER_Y, ROLLER_Z), cq.Vector(1, 0, 0)),
        box(*ROLLER_SLOT, 0, 40),
    ]
    return [place(c) for c in cuts]


def hood_holes():
    """The screen window and the LED channels (world)."""
    cuts = []
    # Window: the active area plus a margin at the roof underside, opening 45 degrees to the top.
    cx, cy = SCREEN["glass_c"]
    cy -= SCREEN["aa_dy"]
    w, h = (a + 2 * DISPLAY_WINDOW_MARGIN for a in SCREEN["aa"])
    e = POD_ROOF + 0.2
    lo = cq.Wire.makePolygon([cq.Vector(cx + dx * w / 2, cy + dy * h / 2, ROOF_Z - 0.1)
                              for dx, dy in ((-1, -1), (1, -1), (1, 1), (-1, 1), (-1, -1))])
    hi = cq.Wire.makePolygon([cq.Vector(cx + dx * (w / 2 + e), cy + dy * (h / 2 + e), POD_TOP + 0.1)
                              for dx, dy in ((-1, -1), (1, -1), (1, 1), (-1, 1), (-1, -1))])
    cuts.append(cq.Solid.makeLoft([lo, hi], True))
    for x in LED_X:
        cuts.append(cyl_z(x, LED_Y, LED_BORE_D, HOOD_BOTTOM - 1, ROOF_Z))
        cuts.append(cyl_z(x, LED_Y, LED_HOLE_D, ROOF_Z - 0.1, POD_TOP + 1))
    return [place(c) for c in cuts]


def led_seal_face():
    """The black foam strip under the LED block, with a hole over each LED (local XY, z = 0)."""
    x0, x1, y0, y1 = led_block_xy()
    f = box(x0, x1, y0, y1, -1, 1)
    for x in LED_X:
        f = f.cut(cyl_z(x, LED_Y, LED_SEAL_HOLE_D, -2, 2))
    return min((fc for fc in f.Faces() if fc.normalAt().z < -0.99), key=lambda fc: fc.Center().z).translate((0, 0, 1))


def led_seal():
    return place(extrude_face(led_seal_face(), PLATE_Z0 + PLATE_T, HOOD_BOTTOM))


def top_case(shell):
    t = shell.cut(half_space_local(SPLIT, False))
    t = _seam_reveal(t, cq.Vector(0, S, -C))
    t = t.cut(inner_prism(-50, 50))
    # Chamfer the inside edge of the bezel.
    n = cq.Vector(0, -S, C)
    top = max((f for f in t.Faces() if f.normalAt().dot(n) > 0.99), key=lambda f: f.Center().dot(n))
    inner = [e for w in top.innerWires() for e in w.Edges()]
    t = t.chamfer(INNER_CHAMFER, None, inner)
    t = t.fuse(hood_solid())
    for c in hood_cavity():
        t = t.cut(c)
    t = t.fuse(hood_inserts())
    for c in hood_holes():
        t = t.cut(c)
    for p in sock_pockets():
        t = t.cut(p)
    for x, y, z in screw_points():
        t = t.cut(cyl_z(x, y, INSERT_D, z - 1, z + INSERT_DEPTH))
    return t.clean()


# --- Gaskets, foam, stand-ins ------------------------------------------------------------------

def gaskets():
    socks = []
    h = SOCK_H - SOCK_SQUEEZE
    for x in TAB_X:
        for s in (-1, 1):
            edge = s * BOARD_H / 2
            sock = d_shape(x, edge, s, SOCK_L, SOCK_W, SOCK_Z0, SOCK_Z0 + h)
            sock = sock.fillet(0.6, [e for e in sock.Edges() if e.geomType() != "LINE"
                                     or abs(e.tangentAt(0).z) < 0.5])
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


def strips():
    z1 = STRIP_CHANNEL_DEPTH
    return cq.Compound.makeCompound([box(-STRIP_L / 2, STRIP_L / 2, y - STRIP_W / 2, y + STRIP_W / 2,
                                         z1 - STRIP_T, z1) for y in STRIP_Y])


def display():
    """Stand-in screen: driver or module board, glass and active area."""
    glass_top, glass_bottom, pcb_top, pcb_bottom, _ = display_z()
    pcb = box(*display_rect("pcb"), pcb_bottom, pcb_top)
    glass = box(*display_rect("glass"), glass_bottom, glass_top)
    cx, cy = SCREEN["glass_c"]
    aw, ah = SCREEN["aa"]
    aa = box(*_rect((aw, ah), (cx, cy - SCREEN["aa_dy"])), glass_top, glass_top + 0.05)
    return place(pcb), place(glass), place(aa)


# --- Output ------------------------------------------------------------------------------------

def report(parts):
    for name, shape in parts.items():
        bb = shape.BoundingBox()
        print(f"{name:12s} {bb.xlen:6.1f} x {bb.ylen:6.1f} x {bb.zlen:5.1f} mm, {shape.Volume() / 1000:7.1f} cm3")
    print(f"front height (top of the bezel at the front edge): {to_world(-(BOARD_H / 2 + BEZEL_FB), BEZEL_TOP)[1]:.1f} mm")
    print(f"back height: {to_world(BOARD_H / 2 + BEZEL_FB, BEZEL_TOP)[1]:.1f} mm")
    print(f"plate top at the front row edge: {to_world(-BOARD_H / 2, PLATE_Z0 + PLATE_T)[1]:.1f} mm")
    print(f"hood top: {to_world(HOOD_Y0, POD_TOP)[1]:.1f} mm at its front, {POD_TOP - BEZEL_TOP:.1f} mm "
          f"above the bezel; encoder cover {ENC_TOP - BEZEL_TOP:.1f} mm above the bezel")
    for y in ROW_EDGES:
        yw, zw = to_world(y, -UNDER_PCB_STEP)
        print(f"floor step at world y {yw:6.1f}: {max(zw, FLOOR):.1f} mm")


def write_step(shape, path, name, color):
    a = cq.Assembly(name="Dachshund60F")
    a.add(shape, name=name, color=cq.Color(*color))
    a.save(path, "STEP")
    print(f"{os.path.relpath(path, HW)}: {os.path.getsize(path) / 1e6:.1f} MB")


def build_parts():
    shell = outer_shell()
    board, glass, screen = display()
    return {
        "Case top": top_case(shell),
        "Case bottom": bottom_case(shell),
        "Plate foam": plate_foam(),
        "Gasket": gaskets(),
        "Battery": battery(),
        "Roller": roller(),
        "Rubber strips": strips(),
        "LED seal": led_seal(),
        "Display board": board,
        "Display glass": glass,
        "Display area": screen,
    }


FILES = {
    "Case top": "Dachshund60F-Case-Top.step",
    "Case bottom": "Dachshund60F-Case-Bottom.step",
    "Gasket": "Dachshund60F-Gaskets.step",
}


EXPLODE = {"Case top": 70, "Display board": 70, "Display glass": 70, "Display area": 70, "LED seal": 50,
           "Board": 38, "Plate foam": 38, "Gasket": 38, "Roller": 38, "Rubber strips": -40}


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
    # The LED seal: a strip of 1.5 mm black closed-cell foam (same frame).
    dxf = os.path.join(OUT, "Dachshund60F-LED-Seal.dxf")
    cq.exporters.export(cq.Workplane("XY").add(led_seal_face()), dxf, "DXF")
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
                                                                      "Plate foam", "Gasket", "Display board",
                                                                      "Display glass", "Display area",
                                                                      "LED seal")), meshes[2])
            render.render(*meshes, os.path.join(HERE, "renders"))


if __name__ == "__main__":
    main()
