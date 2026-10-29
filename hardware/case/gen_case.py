"""Generate the Dachshund60F case, gaskets and plate foam, and the full assembly.

    python hardware/case/gen_case.py            # parts and assembly
    python hardware/case/gen_case.py --parts    # parts only (skips the slow board import)
    python hardware/case/gen_case.py --renders  # also renders PNGs to hardware/case/renders/

Needs CadQuery (pip install cadquery). Writes to hardware/case/step/:

- Dachshund60F-Case-Top.step      top case (aluminium): bezel at keycap-bottom height with a 1 mm gap
                                  round the keys, upper half of the gasket pockets, a flat roof over
                                  the sidebar (screen window and mount, LED holes, roller slot) and
                                  a round hump over the encoder
- Dachshund60F-Case-Bottom.step   bottom case (aluminium): tub with a floor stepped by switch row,
                                  lower gasket seats, USB-C, battery tray, pockets for the feet
- Dachshund60F-Case-Panel.step    a polycarbonate panel in the middle of the right end (side wall and
                                  the floor inside it): next to and under the antenna, with the power
                                  switch's cap
- Dachshund60F-Switch-Cap.step    the power switch's slider and grip
- Dachshund60F-Plate-Foam.dxf     plate-PCB midlayer foam outline, to cut from 3.5 mm Poron
                                  (the plate's own holes, grown 0.3 mm; board-centred origin)
- Dachshund60F-Gaskets.step       the eight silicone socks on the plate tabs
- Dachshund60F-Case-Assembly.step everything above plus the mainboard, plate,
                                  USB-C daughterboard, and stand-ins for the battery, the
                                  roller, the screen and the feet

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
from OCP.BRep import BRep_Tool
from OCP.BRepBuilderAPI import (BRepBuilderAPI_MakeEdge, BRepBuilderAPI_MakeFace, BRepBuilderAPI_MakeSolid,
                                BRepBuilderAPI_MakeWire, BRepBuilderAPI_NurbsConvert, BRepBuilderAPI_Sewing)
from OCP.BRepLib import BRepLib
from OCP.BRepOffsetAPI import BRepOffsetAPI_ThruSections
from OCP.BRepPrimAPI import BRepPrimAPI_MakePrism
from OCP.Geom import Geom_BSplineCurve
from OCP.Geom2dAPI import Geom2dAPI_Interpolate
from OCP.ShapeFix import ShapeFix_Face, ShapeFix_Shell
from OCP.TColgp import TColgp_Array1OfPnt, TColgp_HArray1OfPnt2d
from OCP.TColStd import TColStd_Array1OfInteger, TColStd_Array1OfReal, TColStd_HArray1OfReal
from OCP.TopAbs import TopAbs_EDGE, TopAbs_FACE
from OCP.TopExp import TopExp
from OCP.TopoDS import TopoDS
from OCP.TopTools import TopTools_IndexedDataMapOfShapeListOfShape
from OCP.gp import gp_Pnt, gp_Pnt2d, gp_Vec

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
BEZEL_SIDE = 6.5       # wall outside the outline on the ends (no gaskets or screws there)
BEZEL_FB = 12.0        # wall outside the outline front and back (the socks reach 8 mm out)
# Plan-view corners: continuous curvature (as on Apple's products), not circular arcs. Each corner is a
# superellipse quadrant that leaves the straight edge with zero curvature and eases in, so there's no
# visible kink where the arc starts. CORNER_L is how far along each edge the curve runs (about the look
# of a 7 mm radius); CORNER_N is its exponent (2 would be a circle).
CORNER_L, CORNER_N = 6.0, 5.0
TOP_ROLL = 1.5         # the top edge's continuous-curvature roll: how far it runs down the wall and across the top
SEAM_REVEAL = 0.4      # chamfer on both halves at the seam, so it reads as a groove
INNER_CHAMFER = 0.3    # chamfer on the key opening's top edge
FOOT_CHAMFER = 0.6     # bottom edge, so the case seems to float
BEZEL_ABOVE_PLATE = 6.6  # bezel top above the plate top: a keycap's bottom edge, so no switch shows
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
SPLIT = SOCK_Z0 + 1.0  # top/bottom case seam (local Z): the socks sit 1 mm deep in seats in the bottom case
BEZEL_TOP = PLATE_Z0 + PLATE_T + BEZEL_ABOVE_PLATE

# Top case screws: M3 x 10 socket head from below into tapped M3 holes (heat-set inserts are for plastic;
# in aluminium the thread is tapped: a 2.5 mm drill, TAP_DEPTH of full thread (1.7 x d, plenty in 6061), the
# drill 0.8 mm deeper; under the thumb scoop there's no room for more).
# Eight: four along the front and four along the back at an even 101 mm pitch (x = +-50.5, +-151.5), each
# at least 10.7 mm from the nearest sock and clear of the USB-C daughterboard. None in the ends. Underneath
# they keep one 50.5 mm rhythm with the feet (FOOT_X) halfway between them, and the weight between the
# middle pair.
SCREW_X = (-151.5, -50.5, 50.5, 151.5)
SCREWS = [(x, y) for y in (-66.6, 66.6) for x in SCREW_X]
# Two steel dowel pins (3 mm m6 x 8) at opposite corners locate the top case on the bottom before the screws
# go in: pressed into the bottom case, a sliding fit in the top case, the second hole slotted along the line
# between them so the pair can't bind.
DOWELS = [(-101.0, -66.6), (101.0, 66.6)]  # over the feet, between the screws
DOWEL_D, DOWEL_L = 3.0, 8.0
DOWEL_PRESS_D, DOWEL_SLIP_D, DOWEL_SLOT = 2.98, 3.05, 0.6
SCREW_HOLE, SCREW_CBORE, SCREW_GRIP = 3.4, 5.8, 6.0
TAP_DRILL, TAP_DEPTH = 2.5, 5.0
# CNC (3-axis, 6061): inside corners along the cutter's axis get a radius of at least CNC_MIN_R and a third
# of the pocket's depth; pockets that take square-cornered parts (the screen) get DOGBONE_D reliefs.
CNC_MIN_R, CNC_MAX_R = 1.0, 3.0
DOGBONE_D = 2.0
# Tongue and groove at the seam: a tongue on the top case's lower face in a groove in the bottom case's top
# face, both following the outline (offsets in from the outside; heights from the seam). 0.15 mm a side
# and 0.2 mm under the tongue, so the halves seat on their faces. It runs over the polycarbonate panel too
# (it keeps the panel in), and stops only at the power switch and the roller's notch.
TONGUE = (1.75, 2.95, 1.5)
GROOVE = (1.6, 3.1, 1.7)
TONGUE_GAP_ZONES = [(145.0, -22.0, -2.0), (145.0, 38.0, 62.0)]  # world x from, y from, y to

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
USB_Z = 10.5           # world Z of the daughterboard's bottom (2.3 mm under the hot-swap sockets)
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
SW1_LOCAL = (157.25, -12.45)     # power slide switch, actuator out of the right edge (x 159.2-160.4)
SW2_LOCAL = (136.75, -6.45)      # reset button on the back
RESET_PIN_D = 2.2

# The bottom case is aluminium except a polycarbonate panel in the middle of its right end: the side wall
# and the floor inside it, from the power switch to past the radio module's antenna (world x0, y0, y1).
# The top case over it stays aluminium.
CASE_PANEL = (150.0, -32.0, 36.0)
PANEL_LAP = 4.0

# Power switch cap (SW1, C&K PCM12, slides along y). A slider in a pocket in the end wall's inside face
# takes the switch's actuator in a notch open at the top, so the board drops onto it; its stem runs out
# through a window to a ridged grip in a recess in the end face. The grip is pressed (or glued) onto the
# stem from outside once the slider is in. CAP_TRAVEL is the switch's (C&K: 1.5 mm, check yours). Its two
# positions sit either side of the switch's middle (its outer pins are symmetric), and every pocket is the
# part's length plus the travel plus CAP_END_CLEAR at each end: so at on and at off the switch's own detent
# holds the cap, with its grip just short of the recess's end wall (0.15-0.35 mm, with the notch's play).
# No off-the-shelf actuator extension fits a PCM12, so the cap is made (printed or cut).
CAP_TRAVEL = 1.5
SW1_Y = KICAD_ORIGIN[1] - 112.0  # local y of the switch's middle (SW1 at KiCad y 112)
CAP_END_CLEAR = 0.25
CAP_Y, CAP_Z = -11.87, -0.89  # local y, z of the actuator's middle in the board model (it hangs under the PCB)
CAP_ACTUATOR = (1.5, 1.4)     # notch for the actuator (1.3 x 0.8): across the slide (y), depth (x)
CAP_FLANGE = (2.1, 11.0, 4.2)  # slider flange in the wall: x, y, z
CAP_STEM = (6.0, 2.4)         # y, z
CAP_GRIP = (2.3, 14.0, 5.5)   # x (its face flush with the end face), y, z
CAP_CLEAR = 0.25

# Roller on the encoder shaft. SW3 (Bourns PEC12R-2230F-S0024, 30 mm shaft) is turned so its shaft points
# right, its axis 10.0 mm above the PCB top: the body is at x 132.55-138.55 (13.4 mm tall, to 16.7 mm above the PCB
# top), its hub (7 mm across) to 143.55 and the shaft's tip at 162.55. The roller's blind bore takes the
# shaft from just past the hub to its tip, and the wheel carries on through a notch in the end wall to
# just inside the end face, so a finger can push its end in: SW3's switch is pressed along the shaft
# (0.5 mm of travel). Its bottom clears the PCB top by 0.5 mm.
ENC_AXIS_Y, ENC_AXIS_Z = 50.025, PCB_T + 10.0
ENC_BODY = (132.55, 138.55, 43.775, 56.275, PCB_T + 3.3, PCB_T + 16.7)  # x0, x1, y0, y1, z0, z1
ENC_HUB_X1, ENC_HUB_D = 143.55, 7.0
ROLLER_X = (144.5, BOARD_W / 2 + BEZEL_SIDE - 0.25)
ROLLER_Y, ROLLER_Z, ROLLER_D = ENC_AXIS_Y, ENC_AXIS_Z, 19.0
# Its look follows the Lemokey L1's corner wheel: a straight knurl round the rim says "roll", and a
# contrasting cap set into its end says "push".
WHEEL_RIBS, WHEEL_RIB_GAP, WHEEL_RIB_DEPTH = 44, 0.6, 0.5
WHEEL_CHAMFER = 0.5
WHEEL_CAP_D, WHEEL_CAP_T, WHEEL_CAP_DOME, WHEEL_CAP_SINK = 11.0, 1.5, 0.5, 0.6

# Top case. One bezel height all round, at a keycap's bottom edge, with the key opening 1 mm outside the
# keycaps. Over the sidebar the bezel is a flat roof that holds the screen under a window and has a light
# channel over each of D3-D7. The encoder's body stands above it, under a hump: a cylinder coaxial with
# the roller, so the hump and the wheel read as one round shape.
KEYCAP_BOTTOM = PLATE_Z0 + PLATE_T + 6.6  # Cherry-profile skirt at rest (MX top housing: +6.2)
KEY_GAP = 1.0              # keycap edge to the opening
KEYCAP_INSET = 0.525       # keycap edge inside its 19.05 mm grid square
# The key grid (local): rows 0-4 end at KEY_GRID_X1; the bottom row (in front of KEY_GRID_ROW5_Y) runs on
# to KEY_GRID_ROW5_X1, under the sidebar's front.
KEY_GRID = (-158.25, 127.5, -59.513, 59.55)
KEY_GRID_ROW5_Y, KEY_GRID_ROW5_X1 = -40.46, 146.55
OPENING_R = 1.0            # plan-view corner radius of the opening
# The bottom row has a 0.5 u gap between the right Ctrl (ends at 12.5 u) and the left arrow (13 u), under
# the right Shift: the bezel comes back into it as a tongue, KEY_GAP from the keycaps on all three sides.
ARROW_GAP_X = (KEY_GRID[0] + 12.5 * 19.05, KEY_GRID[0] + 13 * 19.05)
# The F row stands 0.25 u behind the number row, in blocks of four (gaps after Esc, F4, F8 and F12, in units
# from the grid's left edge): the bezel fills those gaps too, a bar across the board and four posts.
F_ROW_GAP_U = 0.25
F_ROW_POSTS_U = (1.0, 5.25, 9.5, 13.75)
F_ROW_RIB_CLEAR = 1.0  # the bar and posts run down as ribs (stiffer than the 1.5 mm roof) to this far above the plate
# Thumb scoop: across the whole front, the bezel stays flat for FRONT_SCOOP_FLAT in front of the key
# opening, then its top swoops down to the outer edge, FRONT_SCOOP_DROP lower there, and is still falling
# when it reaches the front's roll. The fall is the quintic Bezier with its heights at 0, 0, 0, 1/3, 2/3, 1
# times the drop (z = BEZEL_TOP - drop * (10/3 t^3 (1 - t) + t^5), t = 0 at its start, 1 at the edge): it
# leaves the flat with no change in slope or curvature, steepens, and runs straight (5/3 of the mean slope,
# no curvature) into the roll, so it never flattens out again near the edge.
FRONT_SCOOP_DROP, FRONT_SCOOP_FLAT = 3.5, 2.5
ROOF_T = 1.5               # bezel roof over the board edge and the sidebar
HUMP_R = ROLLER_D / 2 + 0.8  # outside radius of the hump over the encoder body (1 mm wall over the pocket)
HUMP_X = (130.15, 144.0)
HUMP_FILLET = 1.0
ENC_CLEAR = 0.5            # body to the hump's pocket, all round
ENC_POCKET_R = 1.5         # pocket corners (leaves 0.85 mm of wall at the hump's shoulders)
ROLLER_SLOT_CLEAR = 0.3    # wheel to the slot's sides

# The sidebar as seen from above runs from the key opening's right edge to the end face; the screen and the
# LED row are centred on it (the glass reaches 1.75 mm into the end wall, in a slot up from the seam).
SIDEBAR_MID = (KEY_GRID[1] + KEY_GAP - KEYCAP_INSET + BOARD_W / 2 + BEZEL_SIDE) / 2

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
                      glass=(29.2, 59.2, 1.0), glass_c=(SIDEBAR_MID, 3.1), aa=(23.71, 48.55), aa_dy=1.8,
                      pcb=(21.5, 34.7, 1.6), pcb_c=(143.0, 13.35), pcb_gap=0.1, back=1.9),
    # Pimoroni 1.12" Mono OLED (128x128, white/black) Breakout, I2C version: 28 x 42 mm, SH1107.
    "sh1107-1.12": dict(overlay="oled-sh1107-128x128", frame="pcb",
                        glass=(25.0, 27.0, 1.4), glass_c=(144.8, -9.5), aa=(20.1, 20.1), aa_dy=0.0,
                        pcb=(28.0, 42.0, 1.6), pcb_c=(144.8, -5.5), pcb_gap=0.0, back=1.0),
    # Generic 1.3" SH1106 128x64 I2C module (Pololu #3761 sizes). It is wider than the sidebar, so it
    # reaches into the right bezel (check it against the 6.5 mm ends before using it).
    "sh1106-1.3": dict(overlay="oled-sh1106-128x64", frame="pcb",
                       glass=(34.5, 23.0, 1.5), glass_c=(147.925, 0.25), aa=(29.42, 14.70), aa_dy=0.0,
                       pcb=(35.4, 33.5, 1.0), pcb_c=(147.925, 0.25), pcb_gap=0.0, back=1.0),
}
DISPLAY = "eink-2.13"
SCREEN = DISPLAYS[DISPLAY]
DISPLAY_TAPE = 0.2           # glass to roof underside
DISPLAY_WINDOW_MARGIN = 0.8  # window edge outside the active area, each side
DISPLAY_FRAME_T, DISPLAY_CLEAR = 1.3, 0.3
DISPLAY_WIRE_GAP = 16.0      # width of the gap at the back of the frame
J4_BOX = (129.6, 149.9, -14.2, -11.7, 10.15)  # J4's 8-pin header in the board model: x0, x1, y0, y1, top
# The driver board stays in front of and left of the radio module's antenna (KiCad x > 333, y 67-79;
# local x > 154.75, y 20.5-32.5), 1 mm clear. Only the panel's glass reaches over it.

# LEDs D3-D7 (reverse mount on the back, shining up through the plate's 4 mm windows). D3 shows something
# different from D4-D7, so it stands apart (an 8 mm gap, the others 5.5 mm), with a score across the roof
# in the gap. Over them a 3D-printed light guide (opaque, black or white inside) stands from 1 mm above the
# plate to the roof's underside, a channel straight up from each LED, sealed to the plate by a strip of
# black foam so no LED lights its neighbour's hole. A clear plug in each 3 mm roof hole, flush with the
# top, is the diffuser: its 4 mm head sits in a counterbore at the top of the guide's channel, so the plugs
# locate the guide, and the guide holds the plugs up in the roof.
LED_X = [132.25, 140.25, 145.75, 151.25, 156.75]  # KiCad x 310.5, 318.5, 324.0, 329.5, 335.0
LED_Y = -30.95
LED_HOLE_D = 3.0           # in the roof
LED_CHANNEL_D = 3.2        # the guide's channel
LED_HEAD_D, LED_HEAD_T = 4.0, 1.0  # the plug's head and the counterbore it sits in
LED_PLUG_CLEAR = 0.05      # plug to hole and counterbore, each side (glue or a light press)
LED_GUIDE_BOTTOM = PLATE_Z0 + PLATE_T + 1.0  # 1 mm above the plate (the seal fills it)
LED_GUIDE_Y = (LED_Y - 3.0, LED_Y + 2.75)    # its back face 0.1 mm off the screen's frame
LED_GUIDE_WALL = 1.0       # at the ends (the right end stays inside the end wall)
LED_SEAL_T, LED_SEAL_HOLE_D = 1.5, 4.4  # black closed-cell foam, squeezed to 1.0 mm
LED_SCORE_W, LED_SCORE_L = 0.6, 5.0     # the score between D3 and D4: a 90-degree V, 0.3 mm deep

# Feet: four splitkb SKUF silicone feet (41.8 x 5.8 x 2 mm, 3M 9488A adhesive), each in a 1 mm pocket, so
# they stand 1 mm proud: two along the front and two along the back, each halfway between two screws.
FOOT_L, FOOT_W, FOOT_T = 41.8, 5.8, 2.0
FOOT_POCKET_L, FOOT_POCKET_W, FOOT_POCKET_DEPTH = 42.4, 6.4, 1.0  # the usual SKUF pocket
FOOT_X = (-101.0, 101.0)               # world X of the foot centres: halfway between the screws
FOOT_Y = (-56.0, 56.0)                 # world Y of the foot centres

# Weight: a brass block set into the bottom between the middle screws, flush with the bottom face and
# engraved there with a dachshund (a single-line side view) and the board's name. Its top steps up with
# the floor's steps inside, leaving WEIGHT_FLOOR of aluminium over it everywhere, and four M3 x 5 button
# head screws (ISO 7380) from inside, in 1 mm counterbores, hold it into tapped holes.
WEIGHT_W, WEIGHT_H = 80.0, 64.0  # world x, y, centred on the bottom face
WEIGHT_FLOOR = 2.0               # aluminium left over it
WEIGHT_R, WEIGHT_CLEAR, WEIGHT_CHAMFER = 3.0, 0.1, 0.4  # plan corners, pocket clearance, visible edge
WEIGHT_SCREWS = [(x, y) for y in (7.5, 23.5) for x in (-30.0, 30.0)]  # world, where it's thickest
WEIGHT_SCREW_CBORE = (6.2, 1.0)  # for the button heads, in the floor's top
ENGRAVE_DEPTH, ENGRAVE_W = 0.3, 0.6  # a 0.6 mm engraving cutter
ENGRAVE_FONT = "/usr/share/fonts/opentype/inter/InterDisplay-Medium.otf"
ENGRAVE_TEXT, ENGRAVE_TEXT_SIZE = "Dachshund60F", 6.5
# The dachshund, facing right, as the strokes of one line drawing (mm, x right, y up as you look at the
# bottom with the board turned over front to back): tail, back, head and snout down the chest to the
# front paw; the rump and back leg; the belly between the legs; the ear.
DOG = [
    [(0.0, 16.0), (3.4, 13.4), (7.6, 11.8), (12, 11.6), (22, 11.9), (32, 11.9), (40, 12.6), (44.5, 15.2),
     (47.5, 18.6), (51, 19.6), (54.5, 18.6), (60, 16.0), (62.6, 14.6), (61.6, 13.4), (55.5, 13.0),
     (51.5, 11.8), (49.0, 8.6), (48.6, 5.0), (49.2, 1.2), (51.4, 0.0)],
    [(7.0, 11.2), (5.0, 8.6), (5.0, 4.6), (6.4, 1.6), (9.4, 0.0)],
    [(47.0, 0.0), (45.4, 1.6), (44.8, 5.2), (40, 5.9), (20, 5.9), (14.4, 5.4), (13.4, 2.0), (14.6, 0.0)],
    [(51.5, 18.4), (49.3, 15.4), (49.6, 11.8), (51.2, 12.6)],
]
DOG_SCALE, DOG_V, TEXT_V = 0.85, 2.0, -15.0  # the drawing's size, and where it and the name sit (up from the middle)

COLORS = {
    "Case top": (0.62, 0.63, 0.65),     # bead-blasted aluminium
    "Case bottom": (0.55, 0.56, 0.58),
    "Case panel": (0.20, 0.22, 0.26),   # smoked polycarbonate
    "Switch cap": (0.12, 0.12, 0.12),
    "Plate foam": (0.93, 0.88, 0.80),
    "Gasket": (0.22, 0.22, 0.22),
    "Battery": (0.55, 0.62, 0.70),
    "Roller": (0.15, 0.13, 0.12),
    "Wheel cap": (0.90, 0.55, 0.15),  # amber
    "Feet": (0.10, 0.10, 0.10),
    "Dowels": (0.75, 0.75, 0.78),
    "LED seal": (0.08, 0.08, 0.08),
    "LED guide": (0.12, 0.12, 0.13),     # printed, opaque
    "LED plugs": (0.92, 0.94, 0.95),     # clear, frosted
    "Weight": (0.80, 0.64, 0.32),        # brass
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


def squircle(w, h, z0, z1, x=0.0, y=0.0, size=CORNER_L, n=CORNER_N, samples=24):
    """A w x h prism with continuous-curvature corners (superellipse quadrants, each running size along
    both edges), from z0 to z1."""
    corners = [(1, 1), (-1, 1), (-1, -1), (1, -1)]  # counter-clockwise, from the top right

    def pt(sx, sy, t):  # t = 0 on the side edge, pi/2 on the front or back edge
        cx, cy = x + sx * (w / 2 - size), y + sy * (h / 2 - size)
        return cq.Vector(cx + sx * size * max(math.cos(t), 0.0) ** (2 / n),
                         cy + sy * size * max(math.sin(t), 0.0) ** (2 / n), z0)

    loop = []
    for sx, sy in corners:
        pts = [pt(sx, sy, math.pi / 2 * k / samples) for k in range(samples + 1)]
        loop.append(pts if sx * sy > 0 else pts[::-1])
    edges = []
    for i, pts in enumerate(loop):
        prev, nxt = loop[i - 1][-1], loop[(i + 1) % 4][0]
        # Leave and join the straight edges along them (zero curvature there, so the joins are smooth).
        tangents = [(pts[0] - prev).normalized(), (nxt - pts[-1]).normalized()]
        edges.append(cq.Edge.makeSpline(pts, tangents=tangents))
        edges.append(cq.Edge.makeLine(pts[-1], nxt))
    face = cq.Face.makeFromWires(cq.Wire.assembleEdges(edges))
    return cq.Solid.extrudeLinear(face, cq.Vector(0, 0, z1 - z0))


def box(x0, x1, y0, y1, z0, z1):
    return cq.Solid.makeBox(x1 - x0, y1 - y0, z1 - z0, cq.Vector(x0, y0, z0))


def round_edges_at(solid, x, r, axis=(0, 0, 1)):
    """Fillet the solid's straight edges along axis that lie at x."""
    a = cq.Vector(*axis)
    edges = [e for e in solid.Edges() if e.geomType() == "LINE" and abs(e.tangentAt(0).dot(a)) > 0.999
             and abs(e.Center().x - x) < 1e-6]
    return solid.fillet(r, edges)


