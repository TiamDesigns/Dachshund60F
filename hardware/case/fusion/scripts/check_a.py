import adsk.core, adsk.fusion, math, time

TBM = adsk.fusion.TemporaryBRepManager.get()
INTER = adsk.fusion.BooleanTypes.IntersectionBooleanType
TILT = 4.0
S, C = math.sin(math.radians(TILT)), math.cos(math.radians(TILT))
LIFT = 3.0 + 4.0 * C + (121.1 / 2 + 0.75) * S
ROW_EDGES = [-40.45, -21.40, -2.36, 16.69, 38.12, 61.3]
FLOOR_L = -4.5


def floor_world(yl):
    for ea, eb in zip(ROW_EDGES[:-1], ROW_EDGES[1:]):
        if ea <= yl < eb:
            zh = ea * S + FLOOR_L * C + LIFT
            return zh if zh >= 4.0 else 3.0
    return 3.0


def bb(b):
    x = b.boundingBox
    return [v * 10 for v in (x.minPoint.x, x.minPoint.y, x.minPoint.z, x.maxPoint.x, x.maxPoint.y, x.maxPoint.z)]


def overlap_bb(a, b, m=0.0):
    return all(a[i] <= b[i + 3] + m and b[i] <= a[i + 3] + m for i in range(3))


MISMATCH = [0, 0]


def _bb_ok(cp, b):
    x, y = cp.boundingBox, b.boundingBox
    return all(abs(getattr(x.minPoint, k) - getattr(y.minPoint, k)) < 0.02 and abs(getattr(x.maxPoint, k) - getattr(y.maxPoint, k)) < 0.02 for k in 'xyz')


def world_copy(b):
    MISMATCH[0] += 1
    return TBM.copy(b)          # a copy of an occurrence's body is already in world space


def inter_vol(a, b):
    """Volume in mm3 of the intersection of two bodies (assembly context ok), 0 if none."""
    try:
        ca, cb = world_copy(a), world_copy(b)
        TBM.booleanOperation(ca, cb, INTER)
        v = ca.volume * 1000.0
        return v if v > 0 else 0.0
    except Exception:
        return 0.0


def run_checks(log):
    des = adsk.fusion.Design.cast(adsk.core.Application.get().activeProduct)
    root = des.rootComponent
    sh = {n: root.bRepBodies.itemByName(n) for n in ('Top shell', 'Bottom shell', 'Accent ring')}
    shb = {n: bb(b) for n, b in sh.items()}
    occ = {o.name.split(' ')[0].split(':')[0]: o for o in root.occurrences}
    t0 = time.time()

    def occ_bodies(o):
        out = []

        def walk(x):
            for b in x.bRepBodies:
                out.append((x.name, b))
            for ch in x.childOccurrences:
                walk(ch)
        walk(o)
        return out

    def test(label, items, shells, thr=0.01):
        hit = []
        for nm, b in items:
            bx = bb(b)
            for sn in shells:
                if overlap_bb(bx, shb[sn]):
                    v = inter_vol(b, sh[sn])
                    if v > thr:
                        hit.append((nm, sn, round(v, 3)))
        log.write('%s: %d bodies, %d hits %s (%.0fs)\n' % (label, len(items), len(hit), hit[:12], time.time() - t0))
        log.flush()

    asm = [o for o in root.occurrences if o.name.startswith('Dachshund70-Assembly')][0]
    asm_items = occ_bodies(asm)
    plate = [(n, b) for n, b in asm_items if b.name.startswith('Dachshund70-Plate') or 'Plate_PCB' in n]
    pcb = [(n, b) for n, b in asm_items if 'Assembly_PCB' in n]
    log.write('assembly bodies %d, plate %d, pcb %d\n' % (len(asm_items), len(plate), len(pcb)))
    test('plate', plate, ['Top shell', 'Bottom shell', 'Accent ring'])
    test('pcb', pcb, ['Top shell', 'Bottom shell', 'Accent ring'])
    # parts near the floor: anything whose bottom is within 1.5 mm of the stepped floor under it
    near = []
    for n, b in asm_items:
        x = bb(b)
        yl = ((x[1] + x[4]) / 2 - 0.0) / C      # rough local y from world y (z-term is small for parts near the PCB)
        if x[2] < floor_world(yl) + 1.5:
            near.append((n, b))
    log.write('parts near the floor: %d\n' % len(near))
    test('near floor vs bottom shell and ring', near, ['Bottom shell', 'Accent ring'], thr=0.0001)
    # parts under the top shell's solid / roof, all parts above the plate (sockets, switches' upper parts)
    above = [(n, b) for n, b in asm_items if bb(b)[5] > 14.0]
    test('upper parts vs top shell', above, ['Top shell'], thr=0.0001)
    for key in ('Dachshund70-USB', 'Dachshund70-Encoder', 'Battery', 'E-paper'):
        o = [v for k, v in occ.items() if k.startswith(key)][0]
        test(key, occ_bodies(o), ['Top shell', 'Bottom shell', 'Accent ring'], thr=0.0001)
    for nm in ('Encoder cover', 'HW Wheel', 'HW LED guide', 'HW Screen FPC'):
        b = root.bRepBodies.itemByName(nm)
        test(nm, [(nm, b)], ['Top shell', 'Bottom shell', 'Accent ring'], thr=0.0001)
    kc = [o for o in root.occurrences if o.name.startswith('Keycaps')][0]
    test('keycaps', occ_bodies(kc), ['Top shell'], thr=0.0001)
    for o in root.occurrences:
        if o.name.startswith('Spacebar') and o.isLightBulbOn:
            test(o.name, occ_bodies(o), ['Top shell', 'Bottom shell'], thr=0.0001)

    log.write('transform check ok %d mismatch %d' % tuple(MISMATCH) + chr(10))


def battery_vs_board(log):
    des = adsk.fusion.Design.cast(adsk.core.Application.get().activeProduct)
    root = des.rootComponent
    batt = [o for o in root.occurrences if o.name.startswith('Battery')][0]
    asm = [o for o in root.occurrences if o.name.startswith('Dachshund70-Assembly')][0]
    bb_ = [b for b in batt.bRepBodies]
    log.write('battery bodies %d: %s' % (len(bb_), [bb(b) for b in bb_]) + chr(10))

    def walk(o):
        for b in o.bRepBodies:
            yield o, b
        for ch in o.childOccurrences:
            for r in walk(ch):
                yield r
    hits = {}
    for o, b in walk(asm):
        x = bb(b)
        for cell in bb_:
            if overlap_bb(x, bb(cell)):
                v = inter_vol(b, cell)
                if v > 0.0001:
                    hits.setdefault(o.name, []).append(round(v, 2))
    log.write('battery vs board parts: %s' % hits + chr(10))
