import adsk.core, adsk.fusion, math, json

TBM = adsk.fusion.TemporaryBRepManager.get()
TILT = 4.0
S, C = math.sin(math.radians(TILT)), math.cos(math.radians(TILT))
LIFT = 3.0 + 4.0 * C + (121.1 / 2 + 0.75) * S


def collect(log):
    des = adsk.fusion.Design.cast(adsk.core.Application.get().activeProduct)
    root = des.rootComponent
    asm = [o for o in root.occurrences if o.name.startswith('Dachshund70-Assembly')][0]
    inv = asm.transform2.copy()
    inv.invert()
    parts = []

    def walk(o):
        for b in o.bRepBodies:
            yield o, b
        for ch in o.childOccurrences:
            for r in walk(ch):
                yield r
    for o, b in walk(asm):
        if 'Assembly_PCB' in o.name or 'Plate_PCB' in o.name:
            continue
        cp = TBM.copy(b)
        TBM.transform(cp, inv)
        x = cp.boundingBox
        parts.append((o.name, [round(v * 10, 2) for v in (x.minPoint.x, x.minPoint.y, x.minPoint.z, x.maxPoint.x, x.maxPoint.y, x.maxPoint.z)]))
    json.dump(parts, open(log, 'w'))
    return len(parts)