def cnc_round(shape, axis, skip=lambda e: False):
    """Round the sharp inside corners whose edges run along the cutter's axis: radius a third of the edge's
    length (the pocket's depth), CNC_MIN_R to CNC_MAX_R. Returns the shape and any edge it could not round."""
    a = cq.Vector(*axis)
    m = TopTools_IndexedDataMapOfShapeListOfShape()
    TopExp.MapShapesAndAncestors_s(shape.wrapped, TopAbs_EDGE, TopAbs_FACE, m)
    todo = []
    for i in range(1, m.Extent() + 1):
        e = cq.Edge(m.FindKey(i))
        if e.geomType() != "LINE" or abs(e.tangentAt(0.5).dot(a)) < 0.9995 or skip(e):
            continue
        faces = [cq.Face(f) for f in m.FindFromIndex(i)]
        if len(faces) != 2:
            continue
        mid, t = e.positionAt(0.5), e.tangentAt(0.5)
        ns, ds = [], []
        for f in faces:
            n = f.normalAt(mid)
            d = n.cross(t).normalized()
            if f.distance(cq.Vertex.makeVertex(*(mid + d * 0.02).toTuple())) > 1e-3:
                d = d * -1
            ns.append(n)
            ds.append(d)
        if abs(ns[0].dot(ns[1])) > 0.985 or (ds[0] + ds[1]).Length < 1e-6:
            continue
        if not shape.isInside(mid + (ds[0] + ds[1]).normalized() * 0.05, 1e-5):
            todo.append(e)
    failed = []
    for e in sorted(todo, key=lambda e: -e.Length()):
        r = min(max(e.Length() / 3, CNC_MIN_R), CNC_MAX_R)
        for rr in (r, CNC_MIN_R):
            try:
                hits = [x for x in shape.Edges() if x.geomType() == "LINE"
                        and (x.Center() - e.Center()).Length < 1e-4 and abs(x.Length() - e.Length()) < 1e-4]
                if not hits:
                    break  # already rounded with a neighbour
                new = shape.fillet(rr, hits)
                if new.isValid():
                    shape = new
                    break
            except Exception:
                pass
        else:
            failed.append(e.Center().toTuple())
    return shape, failed


