import adsk.core, adsk.fusion, math
REPO = 'C:/path/to/Dachshund70/'   # EDIT: the repo root (with a trailing slash)
HERE = REPO + 'hardware/case/fusion/scripts/'

STEP_DIR = REPO + 'hardware/step/'


def mm(v):
    return v / 10.0


def P(x, y, z):
    return adsk.core.Point3D.create(mm(x), mm(y), mm(z))


class Maker:
    """Builds bodies in a new component, in the component's own frame (millimetres)."""

    def __init__(self, root, name):
        self.occ = root.occurrences.addNewComponent(adsk.core.Matrix3D.create())
        self.comp = self.occ.component
        self.comp.name = name
        self.ext = self.comp.features.extrudeFeatures
        self.n = 0

    def plane(self, z):
        i = self.comp.constructionPlanes.createInput()
        i.setByOffset(self.comp.xYConstructionPlane, adsk.core.ValueInput.createByReal(mm(z)))
        pl = self.comp.constructionPlanes.add(i)
        pl.isLightBulbOn = False
        return pl

    def box(self, x0, x1, y0, y1, z0, z1, name, r=0.0):
        sk = self.comp.sketches.add(self.plane(z0))
        sk.isLightBulbOn = False
        to = lambda x, y: sk.modelToSketchSpace(P(x, y, z0))
        if r <= 0:
            sk.sketchCurves.sketchLines.addTwoPointRectangle(to(x0, y0), to(x1, y1))
        else:
            self.rounded(sk, to, x0, x1, y0, y1, r)
        e = self.ext.createInput(sk.profiles.item(0), adsk.fusion.FeatureOperations.NewBodyFeatureOperation)
        e.setDistanceExtent(False, adsk.core.ValueInput.createByReal(mm(z1 - z0)))
        b = self.ext.add(e).bodies.item(0)
        b.name = name
        return b

    def rounded(self, sk, to, x0, x1, y0, y1, r):
        lines, arcs = sk.sketchCurves.sketchLines, sk.sketchCurves.sketchArcs
        k = r * math.sqrt(0.5)
        cs = [((x1 - r, y0), (x1 - r + k, y0 + r - k), (x1, y0 + r)),
              ((x1, y1 - r), (x1 - r + k, y1 - r + k), (x1 - r, y1)),
              ((x0 + r, y1), (x0 + r - k, y1 - r + k), (x0, y1 - r)),
              ((x0, y0 + r), (x0 + r - k, y0 + r - k), (x0 + r, y0))]
        ends = []
        for a, m, b in cs:
            pa, pb = to(*a), to(*b)
            arc = arcs.addByThreePoints(pa, to(*m), pb)
            e0, e1 = arc.startSketchPoint, arc.endSketchPoint
            ends.append((e0, e1) if e0.geometry.distanceTo(pa) <= e1.geometry.distanceTo(pa) else (e1, e0))
        for i in range(4):
            lines.addByTwoPoints(ends[i][1], ends[(i + 1) % 4][0])

    def cyl(self, x0, x1, yc, zc, dia, name):
        i = self.comp.constructionPlanes.createInput()
        i.setByOffset(self.comp.yZConstructionPlane, adsk.core.ValueInput.createByReal(mm(x0)))
        pl = self.comp.constructionPlanes.add(i)
        pl.isLightBulbOn = False
        sk = self.comp.sketches.add(pl)
        sk.isLightBulbOn = False
        sk.sketchCurves.sketchCircles.addByCenterRadius(sk.modelToSketchSpace(P(x0, yc, zc)), mm(dia / 2))
        e = self.ext.createInput(sk.profiles.item(0), adsk.fusion.FeatureOperations.NewBodyFeatureOperation)
        e.setDistanceExtent(False, adsk.core.ValueInput.createByReal(mm(x1 - x0)))
        b = self.ext.add(e).bodies.item(0)
        b.name = name
        return b

    def color(self, design, body, appearance_name, lib='Fusion Appearance Library'):
        app = adsk.core.Application.get()
        try:
            a = [l for l in app.materialLibraries if l.name == lib][0].appearances.itemByName(appearance_name)
            body.appearance = design.appearances.addByCopy(a, appearance_name + ' (%s)' % body.name)
        except Exception:
            pass

    def export(self, design, filename):
        em = design.exportManager
        opts = em.createSTEPExportOptions(STEP_DIR + filename, self.comp)
        return em.execute(opts)


def battery(design, root):
    """EEMB LP803450: 52.0 x 34.5 x 8.3 mm bare pouch cell (datasheet, no tolerance given) with a protection board,
    two leads and a JST-PH 2-pin plug. Origin: middle of the pouch, bottom at z = 0, long axis X, leads leave +x."""
    m = Maker(root, 'Battery LiPo 803450 (52 x 34.5 x 8.3)')
    body = m.box(-26.0, 26.0, -17.25, 17.25, 0.0, 8.3, 'Pouch', r=1.0)
    m.color(design, body, 'Aluminum - Satin')
    m.box(26.0, 30.5, -14.5, 14.5, 3.3, 5.0, 'Protection board')
    # the leads and the plug are modelled in the case (build_case2.py): they route from here to the mainboard's JST-PH
    return m


def screen(design, root):
    """2.13 inch e-paper panel GDEY0213B74 (glass 29.2 x 59.2 x 1.0, active area 23.71 x 48.55, portrait) with its
    flex and the Adafruit eInk Breakout Friend (21.5 x 34.7 x 1.6). Origin: the middle of the glass, glass top at
    z = 0, +y toward the back (where the flex leaves)."""
    m = Maker(root, 'E-paper 2.13 GDEY0213B74 with driver board')
    m.box(-14.6, 14.6, -29.6, 29.6, -1.0, 0.0, 'Glass', r=0.5)
    m.box(-11.855, 11.855, -1.8 - 24.275, -1.8 + 24.275, -0.1, 0.0, 'Active area')
    # the FPC fold is modelled in the case (build_case2.py), with its relief in the top shell
    # the breakout board hangs behind the glass, offset toward the back and left (gen_case: pcb_c - glass_c)
    bx, by = 143.0 - 144.5, 10.25          # the board stays on the mainboard's header (x 143.0) while the glass sits at x 144.5
    m.box(bx - 10.75, bx + 10.75, by - 17.35, by + 17.35, -2.7, -1.1, 'Driver board', r=1.0)
    m.box(bx - 9.0, bx + 9.0, by + 11.0, by + 16.0, -3.9, -2.7, 'FPC connector')
    m.box(bx - 9.0, bx + 9.0, by - 16.0, by - 13.0, -4.6, -2.7, 'Header and parts')
    return m
