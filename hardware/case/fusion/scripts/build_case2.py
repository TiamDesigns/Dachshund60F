import adsk.core, adsk.fusion, math, json

# ---------------- parameters (mm; local = board frame, tilted TILT deg about X in the world) ----------------
BAR_TEXT_FLIP = True
PRM = dict(
    OUT_X=176.3,          # half width of the outline (ends)
    RUN_FRONT=10.8, RUN_END=8.3, RUN_BACK=10.8,   # run of each slope in the plate's plane at the top plane (front and back match,
                                                   # the ends match); the gaskets lie along the plate edge, so no extra room at the back
    POWER_SLOT=(27.4, 5.2, 2.4),   # access slot over the power slide switch's lever in the back face: x centre, width, height
    DAUGHTER_X=(-21.02, 32.98),    # the daughterboard (54 x 9 mm) from the USB-C centre: x range
    DAUGHTER_TAPS=(-16.5, 19.0),   # its two M3 holes (H1 and H2)
    R_OUT=10.0, R_TOP=3.0,
    SLOPE_Z=4.1,                      # local z where every slope meets its wall (the wall is square to the plate)
    W_FLAT=6.8,                       # the flat strip beside the openings: one width all round (runs and angles follow)
    RING_BOT=-2.2, RING_TOP=-0.6,     # accent ring: parallel to the plate, 3 mm under the PCB's bottom face; laser-cut from flat
                                      # 1.6 sheet, so every wall of it is square to the sheet (grooves only besides)
    UNDERCUT_IN=0.0, UNDERCUT_H=0.5,    # (no swept undercut: the bottom edge gets a real fillet, BOTTOM_FILLET)
    BOTTOM_FILLET=0.0,                # removed for now (added later by hand)
    SCOOP_A=20.0,                     # radius of the scoop cutter's cross-section (a surface of revolution about a y-parallel axis)
    BACK_Y=65.6,          # world y of the back screws (each in a boss with a 1.5 mm wall all round)
    BACK_GAP=0.0,        # no bottom tongue/groove this far either side of each back screw
    SOLID_Z=8.2,          # top block is solid above this local z (the sock tops are at 8.1, plate top 6.6)
    ROOF=True, UNDERSIDE=True,
    MOD_X0=125.655,        # local x of the encoder module's left edge (u = 0)
    SHAFT_TIP_U=31.5,     # Bourns PEC12R-2220F: 20 mm shaft from the body face (u 11.5), so the tip is at u 31.5
    WHEEL_U=(18.0, 31.5), # the wheel on the shaft: from just past the body to the tip, covering the hub
    DISP_CX=144.5,        # local x of the e-ink glass centre
    FLOOR_L=-4.5,         # cavity floor plane under the PCB (local z): 0.9 under the lowest switch/stabilizer legs
    SCOOP_T=(0.12, 0.71, 0.86), SCOOP_PEAK=0.55, SCOOP_D=4.0, SCOOP_VIS=0.93, SCOOP_FILLET=1.5,
    COVER_WALL=2.0, COVER_GAP=0.25,   # encoder cover: wall; gap from it to the top case's cutout
    AXIS_Y=50.03,         # encoder axis, local y: on the F row's centre line (59.55 - 0.5 * 19.05)
    KNOB_D=19.2,          # knob: the cover's side walls run tangent to it (the housing is as wide as the knob), 0.4 above the module PCB
)

TILT = 4.0
S, C = math.sin(math.radians(TILT)), math.cos(math.radians(TILT))
LIFT = 3.0 + 4.0 * C + (121.1 / 2 + 0.75) * S      # floor 3.0, 4.0 from it to the PCB bottom at the front edge
INNER_W, INNER_H, INNER_R = 320.0, 122.6, 5.75
TOP_Z = 13.2
ROOF_Z = TOP_Z - 1.5
G = 1.0 - 0.525
U = 19.05
X0, X1, Y0, Y1 = -158.25, 127.5, -59.513, 59.55
ROW5_Y, ROW5_X1 = -40.46, 146.55
ARROW = (X0 + 12.5 * U, X0 + 13 * U)
F_GAP = 0.25
F_POSTS = (1.0, 5.25, 9.5, 13.75)
OPEN_R = 1.0
FLOOR, UNDER = 3.0, 3.0
TOP_TONGUE, TOP_GROOVE = (1.75, 2.95), (1.6, 3.1)
BOT_TONGUE, BOT_GROOVE = (4.15, 5.35), (4.0, 5.5)
TONGUE_H, GROOVE_D = 0.6, 0.8
BOARD_W, BOARD_H = 318.5, 121.1
SOCK_W, SOCK_H, SQUEEZE, CLEAR = 5.0, 5.0, 0.4, 0.15
SOCK_Z0 = 5.8 - (SOCK_H - SQUEEZE) / 2
SOCK_ZH = SOCK_H - SQUEEZE
# branched nubs (plate/gen_plate.py): neck 4, slot 2, arm 1.5 x 12, the prongs continue the arms along the plate edge
NECK_W, SLOT_W, ARM_W, ARM_LEN, TAB_W, TAB_L = 4.0, 2.0, 1.5, 12.0, 2.8, 6.5
SOCK_LONG = 8.0
ARM_OUTER = SLOT_W + ARM_W
ARM_MID = SLOT_W + ARM_W / 2                 # the arm's and the sock's centre line, past the plate edge
PRONG_START = NECK_W / 2 + ARM_LEN           # the arm's end, from the nub centre: the sock's open end
BACK_NUBS = (-107.7, -15.0, 77.7)
FRONT_NUBS = (-106.17, 0.0, 106.17)
SOCKS = [(f + sg * PRONG_START, f + sg * (PRONG_START + SOCK_LONG), 1) for f in BACK_NUBS for sg in (-1, 1)] + \
        [(f + sg * PRONG_START, f + sg * (PRONG_START + SOCK_LONG), -1) for f in FRONT_NUBS for sg in (-1, 1)]
NUBS = [(f, 1) for f in BACK_NUBS] + [(f, -1) for f in FRONT_NUBS]
SCREW_X = (-151.5, -50.5, 50.5, 151.5)
FRONT_Y = -66.81
SCREW_HOLE, SCREW_CBORE, SCREW_GRIP = 3.4, 5.8, 3.5    # grip: vertical, seat to ring top
TAP_DRILL, TAP_DEPTH = 2.5, 5.0                         # vertical, from the ring top up
FOOT = (300.0, 5.8, 2.0)               # two long rubber rails, near the front and back edges
FOOT_POCKET = (300.6, 6.4, 1.0)
FOOT_XY = [(0.0, -58.0), (0.0, 53.0)]
USB_Z, USB_SHELL = 7.0, (8.94, 3.26)       # the daughterboard sits low: the ring runs at 13.7 where it meets the back face
PCB_T = 1.6
WARN = []


def mm(v):
    return v / 10.0


def P(x, y, z):
    return adsk.core.Point3D.create(mm(x), mm(y), mm(z))


def l2w(x, y, zl):
    return (x, y * C - zl * S, y * S + zl * C + LIFT)


def unit(v):
    n = math.hypot(*v)
    return (v[0] / n, v[1] / n)


def rrect(sk, to_sk, cx, cy, hx, hy, r):
    rpoly(sk, to_sk, [(cx + hx, cy - hy), (cx + hx, cy + hy), (cx - hx, cy + hy), (cx - hx, cy - hy)], r)


def rpoly(sk, to_sk, pts, r):
    if sum(a[0] * b[1] - b[0] * a[1] for a, b in zip(pts, pts[1:] + pts[:1])) < 0:
        pts = pts[::-1]
    n = len(pts)
    cs = []
    for i in range(n):
        p0, p1, p2 = pts[i - 1], pts[i], pts[(i + 1) % n]
        d1 = unit((p1[0] - p0[0], p1[1] - p0[1]))
        d2 = unit((p2[0] - p1[0], p2[1] - p1[1]))
        a = (p1[0] - d1[0] * r, p1[1] - d1[1] * r)
        b = (p1[0] + d2[0] * r, p1[1] + d2[1] * r)
        c = (a[0] + d2[0] * r, a[1] + d2[1] * r)
        v = unit((p1[0] - c[0], p1[1] - c[1]))
        cs.append((a, (c[0] + v[0] * r, c[1] + v[1] * r), b))
    lines, arcs = sk.sketchCurves.sketchLines, sk.sketchCurves.sketchArcs
    # Fusion always makes an arc counter-clockwise, so on a concave corner (a clockwise sweep) its "start" is the
    # end I mean: take each arc's two sketch points by where they are, never by start/end.
    ends = []
    for a, m, b in cs:
        pa, pb = to_sk(*a), to_sk(*b)
        arc = arcs.addByThreePoints(pa, to_sk(*m), pb)
        e0, e1 = arc.startSketchPoint, arc.endSketchPoint
        if e0.geometry.distanceTo(pa) <= e1.geometry.distanceTo(pa):
            ends.append((e0, e1))
        else:
            ends.append((e1, e0))
    for i in range(n):
        lines.addByTwoPoints(ends[i][1], ends[(i + 1) % n][0])


def rr(hx, hy):
    return [(hx, -hy), (hx, hy), (-hx, hy), (-hx, -hy)]


def nrm(f):
    ok, n = f.evaluator.getNormalAtPoint(f.pointOnFace)
    return n


def bbox(b):
    bb = b.boundingBox
    return [round(v * 10, 3) for v in (bb.minPoint.x, bb.minPoint.y, bb.minPoint.z, bb.maxPoint.x, bb.maxPoint.y, bb.maxPoint.z)]


def opening_polys(sk, to_sk):
    """The key opening, every edge on the 19.05 grid in the plate's frame: the main block (with the bottom row's
    run under the sidebar and the arrow gap) and the five F-row blocks."""
    yA, yTop = Y0 - G, Y1 + G
    yf = Y1 - U
    yBarLo, yBarHi = yf - F_GAP * U + G, yf - G
    main = [(X0 - G, yA), (ARROW[0] + G, yA), (ARROW[0] + G, ROW5_Y - G), (ARROW[1] - G, ROW5_Y - G), (ARROW[1] - G, yA),
            (ROW5_X1 + G, yA), (ROW5_X1 + G, ROW5_Y + G), (X1 + G, ROW5_Y + G), (X1 + G, yBarLo), (X0 - G, yBarLo)]
    rpoly(sk, to_sk, main, OPEN_R)
    xs = [X0 - G]
    for a in F_POSTS:
        xs += [X0 + a * U + G, X0 + (a + F_GAP) * U - G]
    xs.append(X1 + G)
    for i in range(0, len(xs), 2):
        rpoly(sk, to_sk, [(xs[i], yBarHi), (xs[i + 1], yBarHi), (xs[i + 1], yTop), (xs[i], yTop)], OPEN_R)