def dogbone_box(x0, x1, y0, y1, z0, z1, d=DOGBONE_D):
    """A pocket for a square-cornered part: a box with a round relief through each corner, so a cutter of
    diameter d can reach the corners."""
    b = box(x0, x1, y0, y1, z0, z1)
    o = d / 2 / math.sqrt(2)
    for x, sx in ((x0, 1), (x1, -1)):
        for y, sy in ((y0, 1), (y1, -1)):
            b = b.fuse(cyl_z(x + sx * o, y + sy * o, d, z0, z1))
    return b.clean()


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


def seam_groove():
    """The groove cut round the seam before the case is split there: SEAM_REVEAL deep and as far up and
    down the wall, with a 0.2 mm flat at its root, so each half keeps a chamfered edge."""
    pts, nrms, arc = outline(OUTLINE_STEP, OUTLINE_SAMPLES)
    rings = {"low": [], "tip0": [], "tip1": [], "high": []}
    for (x, y), (nx, ny) in zip(pts, nrms):
        z = to_world((y + SPLIT * S) / C, SPLIT)[1]  # the seam's height on the wall here
        out, d = 1.0, SEAM_REVEAL
        rings["low"].append(cq.Vector(x + nx * out, y + ny * out, z - d - out))
        rings["tip0"].append(cq.Vector(x - nx * d, y - ny * d, z - 0.1))
        rings["tip1"].append(cq.Vector(x - nx * d, y - ny * d, z + 0.1))
        rings["high"].append(cq.Vector(x + nx * out, y + ny * out, z + d + out))
    params = [a / arc[-1] for a in arc[:-1]] + [1.0]
    low, tip0, tip1, high = (cq.Wire.assembleEdges([cq.Edge.makeSpline(rings[k], periodic=True, parameters=params)])
                             for k in ("low", "tip0", "tip1", "high"))
    return _sew([_loft([low, tip0, tip1, high], True), _loft([high, low], True)])


