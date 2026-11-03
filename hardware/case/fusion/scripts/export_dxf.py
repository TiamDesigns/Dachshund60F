import adsk.core, adsk.fusion, math
REPO = 'C:/path/to/Dachshund70/'   # EDIT: the repo root (with a trailing slash)
HERE = REPO + 'hardware/case/fusion/scripts/'

OUT = REPO + 'hardware/case/dxf/'
TILT = 4.0
S, C = math.sin(math.radians(TILT)), math.cos(math.radians(TILT))
LIFT = 3.0 + 4.0 * C + (121.1 / 2 + 0.75) * S


class Dxf:
    def __init__(self, layers):
        self.layers = layers
        self.ents = []

    def line(self, layer, a, b):
        self.ents += ['0', 'LINE', '8', layer, '10', '%.5f' % a[0], '20', '%.5f' % a[1], '30', '0.0', '11', '%.5f' % b[0], '21', '%.5f' % b[1], '31', '0.0']

    def circle(self, layer, c, r):
        self.ents += ['0', 'CIRCLE', '8', layer, '10', '%.5f' % c[0], '20', '%.5f' % c[1], '30', '0.0', '40', '%.5f' % r]

    def arc(self, layer, c, r, a0, a1):
        self.ents += ['0', 'ARC', '8', layer, '10', '%.5f' % c[0], '20', '%.5f' % c[1], '30', '0.0', '40', '%.5f' % r, '50', '%.5f' % a0, '51', '%.5f' % a1]

    def poly(self, layer, pts, closed=True):
        self.ents += ['0', 'POLYLINE', '8', layer, '66', '1', '70', '1' if closed else '0']
        for p in pts:
            self.ents += ['0', 'VERTEX', '8', layer, '10', '%.5f' % p[0], '20', '%.5f' % p[1], '30', '0.0']
        self.ents += ['0', 'SEQEND', '8', layer]

    def save(self, path):
        h = ['0', 'SECTION', '2', 'HEADER', '9', '$ACADVER', '1', 'AC1009', '9', '$INSUNITS', '70', '4', '0', 'ENDSEC']
        t = ['0', 'SECTION', '2', 'TABLES', '0', 'TABLE', '2', 'LAYER', '70', str(len(self.layers))]
        for i, nm in enumerate(self.layers):
            t += ['0', 'LAYER', '2', nm, '70', '0', '62', str(i + 1), '6', 'CONTINUOUS']
        t += ['0', 'ENDTAB', '0', 'ENDSEC']
        e = ['0', 'SECTION', '2', 'ENTITIES'] + self.ents + ['0', 'ENDSEC', '0', 'EOF']
        open(path, 'w', newline='\n').write('\n'.join(h + t + e) + '\n')


def w2l(p):
    """world point (cm) -> local board frame (mm): x, y, z."""
    x, y, z = p.x * 10, p.y * 10, p.z * 10 - LIFT
    return (x, y * C + z * S, -y * S + z * C)


