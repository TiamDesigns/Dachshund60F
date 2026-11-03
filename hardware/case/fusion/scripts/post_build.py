import adsk.core, adsk.fusion, math
REPO = 'C:/path/to/Dachshund70/'   # EDIT: the repo root (with a trailing slash)
HERE = REPO + 'hardware/case/fusion/scripts/'
KC = REPO + 'hardware/case/step/'

# parts of each spacebar option: caps (in the keycap files, desk frame) and board parts (in the assembly, board frame)
SET_FULL = dict(name='Spacebar 6.25u', caps=['Space (R4, 6.25u)'],
                board=['SW_Cherry_MX_Plate:2', 'Stabilizer_Cherry_MX_6.25u:1'])
SET_SPLIT = dict(name='Spacebar split', caps=['Mod (R4, 1.25u)', 'Space (R4, 2.75u)', 'Mod 2 (R4, 2.25u)'],
                 board=['SW_Cherry_MX_Plate:3', 'SW_Cherry_MX_Plate:1', 'SW_Cherry_MX_PCB:10',
                        'Stabilizer_Cherry_MX_2.00u:1', 'Stabilizer_Cherry_MX_2.00u:4'])


def _mul(a, b):
    """4x4 row-major product a * b of two Matrix3D arrays, as a Matrix3D."""
    A, B = a.asArray(), b.asArray()
    out = [sum(A[4 * i + k] * B[4 * k + j] for k in range(4)) for i in range(4) for j in range(4)]
    m = adsk.core.Matrix3D.create()
    m.setWithArray(out)
    return m


def stage1():
    """Everything that adds timeline items: imports, the two spacebar sets. Transforms are set in stage2()."""
    app = adsk.core.Application.get()
    des = adsk.fusion.Design.cast(app.activeProduct)
    root = des.rootComponent
    ns = {'__name__': 'pm'}
    exec(compile(open(HERE + 'place_models.py').read(), 'place_models.py', 'exec'), ns)
    ns['place_all']()
    im = app.importManager
    for fn in ('Dachshund70-Keycaps-6.25u.step', 'Dachshund70-Keycaps.step'):
        o = im.createSTEPImportOptions(KC + fn)
        o.isViewFit = False
        im.importToTarget(o, root)
    full_caps = [o for o in root.occurrences if o.name.startswith('Keycaps')][0]
    split_caps = [o for o in root.occurrences if o.name.startswith('Keycaps')][1]
    asm = [o for o in root.occurrences if o.name.startswith('Dachshund70-Assembly')][0]
    for spec in (SET_FULL, SET_SPLIT):
        target = root.occurrences.addNewComponent(adsk.core.Matrix3D.create())
        target.component.name = spec['name']
        if spec is SET_FULL:
            for c in list(full_caps.childOccurrences):
                if any(c.name.startswith(n) for n in spec['caps']):
                    c.moveToComponent(target)
        else:
            # the split caps move as the whole keycap file with every other cap hidden (moving them singly drops them)
            for c in split_caps.childOccurrences:
                c.isLightBulbOn = any(c.name.startswith(n) for n in spec['caps'])
            split_caps.moveToComponent(target)
        for c in list(asm.childOccurrences):
            if c.name in spec['board']:
                c.moveToComponent(target)
    print('stage1 done; occurrences:', [o.name for o in root.occurrences])


def stage2():
    """Placement and visibility. Adds no timeline items, and nothing may be created after it."""
    ns = {'__name__': 'pm'}
    exec(compile(open(HERE + 'place_models.py').read(), 'place_models.py', 'exec'), ns)
    lm, wm = ns['local_matrix'], ns['world_matrix']
    X0 = ns['ENC_X0']
    app = adsk.core.Application.get()
    des = adsk.fusion.Design.cast(app.activeProduct)
    root = des.rootComponent
    board = lm(0, 0, 0)
    lift = adsk.core.Matrix3D.create()    # Dachshund70-Keycaps*.step are exported from this model, already in the 4 degree desk frame
    for o in root.occurrences:
        n = o.name
        if n.startswith('Dachshund70-Assembly'):
            o.transform2 = board
        elif n.startswith('Dachshund70-Encoder'):
            o.transform2 = lm(X0 - 99.98, 60.55 + 79.98 + ns['ENC_DY'], 1.8)
        elif n.startswith('Dachshund70-USB'):
            o.transform2 = wm(-121.0, ns['USB_FACE_Y'] + 98.68, ns['USB_Z'])
        elif n.startswith('Battery'):
            o.transform2 = ns['world_matrix_rz'](ns['BATT_CX'], ns['BATT_CY'], ns['BATT_Z0'])
        elif n.startswith('E-paper'):
            o.transform2 = lm(144.5, 3.1, 11.5)
        elif n.startswith('Keycaps'):
            o.transform2 = lift
        elif n.startswith('Spacebar'):
            # the moved parts keep their old parent's frame; give them its placement (the set itself stays at identity)
            for c in o.childOccurrences:
                is_cap = c.name.startswith(('Space (', 'Mod (', 'Keycaps'))
                c.transform2 = _mul(lift if is_cap else board, c.transform2)
    for o in root.occurrences:
        if o.name.startswith('Spacebar split'):
            o.isLightBulbOn = False
    for o in root.occurrences:
        bb = o.boundingBox
        print(o.name, [round(v * 10, 1) for v in (bb.minPoint.x, bb.maxPoint.x, bb.minPoint.y, bb.maxPoint.y, bb.minPoint.z, bb.maxPoint.z)])