def seam_band(off0, off1, z0, z1):
    """A band following the outline, off0..off1 in from the outside, from z0 to z1 (local, from the
    seam), less TONGUE_GAP_ZONES."""
    wires = []
    for off in (off0, off1):
        pts, _, arc = outline(OUTLINE_STEP, OUTLINE_SAMPLES, off)
        params = [a / arc[-1] for a in arc[:-1]] + [1.0]
        vs = []
        for x, y in pts:
            vs.append(cq.Vector(x, y, to_world((y + SPLIT * S) / C, SPLIT)[1]))
        wires.append(cq.Wire.assembleEdges([cq.Edge.makeSpline(vs, periodic=True, parameters=params)]))
    n = cq.Vector(0, -S, C)
    face = cq.Face.makeFromWires(wires[0], [wires[1]]).translate(n * z0)
    band = cq.Solid.extrudeLinear(face, n * (z1 - z0))
    for x0, y0, y1 in TONGUE_GAP_ZONES:
        band = band.cut(box(x0, 300, y0, y1, -50, 100))
    return band.clean()


def _sew(shapes, tol=1e-4):
    """A solid from faces that close a volume."""
    sew = BRepBuilderAPI_Sewing(tol)
    for sh in shapes:
        for f in sh.Faces():
            sew.Add(f.wrapped)
    sew.Perform()
    fix = ShapeFix_Shell(TopoDS.Shell_s(sew.SewedShape()))
    fix.Perform()
    solid = BRepBuilderAPI_MakeSolid(fix.Shell()).Solid()
    BRepLib.OrientClosedSolid_s(solid)
    return cq.Solid(solid)


SCOOP_Y0 = KEY_GRID[2] - (KEY_GAP - KEYCAP_INSET) - FRONT_SCOOP_FLAT  # local y where the scoop starts
SCOOP_Y1 = -(BOARD_H / 2 + BEZEL_FB) - FRONT_SCOOP_DROP * S / C  # where the world-vertical front meets it


def scoop_z(yl):
    """Local z of the bezel's top at local y (flat, then the thumb scoop, carrying on straight past the front)."""
    t = max((SCOOP_Y0 - yl) / (SCOOP_Y0 - SCOOP_Y1), 0.0)
    f = 10 / 3 * t ** 3 * (1 - t) + t ** 5 if t <= 1.0 else 1.0 + 5 / 3 * (t - 1.0)  # straight on past the edge
    return BEZEL_TOP - FRONT_SCOOP_DROP * f


def scoop_profile(samples=24):
    """The thumb scoop's (local y, z) points, from its start to the outer front edge."""
    return [(y, scoop_z(y)) for y in (SCOOP_Y0 + (SCOOP_Y1 - SCOOP_Y0) * i / samples for i in range(samples + 1))]


TOP_SURFACE_HALF = 400.0  # the top surface runs local x and y +-this


def top_surface():
    """The bezel's top surface (world frame) as one face: flat, the thumb scoop, then straight on past the
    front. In the local frame its profile is a degree-5 B-spline: the scoop is exactly a Bezier with its
    heights at 0, 0, 0, 1/3, 2/3, 1 times the drop, so it meets the flat and the straight run past the edge
    with matching slope and curvature, and the knots there are reduced to C2, so the surface is one smooth
    patch."""
    big = TOP_SURFACE_HALF
    y0, y1, b, d = SCOOP_Y0, SCOOP_Y1, BEZEL_TOP, FRONT_SCOOP_DROP
    run = 20.0  # past the front edge
    fall = d * 5 / 3 / (y0 - y1)  # the scoop's slope at the edge
    pieces = [[(big + (y0 - big) * i / 5, b) for i in range(6)],
              [(y0 + (y1 - y0) * i / 5, b - d * f) for i, f in enumerate([0, 0, 0, 1 / 3, 2 / 3, 1])],
              [(y1 - run * i / 5, b - d - fall * run * i / 5) for i in range(6)]]
    poles = pieces[0] + pieces[1][1:] + pieces[2][1:]
    arr = TColgp_Array1OfPnt(1, len(poles))
    for i, (y, z) in enumerate(poles):
        arr.SetValue(i + 1, gp_Pnt(-big, y, z))
    knots, mults = TColStd_Array1OfReal(1, 4), TColStd_Array1OfInteger(1, 4)
    for i, (k, m) in enumerate([(-big, 6), (-y0, 5), (-y1, 5), (run - y1, 6)]):  # parameter u = -y
        knots.SetValue(i + 1, k)
        mults.SetValue(i + 1, m)
    curve = Geom_BSplineCurve(arr, knots, mults, 5)
    for i in (2, 3):
        if not curve.RemoveKnot(i, 3, 1e-9):
            raise RuntimeError("scoop profile is not C2")
    edge = BRepBuilderAPI_MakeEdge(curve).Edge()
    return place(cq.Shape.cast(BRepPrimAPI_MakePrism(edge, gp_Vec(2 * big, 0, 0)).Shape()))


OUTLINE_STEP, OUTLINE_SAMPLES = 8.0, 32  # outline points: spacing on the straight edges, and per corner


def outline(step=OUTLINE_STEP, samples=OUTLINE_SAMPLES, inset=0.0):
    """The case's plan outline (world XY), counter-clockwise from the right end's middle: (points,
    outward normals, arc lengths). The corners are the squircle's (continuous curvature). With inset, the
    outline inset by that much on the straight edges, its corners shrunk round the same centres (a little
    further in than inset, never closer, and never looping on itself as a true offset past the corners'
    tightest radius would)."""
    w, h = 2 * (OUT_X - inset), OUT_Y_BACK - OUT_Y_FRONT - 2 * inset
    yc = (OUT_Y_FRONT + OUT_Y_BACK) / 2
    L, n = CORNER_L - inset, CORNER_N

    def corner(sx, sy, t):
        cx, cy = sx * (w / 2 - L), yc + sy * (h / 2 - L)
        c, s = max(math.cos(t), 0.0), max(math.sin(t), 0.0)
        p = (cx + sx * L * c ** (2 / n), cy + sy * L * s ** (2 / n))
        # Outward normal of |u|^n + |v|^n = 1: (u^(n-1), v^(n-1)), normalised.
        u, v = c ** (2 / n), s ** (2 / n)
        nx, ny = sx * u ** (n - 1), sy * v ** (n - 1)
        k = math.hypot(nx, ny)
        return p, (nx / k, ny / k)

    def line(a, b, nrm):
        # Spacing grows from the corners' (about 0.4 mm) to step and back, so the lofts through these
        # points don't overshoot where the corners start.
        d = math.dist(a, b)
        s0 = L * math.pi / 2 / samples
        ends = []
        s = s0
        while sum(ends) + s < d / 2 and s < step:
            ends.append(s)
            s *= 1.5
        mid = d - 2 * sum(ends)
        m = max(1, round(mid / step))
        gaps = ends + [mid / m] * m + ends[::-1]
        out, t = [], 0.0
        for gp in gaps:
            out.append(((a[0] + (b[0] - a[0]) * t / d, a[1] + (b[1] - a[1]) * t / d), nrm))
            t += gp
        return out

    pts = []
    for i, (sx, sy) in enumerate([(1, 1), (-1, 1), (-1, -1), (1, -1)]):
        ts = [math.pi / 2 * k / samples for k in range(samples + 1)]
        if sx * sy < 0:
            ts = ts[::-1]
        arc = [corner(sx, sy, t) for t in ts]
        if i == 0:  # the right end, from its middle up to the first corner
            pts += line((OUT_X - inset, yc), arc[0][0], (1.0, 0.0))
        pts += arc[:-1]
        nxt = [(-1, 1), (-1, -1), (1, -1), (1, 1)][i]
        a = arc[-1][0]
        nrm = [(0.0, 1.0), (-1.0, 0.0), (0.0, -1.0), (1.0, 0.0)][i]
        if i < 3:
            nsx, nsy = nxt
            b = corner(nsx, nsy, 0.0 if nsx * nsy > 0 else math.pi / 2)[0]
        else:
            b = (OUT_X - inset, yc)
        pts += [arc[-1]] + line(a, b, nrm)[1:]
    s, d = [0.0], 0.0
    for (p, _), (q, _) in zip(pts, pts[1:] + pts[:1]):
        d += math.dist(p, q)
        s.append(d)
    return [p for p, _ in pts], [nr for _, nr in pts], s