def build(prm):
    app = adsk.core.Application.get()
    design = adsk.fusion.Design.cast(app.activeProduct)
    root = design.rootComponent
    p = dict(PRM)
    p.update(prm)
    # The flat top: one strip of width W_FLAT beyond the outermost opening on every side. Left and front are the key
    # opening; the back is the encoder cover cutout (it reaches further back than the F row) and the right end is
    # that cutout's end too (the sidebar's screen and LEDs sit inside the flat).
    W = p['W_FLAT']
    MX0_, AXY_ = p['MOD_X0'], 49.55
    hw_cut = 9.5 + p['COVER_GAP'] + p['COVER_WALL'] + p['COVER_GAP']
    feat_left, feat_front = X0 - G, Y0 - G
    feat_back = Y1 + G                  # the encoder cutout (top at AXY_ + hw_cut) sits inside the flat
    OUTER_HALF = (23.71 + 1.6) / 2 + (TOP_Z - (ROOF_Z - 0.1))     # half width of the screen frame at the top face (the chamfer's outer edge): 14.255
    feat_right = p['DISP_CX'] + OUTER_HALF + p['COVER_GAP']       # the encoder cutout ends 0.25 past the knob, which ends at the frame's outer edge
    fx0, fx1 = feat_left - W, feat_right + W
    y_lo0, y_hi0 = feat_front - W, feat_back + W
    # the outline follows the runs: each edge sits its run beyond the flat
    OUT_FRONT = p['RUN_FRONT'] - y_lo0
    OUT_BACK = y_hi0 + p['RUN_BACK']
    OUT_XL = p['RUN_END'] - fx0
    OUT_XR = fx1 + p['RUN_END']
    OUT_CX = (OUT_XR - OUT_XL) / 2
    OUT_X = (OUT_XR + OUT_XL) / 2            # half length from here on

    R_OUT = p['R_OUT']
    RING_BOT, RING_TOP = p['RING_BOT'], p['RING_TOP']
    # the front and back walls are square to the plate (they lean with the board): the outline is drawn in the plate's plane
    cy_l = (OUT_BACK - OUT_FRONT) / 2
    hy_l = (OUT_BACK + OUT_FRONT) / 2

    def wall_y(yl, zw):
        """World y of the wall that stands at local y yl, at world height zw."""
        zl = (zw - yl * S - LIFT) / C
        return yl * C - zl * S

    def zw_at(yl, zl):
        return yl * S + zl * C + LIFT

    Y_FRONT = wall_y(-OUT_FRONT, 0.0)             # at the flat bottom
    Y_BOT_BACK = wall_y(OUT_BACK, 0.0)
    USB_ZC = USB_Z + PCB_T + USB_SHELL[1] / 2
    Y_BACK = wall_y(OUT_BACK, USB_ZC)             # the back face at the USB-C's height

    tl = design.timeline
    for i in range(tl.count - 1, -1, -1):
        it = tl.item(i)
        if it.isGroup:
            ok = adsk.fusion.TimelineGroup.cast(it).deleteMe(True)
        else:
            ok = it.entity.deleteMe() if it.entity is not None else it.deleteMe()
        if not ok:
            print('could not delete', i, it.name)
    for o in list(root.occurrences):      # anything left over from imports
        o.deleteMe()

    planes = root.constructionPlanes
    ext = root.features.extrudeFeatures
    comb = root.features.combineFeatures
    Cut, Join, Inter, NewB = (adsk.fusion.FeatureOperations.CutFeatureOperation, adsk.fusion.FeatureOperations.JoinFeatureOperation,
                              adsk.fusion.FeatureOperations.IntersectFeatureOperation, adsk.fusion.FeatureOperations.NewBodyFeatureOperation)
    body = lambda n: [b for b in root.bRepBodies if b.name == n][0]

    inp = planes.createInput()
    inp.setByAngle(root.xConstructionAxis, adsk.core.ValueInput.createByReal(math.radians(TILT)), root.xYConstructionPlane)
    tilt0 = planes.add(inp)
    tilt0.name = 'Tilt 7deg through origin'
    tilt0.isLightBulbOn = False

    def tp(zl, name):
        i = planes.createInput()
        i.setByOffset(tilt0, adsk.core.ValueInput.createByReal(mm(zl + LIFT * C)))
        pl = planes.add(i)
        pl.name = name
        pl.isLightBulbOn = False
        return pl

    def zp(z, name):
        i = planes.createInput()
        i.setByOffset(root.xYConstructionPlane, adsk.core.ValueInput.createByReal(mm(z)))
        pl = planes.add(i)
        pl.name = name
        pl.isLightBulbOn = False
        return pl

    def sketch(pl, zl, name):
        sk = root.sketches.add(pl)
        sk.name = name
        sk.isLightBulbOn = False
        return sk, (lambda x, y: sk.modelToSketchSpace(P(*l2w(x, y, zl))))

    def profs_of(sk, only=None):
        c = adsk.core.ObjectCollection.create()
        for pr in sk.profiles:
            if only is None or only(pr):
                c.add(pr)
        return c

    def extrude_all(sk, dist, taper=None, only=None):
        ei = ext.createInput(profs_of(sk, only), NewB)
        if taper is None:
            ei.setDistanceExtent(False, adsk.core.ValueInput.createByReal(mm(dist)))
        else:
            ei.setOneSideExtent(adsk.fusion.DistanceExtentDefinition.create(adsk.core.ValueInput.createByReal(mm(dist))),
                                adsk.fusion.ExtentDirections.PositiveExtentDirection,
                                adsk.core.ValueInput.createByReal(math.radians(taper)))
        return list(ext.add(ei).bodies)

    def cut(targets, tools, op=Cut):
        for n, t in enumerate(targets):
            ci = comb.createInput(body(t), adsk.core.ObjectCollection.createWithArray(tools))
            ci.operation = op
            ci.isKeepToolBodies = (n < len(targets) - 1)
            comb.add(ci)

    def cyl_x(x0, length, dia, name, ay, az):
        i = planes.createInput()
        i.setByOffset(root.yZConstructionPlane, adsk.core.ValueInput.createByReal(mm(x0)))
        pl = planes.add(i)
        pl.name = name + ' plane'
        pl.isLightBulbOn = False
        sk = root.sketches.add(pl)
        sk.name = name
        sk.isLightBulbOn = False
        sk.sketchCurves.sketchCircles.addByCenterRadius(sk.modelToSketchSpace(P(x0, ay, az)), mm(dia / 2))
        e = ext.createInput(sk.profiles.item(0), NewB)
        e.setDistanceExtent(False, adsk.core.ValueInput.createByReal(mm(length)))
        b = ext.add(e).bodies.item(0)
        b.name = name
        return b

    # ---- temporary-body helpers (cables, screws): cylinder / box chains, added to the design through a base feature
    tbm = adsk.fusion.TemporaryBRepManager.get()
    UNION = adsk.fusion.BooleanTypes.UnionBooleanType
    DIFF = adsk.fusion.BooleanTypes.DifferenceBooleanType

    def P3(x, y, z):
        return adsk.core.Point3D.create(mm(x), mm(y), mm(z))

    v_sub = lambda a, b: tuple(x - y for x, y in zip(a, b))
    v_add = lambda a, b: tuple(x + y for x, y in zip(a, b))
    v_mul = lambda a, k: tuple(x * k for x in a)
    v_len = lambda a: math.sqrt(sum(x * x for x in a))
    v_unit = lambda a: v_mul(a, 1.0 / v_len(a))
    v_dot = lambda a, b: sum(x * y for x, y in zip(a, b))

    def add_temp(tbody, name):
        bf = root.features.baseFeatures.add()
        bf.startEdit()
        b_ = root.bRepBodies.add(tbody, bf)
        b_.name = name
        bf.finishEdit()
        return root.bRepBodies.itemByName(name)      # the body object from add() goes stale once the edit is finished

    def filleted(pts, bend_r, n=8):
        """A 3D polyline with every interior corner replaced by a circular arc of radius bend_r (n chords)."""
        out = [pts[0]]
        for i in range(1, len(pts) - 1):
            a = v_unit(v_sub(pts[i], pts[i - 1]))
            b = v_unit(v_sub(pts[i + 1], pts[i]))
            cs = max(-1.0, min(1.0, v_dot(a, b)))
            th = math.acos(cs)
            if th < 1e-3:
                out.append(pts[i])
                continue
            t = bend_r * math.tan(th / 2)
            t = min(t, 0.45 * v_len(v_sub(pts[i], pts[i - 1])), 0.45 * v_len(v_sub(pts[i + 1], pts[i])))
            r = t / math.tan(th / 2)
            nn = v_unit(v_sub(b, v_mul(a, cs)))
            p0 = v_sub(pts[i], v_mul(a, t))
            cen = v_add(p0, v_mul(nn, r))
            for k in range(n + 1):
                phi = th * k / n
                out.append(v_add(cen, v_add(v_mul(nn, -r * math.cos(phi)), v_mul(a, r * math.sin(phi)))))
        out.append(pts[-1])
        return out

    def round_chain(pts, rad, bend_r, name):
        """A round cable of radius rad along a polyline with bends of radius bend_r (swept as short cylinders)."""
        poly = filleted(pts, bend_r)
        body_ = None
        for a_, b_ in zip(poly[:-1], poly[1:]):
            parts = [tbm.createSphere(P3(*a_), mm(rad))]
            if v_len(v_sub(b_, a_)) > 1e-3:
                parts.append(tbm.createCylinderOrCone(P3(*a_), mm(rad), P3(*b_), mm(rad)))
            for q in parts:
                if body_ is None:
                    body_ = q
                else:
                    tbm.booleanOperation(body_, q, UNION)
        tbm.booleanOperation(body_, tbm.createSphere(P3(*poly[-1]), mm(rad)), UNION)
        length = sum(v_len(v_sub(b_, a_)) for a_, b_ in zip(poly[:-1], poly[1:]))
        return add_temp(body_, name), length

    def ribbon_chain(pts, width, thick, bend_r, name):
        """A flat ribbon (width along x) along a polyline in a y-z plane, bends of radius bend_r."""
        poly = filleted(pts, bend_r)
        body_ = None
        xhat = (1.0, 0.0, 0.0)
        for a_, b_ in zip(poly[:-1], poly[1:]):
            ln = v_len(v_sub(b_, a_))
            if ln < 1e-3:
                continue
            d = v_unit(v_sub(b_, a_))
            ctr = v_mul(v_add(a_, b_), 0.5)
            ob = adsk.core.OrientedBoundingBox3D.create(P3(*ctr), adsk.core.Vector3D.create(*d), adsk.core.Vector3D.create(*xhat),
                                                        mm(ln + 0.15), mm(width), mm(thick))
            q = tbm.createBox(ob)
            if body_ is None:
                body_ = q
            else:
                tbm.booleanOperation(body_, q, UNION)
        length = sum(v_len(v_sub(b_, a_)) for a_, b_ in zip(poly[:-1], poly[1:]))
        return add_temp(body_, name), length

    pl_bot = tp(RING_BOT, 'L ring bottom')
    pl_top = tp(RING_TOP, 'L ring top')
    pl_tz = tp(TOP_Z, 'L z13.2 bezel top')

    # ---- outline envelope: the outline extruded square to the plate (so the front and back walls lean with it and the ring's
    # walls are square to its sheet), cut off flat at the desk
    sk, to_env = sketch(tp(-30.0, 'L envelope start'), -30.0, 'Outline (plate plane)')
    rrect(sk, to_env, OUT_CX, cy_l, OUT_X, hy_l, R_OUT)
    extrude_all(sk, 80.0)[0].name = 'Envelope'
    skd = root.sketches.add(root.xYConstructionPlane)
    skd.name = 'Desk slab (world plan)'
    skd.isLightBulbOn = False
    skd.sketchCurves.sketchLines.addTwoPointRectangle(skd.modelToSketchSpace(P(-300, -200, 0)), skd.modelToSketchSpace(P(300, 200, 0)))
    ed = ext.createInput(skd.profiles.item(0), NewB)
    ed.setDistanceExtent(False, adsk.core.ValueInput.createByReal(mm(70.0)))
    desk = ext.add(ed).bodies.item(0)
    desk.name = 'Desk slab'
    ci = comb.createInput(body('Envelope'), adsk.core.ObjectCollection.createWithArray([desk]))
    ci.operation = Inter
    ci.isKeepToolBodies = False
    comb.add(ci)
    env = body('Envelope')

    # ---- slope tool: every slope ends BAND_H above the ring (square to it), on the outline wall itself.
    # A ruled loft from the flat-top footprint (set by the plan runs) down to the exact section of the outline's
    # wall by the plane BAND_H above the ring top: each side's angle follows from its run.
    BAND_H = p['SLOPE_Z'] - p['RING_TOP']
    zlow = RING_TOP + BAND_H
    DEPTH = TOP_Z - zlow
    r0 = min(W + OPEN_R, (y_hi0 - y_lo0) / 2 - 1, (fx1 - fx0) / 2 - 1)
    run = {'left': fx0 + OUT_XL, 'right': OUT_XR - fx1, 'front': y_lo0 + OUT_FRONT, 'back': OUT_BACK - y_hi0}
    eff = dict(run)                                   # the walls are square to the plate: the runs are plate-plane distances
    ang = {k: math.degrees(math.atan2(DEPTH, v)) for k, v in eff.items()}
    print('flat top x %.2f..%.2f y %.2f..%.2f (W %.1f); runs at the top plane: %s; angles to the top plane: %s' % (
        fx0, fx1, y_lo0, y_hi0, W, {k: round(v, 2) for k, v in run.items()}, {k: round(v, 1) for k, v in ang.items()}))
    print('heights (world z, mm): ring %.2f..%.2f at the front wall, %.2f..%.2f at the back wall; slopes start at %.2f (front) %.2f (back); flat top %.2f (front edge) %.2f (back edge)' % (
        zw_at(-OUT_FRONT, RING_BOT), zw_at(-OUT_FRONT, RING_TOP), zw_at(OUT_BACK, RING_BOT), zw_at(OUT_BACK, RING_TOP),
        zw_at(-OUT_FRONT, zlow), zw_at(OUT_BACK, zlow), zw_at(y_lo0, TOP_Z), zw_at(y_hi0, TOP_Z)))
    print('walls: front y %.2f, back y %.2f at the desk; wall lean %.1f deg' % (Y_FRONT, Y_BOT_BACK, TILT))
    pl_low = tp(zlow, 'L slope floor (ring top + band)')
    sk1, t1 = sketch(pl_tz, TOP_Z, 'Top face footprint (local)')
    rrect(sk1, t1, (fx0 + fx1) / 2, (y_lo0 + y_hi0) / 2, (fx1 - fx0) / 2, (y_hi0 - y_lo0) / 2, r0)
    sk2, t2 = sketch(pl_low, zlow, 'Slope bottom edge = outline at the wall top (plate plane)')
    rrect(sk2, t2, OUT_CX, cy_l, OUT_X, hy_l, R_OUT)
    lofts = root.features.loftFeatures
    li = lofts.createInput(NewB)
    li.loftSections.add(sk1.profiles.item(0))
    li.loftSections.add(sk2.profiles.item(0))
    li.isSolid = True
    tool = lofts.add(li).bodies.item(0)
    tool.name = 'Slope tool'
    sk3, t3 = sketch(pl_low, zlow, 'Slope tool slab (local)')
    rrect(sk3, t3, 0, 0, 300.0, 160.0, 5.0)
    ei = ext.createInput(sk3.profiles.item(0), NewB)
    ei.setOneSideExtent(adsk.fusion.DistanceExtentDefinition.create(adsk.core.ValueInput.createByReal(mm(60.0))),
                        adsk.fusion.ExtentDirections.NegativeExtentDirection)
    slab = ext.add(ei).bodies.item(0)
    slab.name = 'Slope tool slab'
    cut(['Slope tool'], [slab], Join)
    ci = comb.createInput(body('Envelope'), adsk.core.ObjectCollection.createWithArray([body('Slope tool')]))
    ci.operation = Inter
    ci.isKeepToolBodies = False
    comb.add(ci)

    # ---- base undercut
    if p['UNDERCUT_IN'] > 0:
        sk4 = root.sketches.add(root.yZConstructionPlane)
        sk4.name = 'Undercut profile (x=0)'
        sk4.isLightBulbOn = False
        to4 = lambda y, z: sk4.modelToSketchSpace(P(0, y, z))
        pts = adsk.core.ObjectCollection.create()
        UI, UH = p['UNDERCUT_IN'], p['UNDERCUT_H']
        for i in range(25):
            z = UH * i / 24
            pts.add(to4(Y_FRONT + UI * (1 - z / UH) ** 2, z))
        spl = sk4.sketchCurves.sketchFittedSplines.add(pts)
        ln = sk4.sketchCurves.sketchLines
        l1 = ln.addByTwoPoints(spl.endSketchPoint, to4(Y_FRONT - 3.0, UH))
        l2 = ln.addByTwoPoints(l1.endSketchPoint, to4(Y_FRONT - 3.0, -1.0))
        l3 = ln.addByTwoPoints(l2.endSketchPoint, to4(Y_FRONT + UI, -1.0))
        ln.addByTwoPoints(l3.endSketchPoint, spl.startSketchPoint)
        path = root.features.createPath(outline_sk.sketchCurves.sketchLines.item(0), True)
        sweeps = root.features.sweepFeatures
        sweeps.add(sweeps.createInput(sk4.profiles.item(0), path, Cut))

    # ---- split into bottom block / ring / top block
    splits = root.features.splitBodyFeatures
    env = body('Envelope')
    splits.add(splits.createInput(env, pl_top, True))
    czf = lambda b: b.physicalProperties.centerOfMass.z
    bodies = sorted(root.bRepBodies, key=czf)
    splits.add(splits.createInput(bodies[0], pl_bot, True))
    bodies = sorted(root.bRepBodies, key=czf)
    for b, nm in zip(bodies, ['Bottom shell', 'Accent ring', 'Top shell']):
        b.name = nm

    # ---- tongue and groove: the ring (cut from flat sheet) has grooves only, the blocks have the tongues
    gap_bodies = []
    if p['BACK_GAP'] > 0:
        skg = root.sketches.add(zp(-20.0, 'back gap plane'))
        skg.name = 'Back screw gaps (world plan)'
        skg.isLightBulbOn = False
        for sx in SCREW_X:
            skg.sketchCurves.sketchLines.addTwoPointRectangle(skg.modelToSketchSpace(P(sx - p['BACK_GAP'], 40.0, -20.0)),
                                                              skg.modelToSketchSpace(P(sx + p['BACK_GAP'], 110.0, -20.0)))
        gap_bodies = extrude_all(skg, 80.0)
        for gi, gb in enumerate(gap_bodies):
            gb.name = 'Back gap %d' % gi

    def joint(tag, bounds, z0, z1, target, op, gap=False, last=False):
        sk, to_sk = sketch(tp(z0, tag + ' band plane'), z0, tag + ' band (plate plane)')
        for o in bounds[:2]:
            rrect(sk, to_sk, OUT_CX, cy_l, OUT_X - o, hy_l - o, R_OUT - o)
        band = [pr for pr in sk.profiles if pr.profileLoops.count == 2][0]
        ei = ext.createInput(band, NewB)
        ei.setDistanceExtent(False, adsk.core.ValueInput.createByReal(mm(z1 - z0)))
        bb = ext.add(ei).bodies.item(0)
        bb.name = tag + ' band'
        if gap and gap_bodies:
            cg = comb.createInput(body(tag + ' band'), adsk.core.ObjectCollection.createWithArray(gap_bodies))
            cg.operation = Cut
            cg.isKeepToolBodies = not last
            comb.add(cg)
        pieces = [b for b in root.bRepBodies if b.name.startswith(tag + ' band')]
        c2 = comb.createInput(body(target), adsk.core.ObjectCollection.createWithArray(pieces))
        c2.operation = op
        c2.isKeepToolBodies = False
        comb.add(c2)

    # ---- ring joint: through slots in the laser-cut ring (square to the sheet) and a tab from each block, meeting inside the slot.
    # Slot 12 x 1.5, tabs 11.8 x 1.3 (0.1 clearance all round), each 0.75 tall so the two tabs leave 0.1 between them in the 1.6 sheet.
    SLOT_L, SLOT_W2, TAB_H = 12.0, 1.5, 0.75
    off = (TOP_GROOVE[0] + TOP_GROOVE[1]) / 2          # slot centre line from the outline wall
    yb_l, yf_l = OUT_BACK - off, -OUT_FRONT + off
    xr_l, xl_l = OUT_XR - off, -OUT_XL + off
    SLOTS = [(x_, yb_l, 0) for x_ in (-100.0, 0.0, 100.0)] + [(x_, yf_l, 0) for x_ in (-100.0, 0.0, 100.0)] +             [(xr_l, y_, 1) for y_ in (-35.0, 0.0, 35.0)] + [(xl_l, y_, 1) for y_ in (-35.0, 0.0, 35.0)]

    def slot_rects(sk, to_sk, grow):
        for sx_, sy_, rot in SLOTS:
            hx, hy = ((SLOT_L, SLOT_W2) if rot == 0 else (SLOT_W2, SLOT_L))
            hx, hy = hx / 2 + grow, hy / 2 + grow
            sk.sketchCurves.sketchLines.addTwoPointRectangle(to_sk(sx_ - hx, sy_ - hy), to_sk(sx_ + hx, sy_ + hy))

    zs0 = RING_BOT - 0.5
    sk, to_sk = sketch(tp(zs0, 'L ring slots start'), zs0, 'Ring joint slots (through, square to the sheet)')
    slot_rects(sk, to_sk, 0.0)
    cut(['Accent ring'], extrude_all(sk, RING_TOP - RING_BOT + 1.0))
    sk, to_sk = sketch(tp(RING_TOP - TAB_H, 'L top tabs start'), RING_TOP - TAB_H, 'Top block tabs (plate plane)')
    slot_rects(sk, to_sk, -0.1)
    cut(['Top shell'], extrude_all(sk, TAB_H), Join)
    sk, to_sk = sketch(tp(RING_BOT, 'L bottom tabs start'), RING_BOT, 'Bottom block tabs (plate plane)')
    slot_rects(sk, to_sk, -0.1)
    cut(['Bottom shell'], extrude_all(sk, TAB_H), Join)

    # ---- top block: lower pocket for the plate and PCB, solid above it
    SOLID_Z = p['SOLID_Z']
    zs = RING_TOP - 0.6
    sk, to_sk = sketch(tp(zs, 'L top pocket start'), zs, 'Top pocket (local)')
    rpoly(sk, to_sk, rr(INNER_W / 2, INNER_H / 2), INNER_R)
    cut(['Top shell'], extrude_all(sk, SOLID_Z - zs))
    # key opening, straight and square to the plate, right down to the pocket
    sk, to_sk = sketch(tp(6.5, 'L z6.5 opening start'), 6.5, 'Key opening (local)')
    opening_polys(sk, to_sk)
    cut(['Top shell'], extrude_all(sk, 14.2 - 6.5))
    # 0.3 mm flare on its top edge: the same profile, tapered 45 degrees outward, from 0.3 under the top
    sk, to_sk = sketch(tp(TOP_Z - 0.3, 'L z12.9 flare start'), TOP_Z - 0.3, 'Key opening flare (local)')
    opening_polys(sk, to_sk)
    cut(['Top shell'], extrude_all(sk, 0.6, taper=45.0))

    # ---- bottom block: the cavity is only what the PCB's underside needs. Its floor is a plane parallel to the PCB,
    # FLOOR_L under it, but never lower than the world floor (FLOOR) at the front. Everything else is solid.
    FLOOR_L = p['FLOOR_L']
    plane_u = tp(FLOOR_L, 'L cavity floor plane')
    sk, to_sk = sketch(plane_u, FLOOR_L, 'Bottom cavity (local)')
    rpoly(sk, to_sk, rr(INNER_W / 2, INNER_H / 2), INNER_R)
    cav = extrude_all(sk, 40.0)[0]
    cav.name = 'Cavity tool'
    skc = root.sketches.add(zp(FLOOR, 'floor clip plane'))
    skc.name = 'Floor clip (world z >= floor)'
    skc.isLightBulbOn = False
    skc.sketchCurves.sketchLines.addTwoPointRectangle(skc.modelToSketchSpace(P(-300, -200, FLOOR)), skc.modelToSketchSpace(P(300, 200, FLOOR)))
    e = ext.createInput(skc.profiles.item(0), NewB)
    e.setDistanceExtent(False, adsk.core.ValueInput.createByReal(mm(80.0)))
    clip = ext.add(e).bodies.item(0)
    clip.name = 'Floor clip tool'
    ci = comb.createInput(cav, adsk.core.ObjectCollection.createWithArray([clip]))
    ci.operation = Inter
    ci.isKeepToolBodies = False
    comb.add(ci)
    cut(['Bottom shell'], [body('Cavity tool')])

    # ---- stepped floor, one step per switch row (as the old generator's floor_steps): each row's floor is flat (world
    # horizontal) at the height of the floor plane at the row's front edge, so it falls away from the plane toward the row's
    # back, and steps up at the next row. The gap under the PCB stays between FLOOR_L and FLOOR_L - 2.3 mm, never less.
    ROW_EDGES = [-40.45, -21.40, -2.36, 16.69, 38.12, INNER_H / 2]
    ZST = FLOOR_L - 4.0                       # this prism reaches below the floor plane, where the steps are cut
    sk, to_sk = sketch(tp(ZST, 'L step cavity plane'), ZST, 'Step cavity (local)')
    rpoly(sk, to_sk, rr(INNER_W / 2, INNER_H / 2), INNER_R)
    cav2 = extrude_all(sk, 44.0)[0]
    cav2.name = 'Step cavity tool'
    step_bodies = []
    for ea, eb in zip(ROW_EDGES[:-1], ROW_EDGES[1:]):
        zh = ea * S + FLOOR_L * C + LIFT
        if zh < FLOOR + 1.0:
            continue
        ya, yb = ea * C - FLOOR_L * S, eb * C - FLOOR_L * S
        skw = root.sketches.add(zp(zh, 'step plane %.1f' % ea))
        skw.name = 'Floor step %.1f' % ea
        skw.isLightBulbOn = False
        skw.sketchCurves.sketchLines.addTwoPointRectangle(skw.modelToSketchSpace(P(-170, ya, zh)), skw.modelToSketchSpace(P(170, yb, zh)))
        e = ext.createInput(skw.profiles.item(0), NewB)
        e.setDistanceExtent(False, adsk.core.ValueInput.createByReal(mm(12.0)))
        step_bodies.append(ext.add(e).bodies.item(0))
    for sb_ in step_bodies[1:]:
        cj = comb.createInput(step_bodies[0], adsk.core.ObjectCollection.createWithArray([sb_]))
        cj.operation = Join
        cj.isKeepToolBodies = False
        comb.add(cj)
    step_bodies[0].name = 'Floor steps tool'
    ci = comb.createInput(step_bodies[0], adsk.core.ObjectCollection.createWithArray([body('Step cavity tool')]))
    ci.operation = Inter
    ci.isKeepToolBodies = False
    comb.add(ci)
    cut(['Bottom shell'], [body('Floor steps tool')])

    def lpocket(x0_, x1_, y0_, y1_, z0_, z1_, name):
        """A pocket in the floor, in the plate's frame: rectangle x/y, from local z0 up to z1."""
        skp, top_ = sketch(tp(z0_, name + ' plane'), z0_, name)
        skp.sketchCurves.sketchLines.addTwoPointRectangle(top_(x0_, y0_), top_(x1_, y1_))
        cut(['Bottom shell'], extrude_all(skp, z1_ - z0_))

    # under the screen and knob (x >= 127.5, past the last key column) the floor is one flat hollow at the base level, no steps
    sk, to_sk = sketch(tp(ZST, 'L hollow outline start'), ZST, 'Sidebar hollow outline (local)')
    rpoly(sk, to_sk, rr(INNER_W / 2, INNER_H / 2), INNER_R)
    hol = extrude_all(sk, 44.0)[0]
    hol.name = 'Sidebar hollow tool'
    skh = root.sketches.add(zp(FLOOR, 'hollow box plane'))
    skh.name = 'Sidebar hollow box (world plan)'
    skh.isLightBulbOn = False
    skh.sketchCurves.sketchLines.addTwoPointRectangle(skh.modelToSketchSpace(P(127.5, -100, FLOOR)), skh.modelToSketchSpace(P(170, 100, FLOOR)))
    eh = ext.createInput(skh.profiles.item(0), NewB)
    eh.setDistanceExtent(False, adsk.core.ValueInput.createByReal(mm(20.0)))
    hbx = ext.add(eh).bodies.item(0)
    hbx.name = 'Sidebar hollow box'
    ci = comb.createInput(body('Sidebar hollow tool'), adsk.core.ObjectCollection.createWithArray([hbx]))
    ci.operation = Inter
    ci.isKeepToolBodies = False
    comb.add(ci)
    cut(['Bottom shell'], [body('Sidebar hollow tool')])
    # bosses round the back screws, so each counterbore keeps 1.5 mm of wall to the cavity
    BACK_Y_ = p['BACK_Y']
    zb0, zb1 = FLOOR_L - 3.5, RING_BOT      # from below the stepped floor up to the ring
    ymid = (BACK_Y_ + 0.5 * (zb0 + zb1) * S) / C
    skb_, tob_ = sketch(tp(zb0, 'L back boss start'), zb0, 'Back screw bosses (local)')
    for sx in SCREW_X:
        skb_.sketchCurves.sketchCircles.addByCenterRadius(tob_(sx, ymid), mm(4.7))
    cut(['Bottom shell'], extrude_all(skb_, zb1 - zb0), Join)
    # the battery (EEMB 803450, 52 x 34.5 x 8.3, a 4.5 mm protection board on its front end) lies flat, long side front to back, under the
    # screen and the knob in one flat pocket (world horizontal: the board's underside slopes away toward the front, so a flat cell keeps
    # its 1.0 mm of aluminium all over). Cell x 124.75..159.25, y 5.3..61.8, bottom z 1.2 (0.2 off the floor), top z 9.5.
    BATT_X0, BATT_X1, BATT_Y0, BATT_Y1, BATT_Z0 = 124.75, 159.25, 5.3, 61.8, 1.2
    BATT_ZT = BATT_Z0 + 8.3
    skb3 = root.sketches.add(zp(BATT_Z0 - 0.2, 'battery bay plane'))
    skb3.name = 'Battery bay (world plan)'
    skb3.isLightBulbOn = False
    skb3.sketchCurves.sketchLines.addTwoPointRectangle(skb3.modelToSketchSpace(P(BATT_X0 - 0.3, BATT_Y0 - 0.3, BATT_Z0 - 0.2)),
                                                       skb3.modelToSketchSpace(P(BATT_X1 + 0.3, BATT_Y1 + 0.3, BATT_Z0 - 0.2)))
    cut(['Bottom shell'], extrude_all(skb3, BATT_ZT + 0.2 - (BATT_Z0 - 0.2)))
    print('battery bay: x %.2f..%.2f y %.2f..%.2f, floor z %.1f, cell z %.1f..%.1f, aluminium under it %.1f mm' % (
        BATT_X0 - 0.3, BATT_X1 + 0.3, BATT_Y0 - 0.3, BATT_Y1 + 0.3, BATT_Z0 - 0.2, BATT_Z0, BATT_ZT, BATT_Z0 - 0.2))

    # ---- ring frame: board outline + 0.5 (the ring is level with the PCB)
    zs = RING_BOT - 1.0
    sk, to_sk = sketch(tp(zs, 'L ring opening start'), zs, 'Ring opening: board outline + 0.5 (local)')
    rrect(sk, to_sk, 0, 0, INNER_W / 2, INNER_H / 2, INNER_R)
    ei = ext.createInput(sk.profiles.item(0), NewB)
    ei.setDistanceExtent(False, adsk.core.ValueInput.createByReal(mm(RING_TOP - zs + 1.0)))
    cut(['Accent ring'], [ext.add(ei).bodies.item(0)])

    # ---- sock pockets: twelve, top block only. Each sock lies along the plate edge from its arm's end (the open end), centred on the arm
    sk, to_sk = sketch(tp(SOCK_Z0, 'L sock pocket start'), SOCK_Z0, 'Sock pockets (local)')
    for xa, xb, s in SOCKS:
        xl, xh = sorted((xa, xb))
        if xa < xb:
            xl -= 0.7                  # the open end reaches back over the prong's 0.5 overlap with its arm
            xh += CLEAR
        else:
            xl -= CLEAR
            xh += 0.7
        ya, yb_ = sorted((s * (BOARD_H / 2 + ARM_MID - SOCK_W / 2 - CLEAR), s * (BOARD_H / 2 + ARM_MID + SOCK_W / 2 + CLEAR)))
        sk.sketchCurves.sketchLines.addTwoPointRectangle(to_sk(xl, ya), to_sk(xh, yb_))
    cut(['Top shell'], extrude_all(sk, SOCK_ZH))
    # clearance round each nub's neck and arms (0.5 mm all round the arms, the plate is 1.6 thick: 3.0 tall) so they flex
    NUB_Z0, NUB_Z1 = 5.8 - 1.5, 5.8 + 1.5
    sk, to_sk = sketch(tp(NUB_Z0, 'L nub clearance start'), NUB_Z0, 'Nub clearance (local)')
    for nx, s in NUBS:
        ya, yb = sorted((s * (BOARD_H / 2 - 0.4), s * (BOARD_H / 2 + ARM_OUTER + 0.9)))
        sk.sketchCurves.sketchLines.addTwoPointRectangle(to_sk(nx - PRONG_START - 0.5, ya), to_sk(nx + PRONG_START + 0.5, yb))
    cut(['Top shell'], extrude_all(sk, NUB_Z1 - NUB_Z0))

    # ---- screws: M3 countersunk (DIN 7991), heads flush with the bottom face: a 90 degree countersink 6.0 wide at the face,
    # a vertical 3.4 clearance hole up through the bottom block and the ring, and a tap in the top block. The back
    # (north) screws are long because the bottom block is 23 mm tall there.
    BACK_Y = p['BACK_Y']
    ring_top_z = lambda y: (RING_TOP + LIFT * C + S * y) / C
    rows = [(FRONT_Y, 'front'), (BACK_Y, 'back')]
    SCREW_LEN = {'front': 12, 'back': 20}
    TAP_DEEP = {lab: SCREW_LEN[lab] - ring_top_z(yrow) + 0.5 for yrow, lab in rows}      # the thread ends 0.5 short of the tap
    print('screws: front M3x12 (%.1f mm in the top block), back M3x20 (%.1f mm)' % (SCREW_LEN['front'] - ring_top_z(FRONT_Y), SCREW_LEN['back'] - ring_top_z(BACK_Y)))
    csk = None
    for yrow, lab in rows:
        for x in SCREW_X:
            q = tbm.createCylinderOrCone(P3(x, yrow, -0.5), mm(3.5), P3(x, yrow, 1.3), mm(1.7))
            if csk is None:
                csk = q
            else:
                tbm.booleanOperation(csk, q, UNION)
    cut(['Bottom shell'], [add_temp(csk, 'Countersink tool')])

    def row_circles(name, d, yrow, plane_ext, targets, xs=SCREW_X):
        sk = root.sketches.add(root.xYConstructionPlane)
        sk.name = name
        sk.isLightBulbOn = False
        for x in xs:
            sk.sketchCurves.sketchCircles.addByCenterRadius(sk.modelToSketchSpace(P(x, yrow, 0)), mm(d / 2))
        ei = ext.createInput(profs_of(sk), NewB)
        ei.setOneSideExtent(adsk.fusion.ToEntityExtentDefinition.create(plane_ext, False), adsk.fusion.ExtentDirections.PositiveExtentDirection)
        cut(targets, list(ext.add(ei).bodies))

    for yrow, lab in rows:
        row_circles('Screw clearance 3.4 (%s)' % lab, SCREW_HOLE, yrow, pl_bot, ['Bottom shell'])
    # the ring is laser-cut: its holes are square to the sheet (not vertical), on the screw axes at mid thickness
    zmid = (RING_BOT + RING_TOP) / 2
    skr, tor = sketch(tp(RING_BOT - 0.5, 'L ring holes start'), RING_BOT - 0.5, 'Ring screw holes (square to the sheet)')
    for yrow, lab in rows:
        yl_ = (yrow + zmid * S) / C
        for x in SCREW_X:
            skr.sketchCurves.sketchCircles.addByCenterRadius(tor(x, yl_), mm(SCREW_HOLE / 2))
    cut(['Accent ring'], extrude_all(skr, RING_TOP - RING_BOT + 1.0))
    for yrow, lab in rows:
        row_circles('Screw taps 2.5 (%s)' % lab, TAP_DRILL, yrow, zp(ring_top_z(yrow) + TAP_DEEP[lab], 'Tap end plane %s' % lab), ['Top shell'])

    # ---- roof features: encoder cutout for the metal cover, bosses, LED row, e-ink window and pockets
    if p['ROOF']:
        MX0 = p['MOD_X0']
        AX_Y, AX_Z = p['AXIS_Y'], 13.4
        MOD_W, MOD_H, MOD_T, MZ0 = 22.5, 22.0, 1.6, 1.8
        ay, az = l2w(0, AX_Y, AX_Z)[1:]
        KR, tol, cwall = p['KNOB_D'] / 2, p['COVER_GAP'], p['COVER_WALL']
        wu0, wu1 = p['WHEEL_U']
        wxa, wxb = MX0 + wu0, p['DISP_CX'] + OUTER_HALF      # the knob, from just past the encoder body to the frame's outer edge (past the shaft's tip)
        shaft_tip = MX0 + p['SHAFT_TIP_U']
        R_END = 1.5                                          # outer corner radius of the cover's end and the knob's end (top view)
        wl_win = p['DISP_CX'] - OUTER_HALF                   # the frame's outer left edge: the cover's closed end
        cov_x0 = wl_win                                      # the cover's closed end (the encoder body starts at u 11.5)
        cov_x1 = wxa - 0.15                                  # its open end: the knob follows 0.15 mm on
        HOUSING = KR                                         # half width of the cover's housing below the dome: its walls run tangent to the knob
        cut_x0, cut_x1, hwc, hhc = cov_x0 - tol, wxb + tol, KR + tol, HOUSING + tol
        sk, to_sk = sketch(tp(6.5, 'L z6.5 encoder cutout start'), 6.5, 'Encoder cover cutout (local)')
        rpoly(sk, to_sk, [(cut_x0, AX_Y - hhc), (cut_x1, AX_Y - hhc), (cut_x1, AX_Y + hhc), (cut_x0, AX_Y + hhc)], R_END + tol)   # corners follow the parts' own, with the same gap
        cut(['Top shell'], extrude_all(sk, 14.2 - 6.5))

        def yz_plane(x_, name):
            i_ = planes.createInput()
            i_.setByOffset(root.yZConstructionPlane, adsk.core.ValueInput.createByReal(mm(x_)))
            pl_ = planes.add(i_)
            pl_.name = name
            pl_.isLightBulbOn = False
            return pl_

        # the encoder cover, a separate part: a dome of the knob's radius (same axis) over a straight lower part, hollow
        # for the encoder body (12.5 wide, 13.4 tall, +0.3 all round), closed at the left end and open toward the knob;
        # its bottom follows the cutout floor (parallel to the PCB)
        Lc = cov_x1 - cov_x0
        pcv = yz_plane(cov_x0, 'Cover plane')
        s1 = root.sketches.add(pcv)
        s1.name = 'Cover dome'
        s1.isLightBulbOn = False
        s1.sketchCurves.sketchCircles.addByCenterRadius(s1.modelToSketchSpace(P(cov_x0, ay, az)), mm(KR))
        e = ext.createInput(s1.profiles.item(0), NewB)
        e.setDistanceExtent(False, adsk.core.ValueInput.createByReal(mm(Lc)))
        dome = ext.add(e).bodies.item(0)
        dome.name = 'Encoder cover'
        sl2, sl2t = sketch(tp(6.6, 'L cover lower start'), 6.6, 'Cover lower part (local)')
        sl2.sketchCurves.sketchLines.addTwoPointRectangle(sl2t(cov_x0, AX_Y - HOUSING), sl2t(cov_x1, AX_Y + HOUSING))
        e = ext.createInput(sl2.profiles.item(0), NewB)
        e.setDistanceExtent(False, adsk.core.ValueInput.createByReal(mm(AX_Z - 6.6)))
        lower = ext.add(e).bodies.item(0)
        cj = comb.createInput(body('Encoder cover'), adsk.core.ObjectCollection.createWithArray([lower]))
        cj.operation = Join
        cj.isKeepToolBodies = False
        comb.add(cj)
        sl, slt = sketch(tp(6.6, 'L cover bottom plane'), 6.6, 'Cover bottom slab (local)')
        rrect(sl, slt, 0, 0, 300.0, 160.0, 5.0)
        e = ext.createInput(sl.profiles.item(0), NewB)
        e.setDistanceExtent(False, adsk.core.ValueInput.createByReal(mm(60.0)))
        slab_c = ext.add(e).bodies.item(0)
        slab_c.name = 'Cover bottom slab'
        ci = comb.createInput(body('Encoder cover'), adsk.core.ObjectCollection.createWithArray([slab_c]))
        ci.operation = Inter
        ci.isKeepToolBodies = False
        comb.add(ci)
        pin = yz_plane(cov_x0 + cwall, 'Cover cavity plane')
        s3 = root.sketches.add(pin)
        s3.name = 'Cover cavity'
        s3.isLightBulbOn = False
        rrect(s3, lambda yl_, zl_: s3.modelToSketchSpace(P(cov_x0 + cwall, *l2w(0, yl_, zl_)[1:])), AX_Y, (AX_Z + 7.6 - 4.0) / 2, 7.0, (AX_Z + 7.6 + 4.0) / 2, 3.0)
        e = ext.createInput(s3.profiles.item(0), NewB)
        e.setDistanceExtent(False, adsk.core.ValueInput.createByReal(mm(Lc - cwall + 1.0)))
        cav_c = ext.add(e).bodies.item(0)
        cav_c.name = 'Cover cavity tool'
        cut(['Encoder cover'], [cav_c])
        # room for the encoder frame's two ears (x 137.0..139.3, 16.6 wide, up to the axis height) at the PCB end
        sear, sear_t = sketch(tp(6.0, 'L cover ear start'), 6.0, 'Cover ear pockets (local)')
        sear.sketchCurves.sketchLines.addTwoPointRectangle(sear_t(MX0 + 10.45, AX_Y - 8.7), sear_t(MX0 + 13.6, AX_Y + 8.7))
        cut(['Encoder cover'], extrude_all(sear, AX_Z + 0.4 - 6.0))
        cvr = body('Encoder cover')
        es_ = adsk.core.ObjectCollection.create()
        for f_ in cvr.faces:
            bb_ = f_.boundingBox
            if f_.geometry.surfaceType == adsk.core.SurfaceTypes.PlaneSurfaceType and abs(bb_.minPoint.x * 10 - cov_x0) < 0.01 and abs(bb_.maxPoint.x * 10 - cov_x0) < 0.01:
                for e_ in f_.edges:
                    es_.add(e_)
        if es_.count:
            fi_ = root.features.filletFeatures
            fin_ = fi_.createInput()
            fin_.addConstantRadiusEdgeSet(es_, adsk.core.ValueInput.createByReal(mm(R_END)), True)
            fi_.add(fin_)

        # the knob (hardware): the F-row key width in y (KNOB_D), on the shaft from just past the body to the tip, with a
        # diamond / cross-hatch knurl: 64 flutes along the axis and 14 rings round it
        wheel = cyl_x(wxa, wxb - wxa, p['KNOB_D'], 'HW Wheel', ay, az)
        wheel = body('HW Wheel')
        es_ = adsk.core.ObjectCollection.create()
        for e_ in wheel.edges:
            bb_ = e_.boundingBox
            if abs(bb_.minPoint.x * 10 - wxb) < 0.01 and abs(bb_.maxPoint.x * 10 - wxb) < 0.01:
                es_.add(e_)
        fi_ = root.features.filletFeatures
        fin_ = fi_.createInput()
        fin_.addConstantRadiusEdgeSet(es_, adsk.core.ValueInput.createByReal(mm(R_END)), True)
        fi_.add(fin_)
        hub_b = cyl_x(wxa - 0.2, (MX0 + 22.7) - (wxa - 0.2), 7.6, 'Knob hub bore tool', ay, az)
        shf_b = cyl_x(MX0 + 22.5, (shaft_tip + 0.3) - (MX0 + 22.5), 6.3, 'Knob shaft bore tool', ay, az)      # blind: ends 0.3 past the shaft's tip
        cut(['HW Wheel'], [hub_b, shf_b])
        pfl = yz_plane(wxa - 1.0, 'Knurl plane')
        sf = root.sketches.add(pfl)
        sf.name = 'Knob flutes'
        sf.isLightBulbOn = False
        NF = 64
        for k_ in range(NF):
            a_ = 2 * math.pi * k_ / NF
            sf.sketchCurves.sketchCircles.addByCenterRadius(sf.modelToSketchSpace(P(wxa - 1.0, ay + KR * math.cos(a_), az + KR * math.sin(a_))), mm(0.33))
        e = ext.createInput(profs_of(sf), NewB)
        e.setDistanceExtent(False, adsk.core.ValueInput.createByReal(mm(wxb - wxa + 2.0)))
        cut(['HW Wheel'], list(ext.add(e).bodies))
        pr = zp(az, 'Knurl ring plane')
        sr = root.sketches.add(pr)
        sr.name = 'Knob rings'
        sr.isLightBulbOn = False
        toh = lambda x_, y_: sr.modelToSketchSpace(P(x_, y_, az))
        NR = 14
        pitch = (wxb - wxa - 0.8) / NR
        for k_ in range(NR):
            xa_ = wxa + 0.4 + k_ * pitch
            sr.sketchCurves.sketchLines.addTwoPointRectangle(toh(xa_, ay + KR - 0.3), toh(xa_ + 0.4, ay + KR + 0.6))
        axis_ln = sr.sketchCurves.sketchLines.addByTwoPoints(toh(wxa - 1.0, ay), toh(wxb + 1.0, ay))
        axis_ln.isConstruction = True
        rvf = root.features.revolveFeatures
        rvi = rvf.createInput(profs_of(sr), axis_ln, Cut)
        rvi.setAngleExtent(False, adsk.core.ValueInput.createByReal(2 * math.pi))
        rvi.participantBodies = [body('HW Wheel')]
        rvf.add(rvi)
        print('encoder: module x0 %.2f, axis y %.2f, knob x %.2f..%.2f dia %.1f, cover x %.2f..%.2f, cutout x %.2f..%.2f (pocket wall at x %.1f)' % (
            MX0, AX_Y, wxa, wxb, p['KNOB_D'], cov_x0, cov_x1, cut_x0, cut_x1, INNER_W / 2))
        # posts under the solid for the module's M2 screws (taps from below)
        BOSS_D, TAP_D, TAP_L = 3.4, 1.6, 5.0
        mh = [(MX0 + 2.7, AX_Y + 11 - 2.7), (MX0 + 2.7, AX_Y + 11 - 19.3)]
        sk, to_sk = sketch(tp(SOLID_Z + 0.2, 'L boss start'), SOLID_Z + 0.2, 'Module bosses (local)')
        for hx_, hy_ in mh:
            sk.sketchCurves.sketchCircles.addByCenterRadius(to_sk(hx_, hy_), mm(BOSS_D / 2))
        e = ext.createInput(profs_of(sk), NewB)
        e.setOneSideExtent(adsk.fusion.DistanceExtentDefinition.create(adsk.core.ValueInput.createByReal(mm(SOLID_Z + 0.2 - (MZ0 + MOD_T)))),
                           adsk.fusion.ExtentDirections.NegativeExtentDirection)
        cut(['Top shell'], list(ext.add(e).bodies), Join)
        sk, to_sk = sketch(tp(MZ0 + MOD_T, 'L boss tap start'), MZ0 + MOD_T, 'Module boss taps (local)')
        for hx_, hy_ in mh:
            sk.sketchCurves.sketchCircles.addByCenterRadius(to_sk(hx_, hy_), mm(TAP_D / 2))
        cut(['Top shell'], extrude_all(sk, TAP_L))

        # e-ink: window (active area + 0.8, 45 degrees to the top), glass pocket, driver-board pocket, J4 notch
        DCX = p['DISP_CX']
        aw, ah = 23.71 + 1.6, 48.55 + 1.6
        dcy = 3.1 - 1.8
        wl, we = DCX - aw / 2, DCX + aw / 2
        sk, to_sk = sketch(tp(ROOF_Z - 0.1, 'L window start'), ROOF_Z - 0.1, 'E-ink window (local)')
        sk.sketchCurves.sketchLines.addTwoPointRectangle(to_sk(wl, dcy - ah / 2), to_sk(we, dcy + ah / 2))
        cut(['Top shell'], extrude_all(sk, 1.5 + 0.1 + 1.9, taper=45.0))
        gx0, gx1 = DCX - 29.2 / 2 - 0.3, DCX + 29.2 / 2 + 0.3
        gy0, gy1 = 3.1 - 59.2 / 2 - 0.3, 3.1 + 59.2 / 2 + 0.3
        sk, to_sk = sketch(tp(ROOF_Z - 1.5, 'L glass pocket start'), ROOF_Z - 1.5, 'E-ink glass pocket (local)')
        sk.sketchCurves.sketchLines.addTwoPointRectangle(to_sk(gx0, gy0), to_sk(gx1, gy1))
        cut(['Top shell'], extrude_all(sk, 1.5))
        sk, to_sk = sketch(tp(6.9, 'L display board pocket start'), 6.9, 'E-ink driver board pocket (local)')
        sk.sketchCurves.sketchLines.addTwoPointRectangle(to_sk(143.0 - 21.5 / 2 - 0.3, 13.35 - 34.7 / 2 - 0.3),
                                                         to_sk(143.0 + 21.5 / 2 + 0.3, 13.35 + 34.7 / 2 + 0.3))
        sk.sketchCurves.sketchLines.addTwoPointRectangle(to_sk(129.1, -14.7), to_sk(150.4, -11.2))   # J4's header
        cut(['Top shell'], extrude_all(sk, 10.6 - 6.9))     # up to 10.6: the board's top edge (10.4) clears the glass pocket's edge
        # LED guide pocket under the row, and the five holes, stretched to the window's width
        LEDX = [129.25 - 0.3, 159.75 + 0.3]
        sk, to_sk = sketch(tp(6.9, 'L led guide pocket start'), 6.9, 'LED guide pocket (local)')
        sk.sketchCurves.sketchLines.addTwoPointRectangle(to_sk(LEDX[0], -33.95 - 0.3), to_sk(LEDX[1], -28.2 + 0.3))
        cut(['Top shell'], extrude_all(sk, ROOF_Z - 6.9))
        led_first, led_last = p['DISP_CX'] - OUTER_HALF + 1.5, p['DISP_CX'] + OUTER_HALF - 1.5     # holes of dia 3.0: outer edges flush with the frame's
        led_x = [led_first + i * (led_last - led_first) / 4 for i in range(5)]
        sk, to_sk = sketch(tp(ROOF_Z - 1.0, 'L led holes start'), ROOF_Z - 1.0, 'LED holes (local)')
        for lx in led_x:
            sk.sketchCurves.sketchCircles.addByCenterRadius(to_sk(lx, -30.95), mm(1.5))
        cut(['Top shell'], extrude_all(sk, 4.2))
        print('LED hole x', [round(v, 3) for v in led_x], 'window x', round(wl, 3), round(we, 3))
        mb_led = [134.75, 140.25, 145.75, 151.25, 156.75]       # the LEDs: D4-D7 stay, D3 moves 2.5 to an equal 5.5 pitch; the printed guide's bores fan out to the holes
        print('LED hole x %s; board LED x %s; bore jogs %s' % ([round(v, 3) for v in led_x], mb_led, [round(a - b, 2) for a, b in zip(led_x, mb_led)]))

        # ---- LED light guide (printed, opaque): one bore per LED, so the five LEDs cannot bleed into each other. A block in the
        # guide pocket over the plate, with a spigot into each plate window down to the PCB top (the sealing face); each bore
        # runs from the LED's cutout (3.4) to its hole in the roof (3.0), with 2+ mm of wall between neighbours
        gsk, gto = sketch(tp(6.6, 'L guide block start'), 6.6, 'LED guide block (local)')
        gsk.sketchCurves.sketchLines.addTwoPointRectangle(gto(LEDX[0] + 0.15, -33.95 - 0.15), gto(LEDX[1] - 0.15, -28.2 + 0.15))
        guide = extrude_all(gsk, ROOF_Z - 6.6 - 0.05)[0]
        guide.name = 'HW LED guide'
        gsp, gspt = sketch(tp(PCB_T, 'L guide spigot start'), PCB_T, 'LED guide spigots (local)')
        for lx_ in mb_led:
            gsp.sketchCurves.sketchCircles.addByCenterRadius(gspt(lx_, -30.95), mm(1.9))
        cut(['HW LED guide'], list(extrude_all(gsp, 6.6 - PCB_T + 0.2)), Join)
        for bi_, (lm_, lt_) in enumerate(zip(mb_led, led_x)):
            sa_, sat = sketch(tp(PCB_T - 0.1, 'L bore bottom %d' % bi_), PCB_T - 0.1, 'LED bore bottom %d' % bi_)
            sa_.sketchCurves.sketchCircles.addByCenterRadius(sat(lm_, -30.95), mm(1.7))
            sb_, sbt = sketch(tp(ROOF_Z + 0.1, 'L bore top %d' % bi_), ROOF_Z + 0.1, 'LED bore top %d' % bi_)
            sb_.sketchCurves.sketchCircles.addByCenterRadius(sbt(lt_, -30.95), mm(1.5))
            li_ = root.features.loftFeatures.createInput(NewB)
            li_.loftSections.add(sa_.profiles.item(0))
            li_.loftSections.add(sb_.profiles.item(0))
            li_.isSolid = True
            bore = root.features.loftFeatures.add(li_).bodies.item(0)
            bore.name = 'LED bore %d' % bi_
            cut(['HW LED guide'], [bore])

        # ---- screen FPC: from under the glass at its back edge, out 1.2 mm, folded down (bend radius 1.0) and back under the
        # driver board to its connector. A relief in the top shell lets the fold sit past the glass pocket.
        gy_back = 3.1 + 29.6
        fy_out = gy_back + 1.2
        fx0_, fx1_ = DCX - 8.9 - 0.5, DCX + 8.9 + 0.5
        sk, to_sk = sketch(tp(7.0, 'L fpc relief start'), 7.0, 'Screen FPC fold relief (local)')
        sk.sketchCurves.sketchLines.addTwoPointRectangle(to_sk(fx0_, 29.0), to_sk(fx1_, fy_out + 1.2))
        cut(['Top shell'], extrude_all(sk, 11.6 - 7.0))
        fpc_l = [(gy_back - 0.5, 10.7), (fy_out, 10.7), (fy_out, 8.2), (26.0, 8.2)]
        fpc_w = [(DCX,) + l2w(DCX, yy_, zz_)[1:] for yy_, zz_ in fpc_l]
        fpc_body, fpc_len = ribbon_chain(fpc_w, 17.8, 0.3, 1.0, 'HW Screen FPC')
        print('screen FPC: %.1f mm of ribbon, fold %.1f mm past the glass edge' % (fpc_len, fy_out - gy_back))

    # ---- underside
    if p['UNDERSIDE']:
        # flat bottom. Weight bar recess in the middle of the underside (bar flush with the face, 0.1 proud of the recess floor)
        # The weight drops into the bottom block from inside: its face shows through a window in the underside, a flange on a ledge round
        # the window stops it falling out, four screws from inside hold it down (no screw heads on the bottom). Its top follows the stepped
        # floor, flush 0.1 under it, so the floor above keeps its shape.
        def stepz(ea_):
            return ea_ * S + FLOOR_L * C + LIFT

        def stepy(ea_):
            return ea_ * C - FLOOR_L * S
        Z_FRONT, Z_BACK, Y_STEP = stepz(-21.40) - 0.1, stepz(-2.36) - 0.1, stepy(-2.36)     # weight top in front of / behind the step at y -2.04
        WIN_X, WIN_Y0, WIN_Y1 = 92.5, -16.5, 14.1          # the weight's face (the window)
        FL_X, FL_Y0, FL_Y1 = 108.0, -19.0, 27.5            # its flange: long ends and a third floor level behind, for screws spaced far apart
        FL_T = 1.5
        LEDGE_F, LEDGE_B = Z_FRONT - FL_T, Z_BACK - FL_T
        Z3, Y_STEP2 = stepz(16.69) - 0.1, stepy(16.69)      # the floor's next step at y 16.96
        LEDGE_C = Z3 - FL_T
        R_WIN, R_FL = 4.0, 5.0                              # plan corner radii of the face and the flange
        WCY = (WIN_Y0 + WIN_Y1) / 2

        def wbox(x0_, x1_, y0_, y1_, z0_, z1_, name):
            skw = root.sketches.add(zp(z0_, name + ' plane'))
            skw.name = name
            skw.isLightBulbOn = False
            skw.sketchCurves.sketchLines.addTwoPointRectangle(skw.modelToSketchSpace(P(x0_, y0_, z0_)), skw.modelToSketchSpace(P(x1_, y1_, z0_)))
            e_ = ext.createInput(skw.profiles.item(0), NewB)
            e_.setDistanceExtent(False, adsk.core.ValueInput.createByReal(mm(z1_ - z0_)))
            b_ = ext.add(e_).bodies.item(0)
            b_.name = name
            return b_

        def wrr(x0_, x1_, y0_, y1_, z0_, z1_, r_, name):
            """A rounded-rectangle prism in plan."""
            skw = root.sketches.add(zp(z0_, name + ' plane'))
            skw.name = name
            skw.isLightBulbOn = False
            rrect(skw, lambda x_, y_: skw.modelToSketchSpace(P(x_, y_, z0_)), (x0_ + x1_) / 2, (y0_ + y1_) / 2, (x1_ - x0_) / 2, (y1_ - y0_) / 2, r_)
            e_ = ext.createInput(skw.profiles.item(0), NewB)
            e_.setDistanceExtent(False, adsk.core.ValueInput.createByReal(mm(z1_ - z0_)))
            b_ = ext.add(e_).bodies.item(0)
            b_.name = name
            return b_

        wrr(-WIN_X - 0.1, WIN_X + 0.1, WIN_Y0 - 0.1, WIN_Y1 + 0.1, -0.2, 12.0, R_WIN + 0.1, 'Weight window tool')
        wrr(-FL_X - 0.2, FL_X + 0.2, FL_Y0 - 0.2, FL_Y1 + 0.2, LEDGE_F, 12.0, R_FL + 0.2, 'Weight ledge tool')
        cut(['Weight ledge tool'], [wbox(-FL_X - 1, FL_X + 1, Y_STEP, FL_Y1 + 1, LEDGE_F - 0.5, LEDGE_B, 'Ledge keep mid')])
        cut(['Weight ledge tool'], [wbox(-FL_X - 1, FL_X + 1, Y_STEP2, FL_Y1 + 1, LEDGE_F - 0.5, LEDGE_C, 'Ledge keep back')])
        cut(['Bottom shell'], [body('Weight window tool'), body('Weight ledge tool')])
        print('weight window x +/-%.1f y %.1f..%.1f, flange x +/-%.1f y %.1f..%.1f, ledge z %.2f (front) %.2f (back), tops %.2f / %.2f, step at y %.2f' % (
            WIN_X, WIN_Y0, WIN_Y1, FL_X, FL_Y0, FL_Y1, LEDGE_F, LEDGE_B, Z_FRONT, Z_BACK, Y_STEP))
        h = 0.1
        # finger scoops on both end faces, along the bottom edge. One fair spline for the arch (a short steep rise on the
        # back side of the peak, a long gentle tail to the front), and a cutter lofted through a few copies of it, each
        # scaled about the peak and set deeper by d = D (1 - f^2)^2: the surface runs into the end face with no slope
        # (tangent blend) and is deepest (D from the nominal end plane) at the bottom edge under the peak.
        yb0, yb1 = Y_FRONT + 0.26, Y_BOT_BACK
        vf, vp, vb = [yb0 + f * (yb1 - yb0) for f in p['SCOOP_T']]       # the arch's ends at the desk and its crest, world y
        hblock = vp * math.tan(math.radians(TILT)) + LIFT + RING_BOT / C   # ring underside at the crest, world z
        Hc = p['SCOOP_PEAK'] * hblock                                    # crest height at the end face
        D0, A = p['SCOOP_D'], p['SCOOP_A']
        # Each scoop is one smooth surface: the cutter is a spindle (a spline profile revolved about an axis parallel to y, A is the
        # radius of the cross-section at the crest). Its axis stands E outside the end face and D_ under the desk, so the end face
        # cuts it in the side-view arch (crest Hc, gentle on the front side, steep behind) and the desk cuts it in the plan arc
        # (D0 deep at the crest). One revolved surface, no seams across it.
        lo, hi = 0.0, 0.99 * A
        for _ in range(100):
            D_ = (lo + hi) / 2
            E_ = math.sqrt(A * A - D_ * D_) - D0
            H_ = math.sqrt(max(A * A - E_ * E_, 0.0)) - D_
            if H_ > Hc:
                lo = D_
            else:
                hi = D_
        D_ = (lo + hi) / 2
        E_ = math.sqrt(A * A - D_ * D_) - D0
        R0 = math.sqrt(E_ * E_ + D_ * D_)                                # radius at which the arch meets the desk
        kk = math.sqrt(1.0 - (R0 / A) ** 2)
        Bf, Bb = (vp - vf) / kk, (vb - vp) / kk                          # half lengths of the spindle either side of the crest
        r_min = 0.62 * A
        ph0 = math.asin(r_min / A)
        NSC = 16
        pl_sc = zp(-D_, 'Scoop axis plane')
        for sgn in (1, -1):
            xo = OUT_XR if sgn > 0 else -OUT_XL
            xc = xo + sgn * E_
            sks = root.sketches.add(pl_sc)
            sks.name = 'Scoop profile %+d' % sgn
            sks.isLightBulbOn = False
            prof = []
            for k in range(NSC + 1):                                     # front half, from the cut-off up to the crest
                ph = ph0 + (math.pi / 2 - ph0) * k / NSC
                prof.append((vp - Bf * math.cos(ph), A * math.sin(ph)))
            for k in range(1, NSC + 1):                                  # back half, from the crest down to the cut-off
                ph = math.pi / 2 - (math.pi / 2 - ph0) * k / NSC
                prof.append((vp + Bb * math.cos(ph), A * math.sin(ph)))
            pts_ = adsk.core.ObjectCollection.create()
            for yy, rr_ in prof:
                pts_.add(sks.modelToSketchSpace(P(xc - sgn * rr_, yy, -D_)))
            spl_ = sks.sketchCurves.sketchFittedSplines.add(pts_)
            ln_ = sks.sketchCurves.sketchLines
            ya, yb_ = prof[0][0], prof[-1][0]
            k1_ = ln_.addByTwoPoints(spl_.endSketchPoint, sks.modelToSketchSpace(P(xc, yb_, -D_)))
            axis_ = ln_.addByTwoPoints(k1_.endSketchPoint, sks.modelToSketchSpace(P(xc, ya, -D_)))
            ln_.addByTwoPoints(axis_.endSketchPoint, spl_.startSketchPoint)
            rvf_ = root.features.revolveFeatures
            rvi_ = rvf_.createInput(sks.profiles.item(0), axis_, NewB)
            rvi_.setAngleExtent(False, adsk.core.ValueInput.createByReal(2 * math.pi))
            tool_ = rvf_.add(rvi_).bodies.item(0)
            tool_.name = 'Scoop tool %+d' % sgn
            cut(['Bottom shell'], [tool_])
        print('scoop: spindle A %.1f, axis %.2f outside the end face and %.2f under the desk; arch %.1f..%.1f..%.1f at the desk, crest %.2f of block %.2f, depth %.1f' % (
            A, E_, D_, vf, vp, vb, Hc, hblock, D0))
        skf = root.sketches.add(zp(-0.5, 'foot plane'))
        skf.name = 'Foot pockets (world plan)'
        skf.isLightBulbOn = False
        for fx, fy in FOOT_XY:
            rrect(skf, lambda x, y: skf.modelToSketchSpace(P(x, y, -0.5)), fx, fy, FOOT_POCKET[0] / 2, FOOT_POCKET[1] / 2, 1.0)
        e = ext.createInput(profs_of(skf), NewB)
        e.setDistanceExtent(False, adsk.core.ValueInput.createByReal(mm(1.5)))
        cut(['Bottom shell'], list(ext.add(e).bodies))
        # USB-C at the back, centred: a 12.4 x 6.6 lead-in recess and the receptacle opening through the wall
        zc = USB_Z + PCB_T + USB_SHELL[1] / 2
        y_face = Y_BACK - 1.0

        def stadium(yc_, w, hh, depth, name, xc=0.0, zcc=None):
            def mk(off):
                i = planes.createInput()
                i.setByOffset(root.xZConstructionPlane, adsk.core.ValueInput.createByReal(mm(off)))
                return planes.add(i)
            pl = mk(yc_)
            if abs(pl.geometry.origin.y * 10 - yc_) > 0.05:     # the XZ plane's normal points -Y
                pl.deleteMe()
                pl = mk(-yc_)
            pl.name = name + ' plane'
            pl.isLightBulbOn = False
            sk = root.sketches.add(pl)
            sk.name = name
            sk.isLightBulbOn = False
            rrect(sk, lambda x, z: sk.modelToSketchSpace(P(x, yc_, z)), xc, zc if zcc is None else zcc, w / 2, hh / 2, hh / 2 - 0.02)
            e = ext.createInput(sk.profiles.item(0), NewB)
            e.setSymmetricExtent(adsk.core.ValueInput.createByReal(mm(depth)), True)
            return ext.add(e).bodies.item(0)

        GAP_P, RECESS_W = 0.2, 1.1                       # plug shell to opening; width of the recess ring round the opening
        port = stadium(Y_BACK - 7.0, USB_SHELL[0] + 2 * GAP_P, USB_SHELL[1] + 2 * GAP_P, 16.0, 'USB-C port')      # full stadium, even 0.2 gap round the shell
        lead = stadium(Y_BACK, USB_SHELL[0] + 2 * (GAP_P + RECESS_W), USB_SHELL[1] + 2 * (GAP_P + RECESS_W), 2.0, 'USB-C lead-in')   # same stadium offset by 1.1, 1.0 deep
        cut(['Bottom shell'], [port, lead])
        # the daughterboard's pocket (flat on a ledge at USB_Z), a relief for the connector's legs, two M3 taps
        y_edge = Y_BACK - 1.0 - 1.3
        ux0, ux1 = p['DAUGHTER_X'][0] - 0.4, p['DAUGHTER_X'][1] + 0.4

        def hbox(x0_, x1_, y0_, y1_, z0_, z1_, name):
            skx = root.sketches.add(zp(z0_, name + ' plane'))
            skx.name = name
            skx.isLightBulbOn = False
            skx.sketchCurves.sketchLines.addTwoPointRectangle(skx.modelToSketchSpace(P(x0_, y0_, z0_)), skx.modelToSketchSpace(P(x1_, y1_, z0_)))
            e = ext.createInput(skx.profiles.item(0), NewB)
            e.setDistanceExtent(False, adsk.core.ValueInput.createByReal(mm(z1_ - z0_)))
            return ext.add(e).bodies.item(0)

        usb_tools = [hbox(ux0, ux1, 55.0, y_edge + 0.2, USB_Z, USB_Z + PCB_T + 4.0, 'USB board pocket'),
                     hbox(ux0, ux1, 52.0, 61.6, USB_Z, 32.0, 'USB board way in'),
                     hbox(-5.5, 5.5, y_edge - 6.0, y_edge + 0.2, USB_Z - 1.0, USB_Z, 'USB leg relief')]
        # power slide switch (C&K PCM12) on the board's top at x 27.4: its lever sticks 0.75 mm past the board's back edge
        # (1.57 mm inside the case's back face), 0.75 above the PCB top. A slot through the back wall reaches it: wide
        # enough for the lever's travel with 0.45 mm to spare at each end, and a tunnel 1.6 mm deep, so it is reachable with a
        # pin or a fingernail but not knocked by accident.
        px, pw, ph = p['POWER_SLOT']
        pz = USB_Z + PCB_T + 0.75
        CAP_W, CAP_H, CAP_CLR, TRAVEL = 3.4, 2.0, 0.2, 1.5
        usb_tools.append(stadium(Y_BACK - 1.55, CAP_W + TRAVEL + 2 * CAP_CLR, CAP_H + 2 * CAP_CLR, 4.1, 'Power switch access slot', xc=px, zcc=pz))
        skt = root.sketches.add(zp(USB_Z - 4.8, 'usb tap plane'))
        skt.name = 'USB board taps'
        skt.isLightBulbOn = False
        for tx in p['DAUGHTER_TAPS']:
            skt.sketchCurves.sketchCircles.addByCenterRadius(skt.modelToSketchSpace(P(tx, Y_BACK - 1.0 - (104.5 - 98.7), USB_Z - 4.8)), mm(1.25))
        e = ext.createInput(profs_of(skt), NewB)
        e.setDistanceExtent(False, adsk.core.ValueInput.createByReal(mm(4.9)))
        usb_tools += list(ext.add(e).bodies)
        cut(['Bottom shell'], usb_tools)
        try:
            bt_ = body('Bottom shell')
            es_ = adsk.core.ObjectCollection.create()
            for e_ in bt_.edges:
                bb_ = e_.boundingBox
                x0_, x1_, y0_, y1_, z0_, z1_ = [v * 10 for v in (bb_.minPoint.x, bb_.maxPoint.x, bb_.minPoint.y, bb_.maxPoint.y, bb_.minPoint.z, bb_.maxPoint.z)]
                near_wall = y0_ > Y_BACK - 0.45 and y1_ < Y_BACK + 0.6
                in_usb = x0_ > -7.0 and x1_ < 7.0 and z0_ > 6.8 and z1_ < 13.8
                in_slot = x0_ > px - 3.8 and x1_ < px + 3.8 and z0_ > 7.6 and z1_ < 11.2
                if near_wall and (in_usb or in_slot):
                    es_.add(e_)
            chf_ = root.features.chamferFeatures
            ci_ = chf_.createInput2()
            ci_.chamferEdgeSets.addEqualDistanceChamferEdgeSet(es_, adsk.core.ValueInput.createByReal(mm(0.25)), True)
            chf_.add(ci_)
            print('chamfered %d edges round the USB-C recess and the switch slot' % es_.count)
        except Exception as ex_:
            print('chamfer failed:', str(ex_)[:120])
        # the power switch's slider cap: machined aluminium, 3.4 x 2.0, riding in the slot (0.2 clear above and below, 0.9 at each end for
        # the lever's 1.5 travel), fitted over the lever's 0.75 tip, 0.3 proud of the face, three grip ribs. No stock cap exists for the
        # PCM12's 1.5 mm actuator.
        cap = stadium(Y_BACK - 0.8, CAP_W, CAP_H, 2.2, 'HW Power slider cap', xc=px, zcc=pz)
        cap.name = 'HW Power slider cap'
        cap_tools = [hbox(px + k_ * 1.0 - 0.15, px + k_ * 1.0 + 0.15, Y_BACK, Y_BACK + 0.6, pz - 1.2, pz + 1.2, 'Cap rib %d' % k_) for k_ in (-1, 0, 1)]
        cap_tools.append(hbox(px - 0.65, px + 0.65, Y_BACK - 2.1, Y_BACK - 0.9, pz - 0.5, pz + 0.5, 'Cap lever pocket'))
        cut(['HW Power slider cap'], cap_tools)

        # an even small fillet all round the bottom edge, continuous through the scoops (the bottom face's outer loop)
        try:
            if p['BOTTOM_FILLET'] <= 0:
                raise ValueError('no bottom edge fillet (removed)')
            bt = body('Bottom shell')
            bface, best_a = None, 0.0
            for f_ in bt.faces:
                if (f_.geometry.surfaceType == adsk.core.SurfaceTypes.PlaneSurfaceType and abs(nrm(f_).z + 1) < 1e-4
                        and f_.boundingBox.maxPoint.z * 10 < 0.05 and f_.area > best_a):
                    bface, best_a = f_, f_.area
            outer = [lp_ for lp_ in bface.loops if lp_.isOuter][0]
            es = adsk.core.ObjectCollection.create()
            for e_ in outer.edges:
                # the scoops' mouths meet the bottom face tangentially at their tails (no fillet can follow them there):
                # fillet the rest of the perimeter, which is one chain, and leave the scoop's own edges
                if any(g_.geometry.surfaceType == adsk.core.SurfaceTypes.NurbsSurfaceType for g_ in e_.faces):
                    continue
                bb_ = e_.boundingBox
                if not (max(abs(bb_.minPoint.x), abs(bb_.maxPoint.x)) * 10 > 172 or max(abs(bb_.minPoint.y), abs(bb_.maxPoint.y)) * 10 > 75):
                    continue
                es.add(e_)
            fi_ = root.features.filletFeatures
            fin_ = fi_.createInput()
            fin_.addConstantRadiusEdgeSet(es, adsk.core.ValueInput.createByReal(mm(p['BOTTOM_FILLET'])), True)
            fi_.add(fin_)
            print('bottom edge filleted: %d edges, R %.2f' % (es.count, p['BOTTOM_FILLET']))
        except Exception as ex_:
            print('bottom edge fillet failed:', str(ex_)[:150])

        # hardware shown with the case: the accent weight bar (brass) with a 4 x 4 perforation pattern at each
        # end, the four rubber feet, and the eight M3 x 8 socket-head screws with their hex sockets
        wrr(-WIN_X, WIN_X, WIN_Y0, WIN_Y1, h, Z_BACK, R_WIN, 'HW Accent weight bar')
        wrr(-FL_X, FL_X, FL_Y0, FL_Y1, LEDGE_F, Z3, R_FL, 'Weight flange')
        cut(['Weight flange'], [wbox(-FL_X - 1, FL_X + 1, Y_STEP, FL_Y1 + 1, LEDGE_F - 0.5, LEDGE_B, 'Flange trim mid')])
        cut(['Weight flange'], [wbox(-FL_X - 1, FL_X + 1, Y_STEP2, FL_Y1 + 1, LEDGE_F - 0.5, LEDGE_C, 'Flange trim back')])
        cut(['Weight flange'], [wbox(-FL_X - 1, FL_X + 1, FL_Y0 - 1, Y_STEP, Z_FRONT, Z3 + 1, 'Flange top front')])
        cut(['Weight flange'], [wbox(-FL_X - 1, FL_X + 1, Y_STEP, Y_STEP2, Z_BACK, Z3 + 1, 'Flange top mid')])
        cut(['HW Accent weight bar'], [wbox(-WIN_X - 1, WIN_X + 1, WIN_Y0 - 1, Y_STEP, Z_FRONT, Z_BACK + 1, 'Body top front')])
        cut(['HW Accent weight bar'], [body('Weight flange')], Join)

        # four M3 x 5 countersunk screws from inside, as far apart as the flange allows: at the flange's long ends (x +/-103.5) and 20 apart
        # in y (y 2.5 on the floor's 6.5 level, y 22.5 on its 7.85 level); 3.8 of thread in the ledge, over 1.5 of wall to every edge
        WSCREWS = [(-103.5, 2.5, Z_BACK, LEDGE_B), (103.5, 2.5, Z_BACK, LEDGE_B), (-103.5, 22.5, Z3, LEDGE_C), (103.5, 22.5, Z3, LEDGE_C)]
        wcs = None
        for sx_, sy_, zt_, zl_ in WSCREWS:
            q = tbm.createCylinderOrCone(P3(sx_, sy_, zt_ + 0.05), mm(3.05), P3(sx_, sy_, zt_ - 1.35), mm(1.7))
            tbm.booleanOperation(q, tbm.createCylinderOrCone(P3(sx_, sy_, zt_ - 1.35), mm(1.7), P3(sx_, sy_, zl_ - 0.05), mm(1.7)), UNION)
            if wcs is None:
                wcs = q
            else:
                tbm.booleanOperation(wcs, q, UNION)
        cut(['HW Accent weight bar'], [add_temp(wcs, 'Weight countersink tool')])
        for zl_ in sorted(set(s_[3] for s_ in WSCREWS)):
            skt3 = root.sketches.add(zp(zl_, 'weight tap plane %.1f' % zl_))
            skt3.name = 'Weight screw taps %.1f' % zl_
            skt3.isLightBulbOn = False
            for sx_, sy_, zt_, zl2_ in WSCREWS:
                if zl2_ == zl_:
                    skt3.sketchCurves.sketchCircles.addByCenterRadius(skt3.modelToSketchSpace(P(sx_, sy_, zl_)), mm(1.25))
            et = ext.createInput(profs_of(skt3), NewB)
            et.setOneSideExtent(adsk.fusion.DistanceExtentDefinition.create(adsk.core.ValueInput.createByReal(mm(3.8))), adsk.fusion.ExtentDirections.NegativeExtentDirection)
            cut(['Bottom shell'], list(ext.add(et).bodies))
        n_scr = 0
        for sx_, sy_, zt_, zl_ in WSCREWS:
            n_scr += 1
            head = tbm.createCylinderOrCone(P3(sx_, sy_, zt_), mm(2.9), P3(sx_, sy_, zt_ - 1.3), mm(1.5))
            tbm.booleanOperation(head, tbm.createCylinderOrCone(P3(sx_, sy_, zt_ - 1.3), mm(1.5), P3(sx_, sy_, zt_ - 5.0), mm(1.5)), UNION)
            tbm.booleanOperation(head, tbm.createCylinderOrCone(P3(sx_, sy_, zt_ + 0.1), mm(1.0), P3(sx_, sy_, zt_ - 1.0), mm(1.0)), DIFF)
            add_temp(head, 'HW M3x5 CSK screw weight %d' % n_scr)

        # ---- engraving on the polished face (viewed from below, back edge up: the dog at the left, the name at the right; world x = -u).
        # The dachshund is a filled silhouette in a 0.3 deep flat-floored pocket (the matte, bead-blasted floor), with 0.4 wide detail grooves
        # 0.5 deep for the ear, chest, leg separations and the eye.
        def cmr(pts, per=6, closed=False, alpha=0.5):
            """Centripetal Catmull-Rom through the points: smooth, no overshoot or kinks with uneven spacing."""
            n_ = len(pts)
            if closed:
                q = [pts[-1]] + list(pts) + [pts[0], pts[1]]
                segs = range(1, n_ + 1)
            else:
                q = [(2 * pts[0][0] - pts[1][0], 2 * pts[0][1] - pts[1][1])] + list(pts) + [(2 * pts[-1][0] - pts[-2][0], 2 * pts[-1][1] - pts[-2][1])]
                segs = range(1, len(q) - 2)
            out = []
            for i_ in segs:
                p0, p1, p2, p3 = q[i_ - 1], q[i_], q[i_ + 1], q[i_ + 2]
                t0 = 0.0
                t1 = t0 + math.hypot(p1[0] - p0[0], p1[1] - p0[1]) ** alpha
                t2 = t1 + math.hypot(p2[0] - p1[0], p2[1] - p1[1]) ** alpha
                t3 = t2 + math.hypot(p3[0] - p2[0], p3[1] - p2[1]) ** alpha
                for k_ in range(per):
                    t = t1 + (t2 - t1) * k_ / per
                    a1 = [(t1 - t) / (t1 - t0) * p0[j] + (t - t0) / (t1 - t0) * p1[j] for j in (0, 1)]
                    a2 = [(t2 - t) / (t2 - t1) * p1[j] + (t - t1) / (t2 - t1) * p2[j] for j in (0, 1)]
                    a3 = [(t3 - t) / (t3 - t2) * p2[j] + (t - t2) / (t3 - t2) * p3[j] for j in (0, 1)]
                    b1 = [(t2 - t) / (t2 - t0) * a1[j] + (t - t0) / (t2 - t0) * a2[j] for j in (0, 1)]
                    b2 = [(t3 - t) / (t3 - t1) * a2[j] + (t - t1) / (t3 - t1) * a3[j] for j in (0, 1)]
                    out.append(tuple((t2 - t) / (t2 - t1) * b1[j] + (t - t1) / (t2 - t1) * b2[j] for j in (0, 1)))
            if not closed:
                out.append(pts[-1])
            return out

        DOG_OUT = [(0.5, 6.6), (2.4, 9.6), (5.0, 12.8), (8.4, 15.0), (14.0, 16.8), (21.0, 18.4), (28.0, 19.8), (33.5, 21.8), (36.6, 25.2), (39.8, 28.2), (43.4, 29.8), (47.6, 29.0), (50.6, 27.8), (53.0, 26.4), (49.8, 25.0), (46.4, 24.2), (43.4, 22.8), (41.6, 20.6), (41.8, 17.0), (43.0, 14.0), (43.7, 10.0), (44.4, 4.6), (44.9, 1.2), (45.7, 0.1), (41.6, 0.0), (41.3, 1.4), (41.6, 5.4), (40.6, 9.6), (34.0, 11.2), (27.0, 11.0), (21.0, 11.8), (18.8, 8.8), (17.2, 5.0), (17.8, 1.4), (18.0, 0.1), (14.4, 0.0), (14.2, 1.6), (14.8, 5.4), (12.2, 8.8), (9.0, 11.8), (7.0, 11.8), (3.6, 8.4)]
        DOG_LINES = {'ear': [(40.6, 28.6), (38.2, 25.6), (37.2, 22.0), (38.4, 19.0), (40.2, 19.4), (41.0, 21.8)], 'chest': [(41.9, 16.8), (40.6, 14.6), (38.8, 13.8)], 'front legs': [(43.2, 8.6), (43.4, 1.6)], 'hind legs': [(16.4, 8.6), (16.2, 1.8)]}
        EYE = (47.8, 27.4, 0.55)
        DOG_S, DOG_CX = 0.88, 45.0                           # scale and centre (world x) of the dog on the face
        dog_xy = lambda u_, v_: (DOG_CX - DOG_S * (u_ - 26.7), WCY + DOG_S * (v_ - 15.0))

        sk_p = root.sketches.add(zp(h, 'dog pocket plane'))
        sk_p.name = 'Dachshund silhouette (pocket 0.3)'
        sk_p.isLightBulbOn = False
        colp = adsk.core.ObjectCollection.create()
        for uv in cmr(DOG_OUT, 6, True):
            colp.add(sk_p.modelToSketchSpace(P(*dog_xy(*uv), h)))
        spp = sk_p.sketchCurves.sketchFittedSplines.add(colp)
        try:
            spp.isClosed = True
        except Exception:
            pass
        if sk_p.profiles.count == 0:
            sk_p.sketchCurves.sketchLines.addByTwoPoints(spp.endSketchPoint, spp.startSketchPoint)
        tools_030 = extrude_all(sk_p, 0.3)
        # name (sans serif), 0.3 deep
        skx = root.sketches.add(zp(h, 'bar text plane'))
        skx.name = 'Weight engraving'
        skx.isLightBulbOn = False
        ti = skx.sketchTexts.createInput2('Dachshund70', mm(6.0))
        ti.setAsMultiLine(skx.modelToSketchSpace(P(-88, WCY + 4, h)), skx.modelToSketchSpace(P(-12, WCY - 4, h)),
                          adsk.core.HorizontalAlignments.CenterHorizontalAlignment, adsk.core.VerticalAlignments.MiddleVerticalAlignment, 0)
        ti.fontName = 'Arial Rounded MT Bold'
        try:
            ti.isHorizontalFlip = BAR_TEXT_FLIP          # the face looks down: mirror it so it reads from below
        except Exception as ex_:
            print('text flip failed', str(ex_)[:80])
        skx.sketchTexts.add(ti)
        et = ext.createInput(skx.sketchTexts.item(0), NewB)
        et.setDistanceExtent(False, adsk.core.ValueInput.createByReal(mm(0.3)))
        tools_030 += list(ext.add(et).bodies)
        cut(['HW Accent weight bar'], tools_030)
        # detail grooves 0.4 wide, 0.5 deep, with round ends
        HW_ = 0.2
        groove_tools = []
        for nm_, pts_dog in DOG_LINES.items():
            curve = cmr(pts_dog, 8)
            n_ = len(curve)
            left, right = [], []
            for i_ in range(n_):
                pa, pb_ = curve[max(i_ - 1, 0)], curve[min(i_ + 1, n_ - 1)]
                tx_, ty_ = pb_[0] - pa[0], pb_[1] - pa[1]
                tl_ = math.hypot(tx_, ty_)
                nx_, ny_ = -ty_ / tl_, tx_ / tl_
                left.append((curve[i_][0] + nx_ * HW_, curve[i_][1] + ny_ * HW_))
                right.append((curve[i_][0] - nx_ * HW_, curve[i_][1] - ny_ * HW_))
            sk_d = root.sketches.add(zp(h, 'dog line plane ' + nm_))
            sk_d.name = 'Dachshund groove: ' + nm_
            sk_d.isLightBulbOn = False
            to_d = lambda uv: sk_d.modelToSketchSpace(P(*dog_xy(*uv), h))
            col_l, col_r = adsk.core.ObjectCollection.create(), adsk.core.ObjectCollection.create()
            for uv in left:
                col_l.add(to_d(uv))
            for uv in right[::-1]:
                col_r.add(to_d(uv))
            spl_l = sk_d.sketchCurves.sketchFittedSplines.add(col_l)
            spl_r = sk_d.sketchCurves.sketchFittedSplines.add(col_r)
            tl0 = (curve[1][0] - curve[0][0], curve[1][1] - curve[0][1])
            tn0 = (curve[-1][0] - curve[-2][0], curve[-1][1] - curve[-2][1])
            for (pt_, tv_, sa_, sb2_) in ((curve[-1], tn0, spl_l.endSketchPoint, spl_r.startSketchPoint),
                                          (curve[0], (-tl0[0], -tl0[1]), spl_r.endSketchPoint, spl_l.startSketchPoint)):
                tl_ = math.hypot(*tv_)
                mid_ = to_d((pt_[0] + tv_[0] / tl_ * HW_, pt_[1] + tv_[1] / tl_ * HW_))
                sk_d.sketchCurves.sketchArcs.addByThreePoints(sa_, mid_, sb2_)
            groove_tools += extrude_all(sk_d, 0.5)
        sk_e = root.sketches.add(zp(h, 'dog eye plane'))
        sk_e.name = 'Dachshund eye'
        sk_e.isLightBulbOn = False
        sk_e.sketchCurves.sketchCircles.addByCenterRadius(sk_e.modelToSketchSpace(P(*dog_xy(EYE[0], EYE[1]), h)), mm(EYE[2]))
        groove_tools += extrude_all(sk_e, 0.5)
        cut(['HW Accent weight bar'], groove_tools)

        # small chamfered frame round the face's edge
        try:
            wbod = body('HW Accent weight bar')
            fbest, abest = None, 0.0
            for f_ in wbod.faces:
                if (f_.geometry.surfaceType == adsk.core.SurfaceTypes.PlaneSurfaceType and abs(nrm(f_).z + 1) < 1e-4
                        and f_.boundingBox.maxPoint.z * 10 < h + 0.01 and f_.area > abest):
                    fbest, abest = f_, f_.area
            outer = [lp_ for lp_ in fbest.loops if lp_.isOuter][0]
            es_ = adsk.core.ObjectCollection.create()
            for e_ in outer.edges:
                es_.add(e_)
            chf_ = root.features.chamferFeatures
            ci_ = chf_.createInput2()
            ci_.chamferEdgeSets.addEqualDistanceChamferEdgeSet(es_, adsk.core.ValueInput.createByReal(mm(0.4)), True)
            chf_.add(ci_)
        except Exception as ex_:
            print('weight chamfer failed:', str(ex_)[:100])
        wbod = body('HW Accent weight bar')
        print('weight: face %.1f x %.1f (R4), flange %.1f x %.1f (R5), thickness %.1f (front) %.1f (back): %.2f cm3 -> brass %.0f g, steel %.0f g, aluminium %.0f g, titanium %.0f g; screws %d M3x5 CSK' % (
            2 * WIN_X, WIN_Y1 - WIN_Y0, 2 * FL_X, FL_Y1 - FL_Y0, Z_FRONT - h, Z_BACK - h, wbod.volume, wbod.volume * 8.5, wbod.volume * 7.85,
            wbod.volume * 2.70, wbod.volume * 4.43, n_scr))
        try:
            lib = [l for l in app.materialLibraries if l.name == 'Fusion Appearance Library'][0]
            polished = design.appearances.itemByName('Weight polished brass') or design.appearances.addByCopy(lib.appearances.itemByName('Brass - Polished'), 'Weight polished brass')
            satin = design.appearances.itemByName('Weight satin brass') or design.appearances.addByCopy(lib.appearances.itemByName('Brass - Matte'), 'Weight satin brass')
            wbod.appearance = polished
            for f_ in wbod.faces:
                if (f_.geometry.surfaceType == adsk.core.SurfaceTypes.PlaneSurfaceType and abs(nrm(f_).z + 1) < 1e-4
                        and abs(f_.boundingBox.minPoint.z * 10 - (h + 0.3)) < 0.01):
                    f_.appearance = satin
        except Exception as ex_:
            print('weight appearance failed:', str(ex_)[:100])
        skf2 = root.sketches.add(zp(-1.0, 'foot body plane'))
        skf2.name = 'Rails (world plan)'
        skf2.isLightBulbOn = False
        for fx, fy in FOOT_XY:
            rrect(skf2, lambda x, y: skf2.modelToSketchSpace(P(x, y, -1.0)), fx, fy, FOOT[0] / 2, FOOT[1] / 2, 1.0)
        for fi, fb in enumerate(extrude_all(skf2, FOOT[2])):
            fb.name = 'HW Rail %d' % (fi + 1)
        for yrow, lab in rows:
            L = SCREW_LEN[lab]
            for si, x in enumerate(SCREW_X):
                head = tbm.createCylinderOrCone(P3(x, yrow, 0.0), mm(2.75), P3(x, yrow, 1.25), mm(1.5))
                tbm.booleanOperation(head, tbm.createCylinderOrCone(P3(x, yrow, 1.25), mm(1.5), P3(x, yrow, L), mm(1.5)), UNION)
                tbm.booleanOperation(head, tbm.createCylinderOrCone(P3(x, yrow, -0.1), mm(1.15), P3(x, yrow, 1.0), mm(1.15)), DIFF)
                add_temp(head, 'HW M3x%d CSK screw %s %d' % (L, lab, si + 1))


    # ---- appearances
    ap = design.appearances
    grey, acc = ap.itemByName('Case grey'), ap.itemByName('Ring accent')
    for b in root.bRepBodies:
        if b.name == 'HW Accent weight bar' and b.appearance is not None and b.appearance.name.startswith('Weight'):
            continue
        b.appearance = acc if b.name in ('Accent ring', 'HW Accent weight bar', 'HW Power slider cap') else grey
    for s_ in design.analyses.sectionAnalyses:
        s_.isLightBulbOn = False
    print('params', p)
    print('outline x %.2f..%.2f (length %.2f)  world y at the desk %.3f..%.3f (depth %.2f)' % (-OUT_XL, OUT_XR, 2 * OUT_X, Y_FRONT, Y_BOT_BACK, Y_BOT_BACK - Y_FRONT))
    for b in root.bRepBodies:
        print(b.name, bbox(b), 'vol %.2f' % b.volume, 'valid', b.isValid, 'lumps', b.lumps.count)
    return design