def ring_dxf():
    root = adsk.fusion.Design.cast(adsk.core.Application.get().activeProduct).rootComponent
    r = root.bRepBodies.itemByName('Accent ring')
    best = None
    for f in r.faces:
        if f.geometry.surfaceType == adsk.core.SurfaceTypes.PlaneSurfaceType:
            ok, n = f.evaluator.getNormalAtPoint(f.pointOnFace)
            if n.z > 0.9 and (best is None or f.area > best.area):
                best = f
    d = Dxf(['RING_OUTLINE', 'RING_HOLES'])
    n_ent = 0
    for lp in best.loops:
        layer = 'RING_OUTLINE' if lp.isOuter else 'RING_HOLES'
        for ce in lp.coEdges:
            e = ce.edge
            ct = e.geometry.curveType
            a = w2l(e.startVertex.geometry)[:2]
            b = w2l(e.endVertex.geometry)[:2]
            ev = e.evaluator
            ok, p0, p1 = ev.getParameterExtents()
            ok, pm = ev.getPointAtParameter((p0 + p1) / 2)
            m = w2l(pm)[:2]
            if ct == adsk.core.Curve3DTypes.Line3DCurveType:
                d.line(layer, a, b)
            elif ct == adsk.core.Curve3DTypes.Circle3DCurveType:
                g = e.geometry
                c = w2l(g.center)[:2]
                d.circle(layer, c, g.radius * 10)
            else:
                # arc through a, m, b: circle centre from the three points
                ax, ay = a; bx, by = b; mx, my = m
                dd = 2 * (ax * (my - by) + mx * (by - ay) + bx * (ay - my))
                ux = ((ax**2 + ay**2) * (my - by) + (mx**2 + my**2) * (by - ay) + (bx**2 + by**2) * (ay - my)) / dd
                uy = ((ax**2 + ay**2) * (bx - mx) + (mx**2 + my**2) * (ax - bx) + (bx**2 + by**2) * (mx - ax)) / dd
                rad = math.hypot(ax - ux, ay - uy)
                ang = lambda q: math.degrees(math.atan2(q[1] - uy, q[0] - ux)) % 360
                a0, am, a1 = ang(a), ang(m), ang(b)
                # CCW from a0 to a1 passes am?
                if ((am - a0) % 360) <= ((a1 - a0) % 360):
                    d.arc(layer, (ux, uy), rad, a0, a1)
                else:
                    d.arc(layer, (ux, uy), rad, a1, a0)
            n_ent += 1
    d.save(OUT + 'Dachshund70-Case-Ring.dxf')
    print('ring dxf entities', n_ent, 'loops', best.loops.count)


def loop_pts(lp, fx):
    pts = []
    for ce in lp.coEdges:
        e = ce.edge
        ev = e.evaluator
        ok, p0, p1 = ev.getParameterExtents()
        ok, st = ev.getStrokes(p0, p1, 0.0003)
        seg = [fx(p) for p in st]
        if ce.isParamReversed:
            seg.reverse()
        if pts and abs(pts[-1][0] - seg[0][0]) + abs(pts[-1][1] - seg[0][1]) < 1e-3:
            seg = seg[1:]
        pts += seg
    if len(pts) > 1 and abs(pts[-1][0] - pts[0][0]) + abs(pts[-1][1] - pts[0][1]) < 1e-3:
        pts.pop()
    return pts


def weight_dxf():
    root = adsk.fusion.Design.cast(adsk.core.Application.get().activeProduct).rootComponent
    w = root.bRepBodies.itemByName('HW Accent weight bar')
    # seen from the engraved face (below): reading orientation u = -x, v = y; origin at the face centre
    fx = lambda p: (-p.x * 10, p.y * 10 + 1.2)
    faces = []
    for f in w.faces:
        if f.geometry.surfaceType != adsk.core.SurfaceTypes.PlaneSurfaceType:
            continue
        ok, n = f.evaluator.getNormalAtPoint(f.pointOnFace)
        z = f.pointOnFace.z * 10
        if n.z < -0.99 and 0.3 < z < 0.7:
            faces.append((f, z))
    biggest = max(faces, key=lambda t: t[0].area)[0]
    d = Dxf(['DOG_SILHOUETTE', 'DOG_DETAIL', 'TEXT'])
    cnt = {}
    for f, z in faces:
        cx = f.centroid.x * 10
        if f == biggest:
            layer = 'DOG_SILHOUETTE'
        elif cx > 0:
            layer = 'DOG_DETAIL'
        else:
            layer = 'TEXT'
        for lp in f.loops:
            if not lp.isOuter and f == biggest:
                continue  # the grooves inside the silhouette are output as their own faces
            pts = loop_pts(lp, fx)
            if len(pts) >= 3:
                d.poly(layer, pts)
                cnt[layer] = cnt.get(layer, 0) + 1
    d.save(OUT + 'Dachshund70-Weight-Engraving.dxf')
    print('weight dxf', cnt, 'faces', len(faces))


def run(_context: str):
    ring_dxf()
    weight_dxf()