def top_z(yw):
    """World height of the bezel's top surface (flat, then the thumb scoop) at world y."""
    zl = scoop_z
    lo, hi = yw - 50, yw + 50  # local y: world y = yl C - zl S is increasing in yl
    for _ in range(60):
        mid = (lo + hi) / 2
        if mid * C - zl(mid) * S < yw:
            lo = mid
        else:
            hi = mid
    return to_world(lo, zl(lo))[1]


def _loft(wires, ruled):
    """The lateral faces through closed wires (no end caps)."""
    t = BRepOffsetAPI_ThruSections(False, ruled, 1e-6)
    for w in wires:
        t.AddWire(w.wrapped)
    t.Build()
    return cq.Shape.cast(t.Shape())


def outer_shell():
    """The case's outside: vertical walls on the plan outline, the bezel's top (flat, then the thumb
    scoop) and between them a continuous-curvature roll, the corners' superellipse (CORNER_N) turned
    on its side, TOP_ROLL along both the wall and the top. Built as a loft through sections of the roll,
    so the wall, the roll and the top meet with no kink in curvature. The foot is chamfered."""
    pts, nrms, arc = outline(OUTLINE_STEP, OUTLINE_SAMPLES)
    n, L = CORNER_N, TOP_ROLL
    # Sections evenly spaced along the roll's curve (evenly spaced angles bunch up at its ends, and the
    # loft, which approximates across the sections, then bulges out of the wall).
    dense = [math.pi / 2 * k / 2000 for k in range(2001)]
    ab = [(math.cos(t) ** (2 / n), math.sin(t) ** (2 / n)) for t in dense]
    run = [0.0]
    for p, q in zip(ab, ab[1:]):
        run.append(run[-1] + math.dist(p, q))
    thetas = [dense[min(range(len(run)), key=lambda i: abs(run[i] - run[-1] * k / 10))] for k in range(11)]
    gap = L * run[-1] / 10
    rows = {k: [] for k in ("bottom", "foot", "wall")}
    sections = [[] for _ in range(len(thetas) + 1)]
    for (x, y), (nx, ny) in zip(pts, nrms):
        zc = top_z(y)
        h = 0.05
        slope = (top_z(y - ny * h) - zc) / h  # rise of the top going inward
        dt = cq.Vector(-nx, -ny, slope).normalized()
        dw = cq.Vector(0, 0, -1)
        c = cq.Vector(x, y, zc)
        o = c + dw * L + dt * L
        rows["bottom"].append(cq.Vector(x - nx * FOOT_CHAMFER, y - ny * FOOT_CHAMFER, 0))  # the foot's chamfer
        rows["foot"].append(cq.Vector(x, y, FOOT_CHAMFER))
        rows["wall"].append(c + dw * (L + 2 * gap))  # the roll's loft starts down the wall, so it leaves it straight
        sections[0].append(c + dw * (L + gap))
        for i, t in enumerate(thetas[:-1]):
            p = o - dt * (L * math.cos(t) ** (2 / n)) - dw * (L * math.sin(t) ** (2 / n))
            sections[i + 1].append(p)
        q = c + dt * L
        sections[-1].append(cq.Vector(q.x, q.y, top_z(q.y)))
    params = [a / arc[-1] for a in arc[:-1]]

    def ring(vs):
        return cq.Wire.assembleEdges([cq.Edge.makeSpline(vs, periodic=True, parameters=params + [1.0])])

    # The top: the bezel surface inside the roll's last ring. That ring is drawn on the surface itself (a
    # periodic spline in the surface's parameters, u = -local y, v = -400 - x), so the top's edge and
    # the roll's last section are one curve.
    surf = BRep_Tool.Surface_s(top_surface().wrapped)
    uv = TColgp_HArray1OfPnt2d(1, len(sections[-1]))
    for i, p in enumerate(sections[-1]):
        yl, _ = to_local(p.y, p.z)
        uv.SetValue(i + 1, gp_Pnt2d(-yl, -TOP_SURFACE_HALF - p.x))
    par = TColStd_HArray1OfReal(1, len(params) + 1)
    for i, a in enumerate(params + [1.0]):
        par.SetValue(i + 1, a)
    interp = Geom2dAPI_Interpolate(uv, par, True, 1e-9)
    interp.Perform()
    edge = BRepBuilderAPI_MakeEdge(interp.Curve(), surf).Edge()
    BRepLib.BuildCurves3d_s(edge, 1e-7)
    edge_wire = cq.Wire(BRepBuilderAPI_MakeWire(edge).Wire())
    face = BRepBuilderAPI_MakeFace(surf, edge_wire.wrapped, True).Face()
    fix = ShapeFix_Face(face)
    fix.Perform()
    top = [cq.Face(fix.Face())]
    bottom, foot, wall = (ring(rows[k]) for k in ("bottom", "foot", "wall"))
    rings = [wall] + [ring(v) for v in sections[:-1]] + [edge_wire]
    side, roll = _loft([bottom, foot, wall], True), _loft(rings, False)
    return _sew([side, roll] + top + [cq.Face.makeFromWires(bottom)])


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


def screw_points(xy=None):
    pts = []
    for x, y in SCREWS if xy is None else xy:
        yw, zw = to_world(y, SPLIT)
        pts.append((x, yw, zw))
    return pts


def dowel_holes(top):
    """The dowel pins' holes: pressed into the bottom case, a sliding fit in the top case, where the
    second hole is a slot along the line between the pins."""
    (xa, ya, za), (xb, yb, zb) = screw_points(DOWELS)
    h = DOWEL_L / 2 + 0.5
    if not top:
        return [cyl_z(x, y, DOWEL_PRESS_D, z - h, z + 1) for x, y, z in ((xa, ya, za), (xb, yb, zb))]
    angle = math.degrees(math.atan2(yb - ya, xb - xa))
    slot = (cq.Workplane("XY").workplane(offset=zb - 1).center(xb, yb)
            .slot2D(DOWEL_SLIP_D + DOWEL_SLOT, DOWEL_SLIP_D, angle).extrude(h + 1).val())
    return [cyl_z(xa, ya, DOWEL_SLIP_D, za - 1, za + h), slot]


def dowels():
    return cq.Compound.makeCompound([cyl_z(x, y, DOWEL_D, z - DOWEL_L / 2, z + DOWEL_L / 2)
                                     for x, y, z in screw_points(DOWELS)])


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
    # (The opening starts inside the board's pocket, so no thin sliver of wall is left between them.)
    for (w, h), y0 in (((USB_SHELL[0] + 0.9, USB_SHELL[1] + 0.8), min(y_face - 1.0, y_edge)),
                       ((12.4, 6.6), y_face)):
        plane = cq.Plane(origin=(USB_X, y0, zc), xDir=(1, 0, 0), normal=(0, 1, 0))
        slot = cq.Workplane(plane).slot2D(w, h).extrude(OUT_Y_BACK + 5 - y0).val()
        cuts.append(slot)
    for kx, ky in USB_HOLES_KICAD:  # tapped M3, 4.8 mm of thread (the floor below stays 1.2 mm)
        x, y = usb_world(kx, ky)
        cuts.append(cyl_z(x, y, TAP_DRILL, USB_Z - 4.8, USB_Z + 0.1))
    return cuts


def _cap_x():
    """Local x of the cap's flange (inner, outer), its grip (inner) and the end face."""
    x0 = INNER_W / 2 - 0.55
    return x0, x0 + CAP_FLANGE[0], OUT_X - CAP_GRIP[0], OUT_X


def switch_cap_cuts():
    f0, f1, g0, face = _cap_x()
    y, c, t, e = SW1_Y, CAP_CLEAR, CAP_TRAVEL, CAP_END_CLEAR  # the travel, centred on the switch
    def slab(x0, x1, w, h):  # cut from the end (along X), so its corners along X are rounded
        b = box(x0, x1, y - w / 2 - t / 2 - e, y + w / 2 + t / 2 + e, CAP_Z - h / 2 - c, CAP_Z + h / 2 + c)
        return b.fillet(CNC_MIN_R, [e for e in b.Edges() if abs(e.tangentAt(0).x) > 0.999])
    return [place(slab(f0 - 1.0, f1 + c, CAP_FLANGE[1], CAP_FLANGE[2])),
            place(slab(f1 - 0.1, g0 + 0.1, CAP_STEM[0], CAP_STEM[1])),
            place(slab(g0 - c, face + 1.0, CAP_GRIP[1], CAP_GRIP[2]))]


def switch_cap():
    """The power switch's cap, where the board model has the actuator: slider (flange and stem) and grip."""
    f0, f1, g0, face = _cap_x()
    y = CAP_Y
    def slab(x0, x1, w, h):  # corners along X rounded to clear the pockets' rounded corners
        b = box(x0, x1, y - w / 2, y + w / 2, CAP_Z - h / 2, CAP_Z + h / 2)
        return b.fillet(CNC_MIN_R - CAP_CLEAR, [e for e in b.Edges() if abs(e.tangentAt(0).x) > 0.999])
    slider = slab(f0, f1, CAP_FLANGE[1], CAP_FLANGE[2]).fuse(slab(f1 - 0.1, g0 + 1.0, CAP_STEM[0], CAP_STEM[1]))
    aw, ad = CAP_ACTUATOR
    slider = slider.cut(box(f0 - 1, f0 + ad, y - aw / 2, y + aw / 2, CAP_Z - 1.0, 20))
    grip = slab(g0, face, CAP_GRIP[1], CAP_GRIP[2])
    grip = grip.fillet(0.5, [e for e in grip.Edges() if e.Center().x > face - 0.01])
    for i in range(-3, 4):  # ridges across the slide
        grip = grip.cut(box(face - 0.5, face + 1, y + i * 1.8 - 0.4, y + i * 1.8 + 0.4, -20, 20))
    return place(slider.fuse(grip).clean())


def bottom_case(shell):
    b = shell.cut(half_space_local(SPLIT, True))
    b = b.cut(cavity())
    b = b.cut(seam_band(*GROOVE[:2], -GROOVE[2], 1.0))
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
    # Power switch: the cap's pocket, window and recess.
    for c in switch_cap_cuts():
        b = b.cut(c)
    # The roller's notch in the end wall, down to the PCB top.
    b = b.cut(place(roller_slot()))
    # Reset: a pin hole straight up to SW2.
    b = b.cut(place(cyl_z(*SW2_LOCAL, RESET_PIN_D, -60, -0.5)))
    # Screws to the top case.
    for x, y, z in screw_points():
        b = b.cut(cyl_z(x, y, SCREW_HOLE, -1, z + 1))
        b = b.cut(cyl_z(x, y, SCREW_CBORE, -1, z - SCREW_GRIP))
    for c in dowel_holes(False):
        b = b.cut(c)
    # Pockets for the feet.
    for x in FOOT_X:
        for y in FOOT_Y:
            b = b.cut(box(x - FOOT_POCKET_L / 2, x + FOOT_POCKET_L / 2, y - FOOT_POCKET_W / 2, y + FOOT_POCKET_W / 2,
                          -1, FOOT_POCKET_DEPTH))
    # The weight's pocket, and its screws' holes and counterbores in the floor over it.
    b = b.cut(weight_body(WEIGHT_CLEAR, -1.0))
    for x, y in WEIGHT_SCREWS:
        zf = floor_top(y)
        b = b.cut(cyl_z(x, y, SCREW_HOLE, zf - WEIGHT_FLOOR - 1, zf + 1))
        b = b.cut(cyl_z(x, y, WEIGHT_SCREW_CBORE[0], zf - WEIGHT_SCREW_CBORE[1], zf + 1))
    b, failed = cnc_round(b.clean(), (0, 0, 1))
    if failed:
        print("bottom case: inside corners left sharp at", failed)
    return b


def split_end(bottom):
    """The bottom case's aluminium part and its polycarbonate panel. The panel's floor laps over a
    PANEL_LAP ledge left in the aluminium's lower half on its three inner sides, so it can't drop out;
    the top case (the tongue runs over it) holds it down and stops it sliding."""
    x0, y0, y1 = CASE_PANEL
    lap, zl, xw = PANEL_LAP, FLOOR / 2, INNER_W / 2
    panel = round_edges_at(box(x0, OUT_X + 1, y0, y1, zl, 100), x0, 1.5)
    panel = panel.fuse(round_edges_at(box(x0 + lap, xw, y0 + lap, y1 - lap, -10, zl + 0.01), x0 + lap, 1.5),
                       box(xw - 0.01, OUT_X + 1, y0, y1, -10, zl + 0.01)).clean()
    inside = [e for e in panel.Edges() if e.geomType() == "LINE" and abs(e.tangentAt(0).z) > 0.999
              and abs(e.Center().x - xw) < 0.05 and min(abs(e.Center().y - y0), abs(e.Center().y - y1)) < 0.05]
    panel = panel.fillet(CNC_MIN_R, inside)  # the ledge's inside corners at the wall
    # fix(): the cut leaves the seam reveal's thin band with a bad edge where it's split.
    return bottom.cut(panel).clean().fix(), bottom.intersect(panel).clean().fix()


ROOF_Z = BEZEL_TOP - ROOF_T  # roof underside


def outer_prism(z0=0.0, z1=80.0):
    yc, h = (OUT_Y_FRONT + OUT_Y_BACK) / 2, OUT_Y_BACK - OUT_Y_FRONT
    return squircle(2 * OUT_X, h, z0, z1, y=yc)


def key_opening(z0, z1):
    """The opening round the keycaps (local frame): KEY_GAP outside them, with the bottom row's run
    under the sidebar's front, and rounded corners."""
    g = KEY_GAP - KEYCAP_INSET  # grid edge to the opening
    x0, x1, y0, y1 = KEY_GRID
    main = box(x0 - g, x1 + g, y0 - g, y1 + g, z0, z1)
    run = box(x1, KEY_GRID_ROW5_X1 + g, y0 - g, KEY_GRID_ROW5_Y + g, z0, z1)
    o = main.fuse(run).cut(box(ARROW_GAP_X[0] + g, ARROW_GAP_X[1] - g, y0 - 10, KEY_GRID_ROW5_Y - g, z0 - 1, z1 + 1))
    u = 19.05
    yf = y1 - u  # the F row's front edge
    o = o.cut(box(x0 - 10, x1 + 10, yf - F_ROW_GAP_U * u + g, yf - g, z0 - 1, z1 + 1))
    for a in F_ROW_POSTS_U:
        o = o.cut(box(x0 + a * u + g, x0 + (a + F_ROW_GAP_U) * u - g, yf - F_ROW_GAP_U * u + g, y1 + 10, z0 - 1, z1 + 1))
    o = o.clean()
    vertical = [e for e in o.Edges() if e.geomType() == "LINE" and abs(e.tangentAt(0).z) > 0.999]
    return o.fillet(OPENING_R, vertical)


def f_row_ribs():
    """The F-row bar and posts carried down under the roof as ribs, on the same plan footprint (KEY_GAP from
    the keycaps), to F_ROW_RIB_CLEAR above the plate (local frame)."""
    g = KEY_GAP - KEYCAP_INSET
    x0, x1, _, y1 = KEY_GRID
    u = 19.05
    yf = y1 - u
    z0, z1 = PLATE_Z0 + PLATE_T + F_ROW_RIB_CLEAR, ROOF_Z + 0.5
    ribs = box(x0 - g - 1, x1 + g + 1, yf - F_ROW_GAP_U * u + g, yf - g, z0, z1)
    for a in F_ROW_POSTS_U:
        ribs = ribs.fuse(box(x0 + a * u + g, x0 + (a + F_ROW_GAP_U) * u - g, yf - F_ROW_GAP_U * u + g, y1 + g + 1,
                             z0, z1))
    return ribs.clean()


def hump():
    """The hump over the encoder body: a cylinder round the roller's axis, from the bezel's roof up."""
    x0, x1 = HUMP_X
    c = cq.Solid.makeCylinder(HUMP_R, x1 - x0, cq.Vector(x0, ENC_AXIS_Y, ENC_AXIS_Z), cq.Vector(1, 0, 0))
    c = c.cut(box(x0 - 1, x1 + 1, -100, 100, -50, BEZEL_TOP - 0.5))
    edges = [e for e in c.Edges() if e.geomType() == "CIRCLE"]
    return place(c.fillet(HUMP_FILLET, edges))


def led_guide_xy():
    r = LED_HEAD_D / 2 + LED_GUIDE_WALL
    return LED_X[0] - r, LED_X[-1] + r, LED_GUIDE_Y[0], LED_GUIDE_Y[1]


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


def roof_inserts():
    """The frame that locates the screen, under the roof."""
    _, glass_bottom, _, pcb_bottom, _ = display_z()
    part = SCREEN["frame"]
    z0 = glass_bottom - 1.0 if part == "glass" else pcb_bottom + 0.5
    frame = box(*display_rect(part, DISPLAY_CLEAR + DISPLAY_FRAME_T), z0, ROOF_Z + 0.1)
    frame = frame.cut(dogbone_box(*display_rect(part, DISPLAY_CLEAR), 0, 40))
    cx = SCREEN[part + "_c"][0]
    frame = frame.cut(box(cx - DISPLAY_WIRE_GAP / 2, cx + DISPLAY_WIRE_GAP / 2, 0, 300, 0, 40))
    x0, x1, y0, y1, z1 = J4_BOX  # a notch over J4's header (or the wires soldered into its holes)
    frame = frame.cut(box(x0 - 0.5, x1 + 0.5, y0 - 0.5, y1 + 0.5, 0, z1 + 0.5))
    return place(frame.clean())


def roller_slot():
    """The roller's slot (local frame): round the wheel's lower half at ROLLER_SLOT_CLEAR, so the case
    follows its curve, and straight up from its axis, out through the end wall."""
    r = ROLLER_D / 2 + ROLLER_SLOT_CLEAR
    x0, x1 = HUMP_X[1], OUT_X + 1.0
    up = round_edges_at(box(x0, x1, ROLLER_Y - r, ROLLER_Y + r, ROLLER_Z, 40), x0, 1.5)
    return _wheel_axis(x0, x1 - x0, r).fuse(up).clean()


def roof_cuts():
    """Inside the hump (the encoder body and a tunnel for its hub), the roller's slot, and room for the
    screen where it reaches past the inside wall (world)."""
    x0, x1, y0, y1, _, z1 = ENC_BODY
    c = ENC_CLEAR
    pocket = cq.Workplane("YZ").workplane(offset=x0 - c).center((y0 + y1) / 2, (ROOF_Z - 1 + z1 + c) / 2) \
        .rect(y1 - y0 + 2 * c, z1 + c - ROOF_Z + 1).extrude(x1 - x0 + 2 * c).edges("|X or |Z").fillet(ENC_POCKET_R).val()
    tunnel = cq.Solid.makeCylinder(ENC_HUB_D / 2 + c, HUMP_X[1] - x1 + 1, cq.Vector(x1, ENC_AXIS_Y, ENC_AXIS_Z),
                                   cq.Vector(1, 0, 0))
    slot = roller_slot()
    back = display_z()[4]
    screen = [dogbone_box(*display_rect(p, DISPLAY_CLEAR), back - 0.3, ROOF_Z) for p in (SCREEN["frame"], "pcb")]
    # Where the glass reaches into the end wall, its pocket runs on down to the seam, so a cutter can reach it.
    gx0, gx1, gy0, gy1 = display_rect(SCREEN["frame"], DISPLAY_CLEAR)
    if gx1 > INNER_W / 2:
        screen.append(dogbone_box(INNER_W / 2 - 2.0, gx1, gy0, gy1, SPLIT - 1.0, ROOF_Z))
    return [place(c) for c in [pocket, tunnel, slot] + screen]


def roof_holes():
    """The screen window, the LEDs' holes and the score between D3 and D4 (world)."""
    cuts = []
    # Window: the active area plus a margin at the roof underside, opening 45 degrees to the top.
    cx, cy = SCREEN["glass_c"]
    cy -= SCREEN["aa_dy"]
    w, h = (a + 2 * DISPLAY_WINDOW_MARGIN for a in SCREEN["aa"])
    e = ROOF_T + 0.2
    lo = cq.Wire.makePolygon([cq.Vector(cx + dx * w / 2, cy + dy * h / 2, ROOF_Z - 0.1)
                              for dx, dy in ((-1, -1), (1, -1), (1, 1), (-1, 1), (-1, -1))])
    hi = cq.Wire.makePolygon([cq.Vector(cx + dx * (w / 2 + e), cy + dy * (h / 2 + e), BEZEL_TOP + 0.1)
                              for dx, dy in ((-1, -1), (1, -1), (1, 1), (-1, 1), (-1, -1))])
    cuts.append(cq.Solid.makeLoft([lo, hi], True))
    for x in LED_X:
        cuts.append(cyl_z(x, LED_Y, LED_HOLE_D, ROOF_Z - 1.0, BEZEL_TOP + 1))
    xs, w = (LED_X[0] + LED_X[1]) / 2, LED_SCORE_W / 2
    v = cq.Wire.makePolygon([cq.Vector(xs - w - 0.5, 0, BEZEL_TOP + 0.5), cq.Vector(xs + w + 0.5, 0, BEZEL_TOP + 0.5),
                             cq.Vector(xs, 0, BEZEL_TOP - w)], close=True)
    score = cq.Solid.extrudeLinear(cq.Face.makeFromWires(v), cq.Vector(0, LED_SCORE_L, 0))
    cuts.append(score.translate((0, LED_Y - LED_SCORE_L / 2, 0)))
    return [place(c) for c in cuts]


def led_seal_face():
    """The black foam strip under the light guide, with a hole over each LED (local XY, z = 0)."""
    x0, x1, y0, y1 = led_guide_xy()
    f = box(x0, x1, y0, y1, -1, 1)
    for x in LED_X:
        f = f.cut(cyl_z(x, LED_Y, LED_SEAL_HOLE_D, -2, 2))
    return min((fc for fc in f.Faces() if fc.normalAt().z < -0.99), key=lambda fc: fc.Center().z).translate((0, 0, 1))


def led_seal():
    return place(extrude_face(led_seal_face(), PLATE_Z0 + PLATE_T, LED_GUIDE_BOTTOM))


def led_guide():
    """The printed light guide: a block from the seal to the roof's underside, a channel up from each LED
    and a counterbore at its top for each plug's head. Its ends are rounded to clear the roof's inside
    corners (CNC_MIN_R)."""
    x0, x1, y0, y1 = led_guide_xy()
    g = box(x0, x1, y0, y1, LED_GUIDE_BOTTOM, ROOF_Z)
    g = g.fillet(CNC_MIN_R, [e for e in g.Edges() if abs(e.tangentAt(0).z) > 0.999])
    for x in LED_X:
        g = g.cut(cyl_z(x, LED_Y, LED_CHANNEL_D, LED_GUIDE_BOTTOM - 1, ROOF_Z + 1))
        g = g.cut(cyl_z(x, LED_Y, LED_HEAD_D + 2 * LED_PLUG_CLEAR, ROOF_Z - LED_HEAD_T, ROOF_Z + 1))
    return place(g.clean())


def led_plugs():
    """The clear plugs: a 3 mm shaft through the roof, flush with its top, on a 4 mm head under it."""
    plugs = []
    for x in LED_X:
        p = cyl_z(x, LED_Y, LED_HOLE_D - 2 * LED_PLUG_CLEAR, ROOF_Z, BEZEL_TOP)
        plugs.append(p.fuse(cyl_z(x, LED_Y, LED_HEAD_D, ROOF_Z - LED_HEAD_T, ROOF_Z)).clean())
    return place(cq.Compound.makeCompound(plugs))


def top_case(shell):
    t = shell.cut(half_space_local(SPLIT, False))
    t = t.cut(inner_prism(-50, ROOF_Z))
    # The key opening, with the chamfer on its top edge cut by a 45-degree flare (the top is flat all
    # round the opening: the thumb scoop starts FRONT_SCOOP_FLAT in front of it).
    t = t.cut(place(key_opening(ROOF_Z - 1, BEZEL_TOP + 1)))
    for f in key_opening(ROOF_Z - 1, BEZEL_TOP).Faces():  # the opening is in pieces (the F row's posts)
        if f.geomType() != "PLANE" or abs(f.Center().z - BEZEL_TOP) > 1e-6:
            continue
        rim = f.outerWire()
        flare = cq.Solid.makeLoft([rim.translate((0, 0, -INNER_CHAMFER)),
                                   rim.offset2D(INNER_CHAMFER + 0.3, "arc")[0].translate((0, 0, 0.3))], True)
        # As B-splines: OCCT drops the cut of its cones from the top surface without a word.
        t = t.cut(place(cq.Solid(BRepBuilderAPI_NurbsConvert(flare.wrapped, True).Shape())))
    t = t.fuse(place(f_row_ribs()))
    t = t.fuse(hump())
    for c in roof_cuts():
        t = t.cut(c)
    t = t.fuse(roof_inserts())
    for c in roof_holes():
        t = t.cut(c)
    for p in sock_pockets():
        t = t.cut(p)
    for x, y, z in screw_points():
        t = t.cut(cyl_z(x, y, TAP_DRILL, z - 1, z + TAP_DEPTH + 0.8))
    for c in dowel_holes(True):
        t = t.cut(c)
    t = t.fuse(seam_band(*TONGUE[:2], -TONGUE[2], 0.01))
    t, failed = cnc_round(t.clean(), (0, -S, C))
    if failed:
        print("top case: inside corners left sharp at", failed)
    return t


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


def _wheel_axis(x0, w, r):
    return cq.Solid.makeCylinder(r, w, cq.Vector(x0, ROLLER_Y, ROLLER_Z), cq.Vector(1, 0, 0))


def roller():
    """The wheel: a straight-knurled rim (ribs along the shaft, for rolling), chamfered ends, and a pocket
    in its right end for the push cap."""
    x0, x1 = ROLLER_X
    r = _wheel_axis(x0, x1 - x0, ROLLER_D / 2)
    r = r.chamfer(WHEEL_CHAMFER, None, [e for e in r.Edges() if e.geomType() == "CIRCLE"])
    n, d = WHEEL_RIBS, WHEEL_RIB_DEPTH
    ribs = []
    for i in range(n):
        a = 360.0 * (i + 0.5) / n
        g = box(x0 + WHEEL_CHAMFER + 0.6, x1 - WHEEL_CHAMFER - 0.6, -WHEEL_RIB_GAP / 2, WHEEL_RIB_GAP / 2,
                ROLLER_D / 2 - d, ROLLER_D / 2 + 1)
        ribs.append(g.rotate((0, 0, 0), (1, 0, 0), a).translate((0, ROLLER_Y, ROLLER_Z)))
    r = r.cut(cq.Compound.makeCompound(ribs))
    cap_r, depth = WHEEL_CAP_D / 2 + 0.1, WHEEL_CAP_T + WHEEL_CAP_SINK
    r = r.cut(_wheel_axis(x1 - depth, depth + 1, cap_r))
    return place(r.clean())


def wheel_cap():
    """The push cap in the wheel's end: a contrasting disc, slightly domed, standing WHEEL_CAP_SINK below the
    rim so it reads as a button."""
    x1 = ROLLER_X[1] - WHEEL_CAP_SINK
    disc = _wheel_axis(x1 - WHEEL_CAP_T, WHEEL_CAP_T, WHEEL_CAP_D / 2)
    rc = (WHEEL_CAP_D ** 2 / 4 + WHEEL_CAP_DOME ** 2) / (2 * WHEEL_CAP_DOME)  # dome's sphere radius
    dome = cq.Solid.makeSphere(rc, cq.Vector(x1 + WHEEL_CAP_DOME - rc, ROLLER_Y, ROLLER_Z), angleDegrees1=-90,
                               angleDegrees2=90)
    dome = dome.intersect(_wheel_axis(x1, WHEEL_CAP_DOME + 0.1, WHEEL_CAP_D / 2))
    return place(disc.fuse(dome).clean())


def floor_top(yw):
    """World z of the floor's top (its steps) at world y, away from the keep-outs."""
    z = FLOOR
    for y in ROW_EDGES:
        ye, ze = to_world(y, -UNDER_PCB_STEP)
        if ze >= FLOOR + 0.5 and yw >= ye:
            z = ze
    return z


def weight_y():
    return (OUT_Y_FRONT + OUT_Y_BACK) / 2


def weight_body(grow=0.0, z0=0.0):
    """The weight's plain shape (world), grown by grow (sides and top) for its pocket."""
    yc = weight_y()
    y0, y1 = yc - WEIGHT_H / 2 - grow, yc + WEIGHT_H / 2 + grow
    edges = sorted({y0, y1} | {to_world(y, -UNDER_PCB_STEP)[0] for y in ROW_EDGES
                               if y0 < to_world(y, -UNDER_PCB_STEP)[0] < y1})
    body = None
    for a, b in zip(edges, edges[1:]):
        blk = box(-WEIGHT_W / 2 - grow, WEIGHT_W / 2 + grow, a, b, z0, floor_top((a + b) / 2) - WEIGHT_FLOOR + grow)
        body = blk if body is None else body.fuse(blk)
    body = body.intersect(rrect(WEIGHT_W + 2 * grow, WEIGHT_H + 2 * grow, WEIGHT_R + grow, z0 - 1, 60, 0, yc)).clean()
    # Where a step meets the sides the pocket's inside corner is rounded (CNC_MIN_R), so the weight's is too.
    steps = [e for e in body.Edges() if e.geomType() == "LINE" and abs(e.tangentAt(0).z) > 0.999
             and abs(abs(e.Center().x) - WEIGHT_W / 2 - grow) < 1e-6 and y0 + 1e-6 < e.Center().y < y1 - 1e-6]
    return body.fillet(CNC_MIN_R + WEIGHT_CLEAR + 0.1 - grow, steps) if steps else body


def engraving():
    """The weight's engraving (world), ENGRAVE_DEPTH into its bottom face: the dachshund and the name."""
    yc = weight_y()
    xs = [x for stroke in DOG for x, _ in stroke]
    u0 = -(max(xs) + min(xs)) / 2 * DOG_SCALE
    cut = None
    for stroke in DOG:
        # As you look at the bottom (turned over front to back) up is towards the front: world y = yc - v.
        pts = [cq.Vector(u0 + x * DOG_SCALE, yc - (DOG_V + y * DOG_SCALE), -0.1) for x, y in stroke]
        outline = cq.Wire.assembleEdges([cq.Edge.makeSpline(pts)]).offset2D(ENGRAVE_W / 2, "arc")[0]
        groove = cq.Solid.extrudeLinear(cq.Face.makeFromWires(outline), cq.Vector(0, 0, ENGRAVE_DEPTH + 0.1))
        cut = groove if cut is None else cut.fuse(groove)
    plane = cq.Plane(origin=(0, yc - TEXT_V, 0), xDir=(1, 0, 0), normal=(0, 0, -1))
    text = (cq.Workplane(plane).text(ENGRAVE_TEXT, ENGRAVE_TEXT_SIZE, ENGRAVE_DEPTH + 0.1, fontPath=ENGRAVE_FONT,
                                     halign="center", valign="center").val().translate((0, 0, ENGRAVE_DEPTH)))
    return cut.fuse(text).clean()


def weight():
    w = weight_body()
    w = w.chamfer(WEIGHT_CHAMFER, None, [e for e in w.Edges() if abs(e.Center().z) < 1e-6])
    w = w.cut(engraving())
    for x, y in WEIGHT_SCREWS:
        top = floor_top(y) - WEIGHT_FLOOR
        w = w.cut(cyl_z(x, y, TAP_DRILL, top - 4.8, top + 1))
    return w


def feet():
    z1 = FOOT_POCKET_DEPTH
    return cq.Compound.makeCompound([rrect(FOOT_L, FOOT_W, 1.0, z1 - FOOT_T, z1, x, y) for x in FOOT_X for y in FOOT_Y])


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

def mesh_volume(shape):
    """Volume from a fine mesh (the exact integration is far off on the lofted roll's surface)."""
    vs, ts = shape.tessellate(0.05, 0.2)
    p = [v.toTuple() for v in vs]
    total = 0.0
    for i, j, k in ts:
        (ax, ay, az), (bx, by, bz), (cx, cy, cz) = p[i], p[j], p[k]
        total += ax * (by * cz - bz * cy) - ay * (bx * cz - bz * cx) + az * (bx * cy - by * cx)
    return total / 6


def report(parts):
    for name, shape in parts.items():
        bb = shape.BoundingBox()
        print(f"{name:12s} {bb.xlen:6.1f} x {bb.ylen:6.1f} x {bb.zlen:5.1f} mm, {mesh_volume(shape) / 1000:7.1f} cm3")
    print(f"front height (top of the bezel at the front edge, after the thumb scoop): "
          f"{to_world(*scoop_profile()[-1])[1]:.1f} mm")
    print(f"back height: {to_world(BOARD_H / 2 + BEZEL_FB, BEZEL_TOP)[1]:.1f} mm")
    print(f"plate top at the front row edge: {to_world(-BOARD_H / 2, PLATE_Z0 + PLATE_T)[1]:.1f} mm")
    hump_top = ENC_AXIS_Z + HUMP_R
    print(f"bezel: {BEZEL_ABOVE_PLATE:.1f} mm above the plate top; hump top {hump_top - BEZEL_TOP:.1f} mm above "
          f"the bezel ({to_world(ENC_AXIS_Y, hump_top)[1]:.1f} mm off the desk); roller {ROLLER_D:.1f} mm across, "
          f"{ROLLER_X[1] - ROLLER_X[0]:.2f} mm wide, its end {OUT_X - ROLLER_X[1]:.2f} mm inside the end face, top {ROLLER_Z + ROLLER_D / 2 - BEZEL_TOP:.1f} mm above the bezel")
    for y in ROW_EDGES:
        yw, zw = to_world(y, -UNDER_PCB_STEP)
        print(f"floor step at world y {yw:6.1f}: {max(zw, FLOOR):.1f} mm")


def write_step(shape, path, name, color):
    a = cq.Assembly(name="Dachshund60F")
    a.add(shape, name=name, color=cq.Color(*color))
    # No curves-on-surfaces (pcurves): on the lofted roll they'd make the file several times bigger, and
    # CAD programs rebuild them on import.
    a.save(path, "STEP", write_pcurves=False)
    print(f"{os.path.relpath(path, HW)}: {os.path.getsize(path) / 1e6:.1f} MB")


def build_parts():
    shell = outer_shell().cut(seam_groove())
    board, glass, screen = display()
    bottom, panel = split_end(bottom_case(shell))
    return {
        "Case top": top_case(shell),
        "Case bottom": bottom,
        "Case panel": panel,
        "Switch cap": switch_cap(),
        "Plate foam": plate_foam(),
        "Gasket": gaskets(),
        "Battery": battery(),
        "Roller": roller(),
        "Wheel cap": wheel_cap(),
        "Feet": feet(),
        "Weight": weight(),
        "Dowels": dowels(),
        "LED seal": led_seal(),
        "LED guide": led_guide(),
        "LED plugs": led_plugs(),
        "Display board": board,
        "Display glass": glass,
        "Display area": screen,
    }


FILES = {
    "Case top": "Dachshund60F-Case-Top.step",
    "Case bottom": "Dachshund60F-Case-Bottom.step",
    "Case panel": "Dachshund60F-Case-Panel.step",
    "Switch cap": "Dachshund60F-Switch-Cap.step",
    "Gasket": "Dachshund60F-Gaskets.step",
    "LED guide": "Dachshund60F-LED-Guide.step",
    "LED plugs": "Dachshund60F-LED-Plugs.step",
    "Weight": "Dachshund60F-Weight.step",
}


EXPLODE = {"Case top": 70, "Display board": 70, "Display glass": 70, "Display area": 70, "LED seal": 50, "LED guide": 62, "LED plugs": 78,
           "Board": 38, "Plate foam": 38, "Gasket": 38, "Roller": 38, "Wheel cap": 38, "Feet": -40, "Weight": -55,
           "Case panel": -20, "Switch cap": -20}


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
            assembly.write_meshes(assembly.build(parts, COLORS, skip=("Board", "Case top", "Roller", "Wheel cap",
                                                                      "Plate foam", "Gasket", "Display board",
                                                                      "Display glass", "Display area",
                                                                      "LED seal", "LED guide", "LED plugs")),
                                 meshes[2])
            render.render(*meshes, os.path.join(HERE, "renders"))


if __name__ == "__main__":
    main()
