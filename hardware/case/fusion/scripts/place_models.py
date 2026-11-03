import adsk.core, adsk.fusion, math
REPO = 'C:/path/to/Dachshund70/'   # EDIT: the repo root (with a trailing slash)
HERE = REPO + 'hardware/case/fusion/scripts/'

STEP = REPO + 'hardware/step/'
SCRATCH = HERE
TILT = 4.0
ENC_X0 = 125.655
ENC_DY = 50.03 - 49.55      # the module's axis moves onto the F row's centre line
S, C = math.sin(math.radians(TILT)), math.cos(math.radians(TILT))
LIFT = 3.0 + 4.0 * C + (121.1 / 2 + 0.75) * S
RUN_BACK, KEY_Y1, KEY_G, W_FLAT = 10.8, 59.55, 0.475, 6.8
USB_Z = 7.0
USB_ZC = USB_Z + 1.6 + 3.26 / 2
OUT_BACK = KEY_Y1 + KEY_G + W_FLAT + RUN_BACK                       # the back wall, local y (it is square to the plate)
Y_BACK = OUT_BACK * C - ((USB_ZC - OUT_BACK * S - LIFT) / C) * S    # the case's back face at the USB-C's height, world y
USB_FACE_Y = Y_BACK - 1.0                                           # the receptacle face sits 1.0 inside it
BATT_CX, BATT_CY, BATT_Z0 = 142.0, 35.8, 1.2                         # the battery, flat, under the screen and knob (see build_case2.py); world coordinates


def local_matrix(x, y, z):
    wy, wz = y * C - z * S, y * S + z * C + LIFT
    m = adsk.core.Matrix3D.create()
    m.setWithArray([1, 0, 0, x / 10, 0, C, -S, wy / 10, 0, S, C, wz / 10, 0, 0, 0, 1])
    return m


def local_matrix_rz(x, y, z):
    """local_matrix with the part turned -90 degrees about the plate's normal: its x runs toward the front (-y), its y along +x."""
    wy, wz = y * C - z * S, y * S + z * C + LIFT
    m = adsk.core.Matrix3D.create()
    m.setWithArray([0, 1, 0, x / 10, -C, 0, -S, wy / 10, -S, 0, C, wz / 10, 0, 0, 0, 1])
    return m


def world_matrix_rz(x, y, z):
    """A part turned -90 degrees about the vertical: its x runs toward the front (-y), its y toward +x."""
    m = adsk.core.Matrix3D.create()
    m.setWithArray([0, 1, 0, x / 10, -1, 0, 0, y / 10, 0, 0, 1, z / 10, 0, 0, 0, 1])
    return m


def world_matrix(x, y, z):
    m = adsk.core.Matrix3D.create()
    m.translation = adsk.core.Vector3D.create(x / 10, y / 10, z / 10)
    return m


def _rx(deg):
    a = math.radians(deg)
    return [[1, 0, 0], [0, math.cos(a), -math.sin(a)], [0, math.sin(a), math.cos(a)]]


def keycap_matrix():
    """The keycap STEPs were placed by hardware/case/gen_case.py for 7 degrees (rotate about X, then lift by 3 + 3 cos + 61.3 sin).
    Undo that and place them on the board at TILT instead."""
    t7 = math.radians(7.0)
    lg7 = 3.0 + 3.0 * math.cos(t7) + (121.1 / 2 + 0.75) * math.sin(t7)
    R7T = [list(r) for r in zip(*_rx(7.0))]
    R4 = _rx(TILT)
    R = [[sum(R4[i][k] * R7T[k][j] for k in range(3)) for j in range(3)] for i in range(3)]
    # p'' = R4 R7^T (p' - (0, 0, lg7)) + (0, 0, LIFT)
    t = [-R[i][2] * lg7 + (LIFT if i == 2 else 0.0) for i in range(3)]
    m = adsk.core.Matrix3D.create()
    m.setWithArray([R[0][0], R[0][1], R[0][2], t[0] / 10, R[1][0], R[1][1], R[1][2], t[1] / 10, R[2][0], R[2][1], R[2][2], t[2] / 10, 0, 0, 0, 1])
    return m


def place_all():
    app = adsk.core.Application.get()
    design = adsk.fusion.Design.cast(app.activeProduct)
    root = design.rootComponent
    im = app.importManager

    def imp(fn, base=STEP):
        o = im.createSTEPImportOptions(base + fn)
        o.isViewFit = False
        im.importToTarget(o, root)
        return root.occurrences.item(root.occurrences.count - 1)

    asm = imp('Dachshund70-Assembly.step')
    asm.transform2 = local_matrix(0, 0, 0)                    # mainboard + plate, in the board frame
    enc = imp('Dachshund70-Encoder.step')
    enc.transform2 = local_matrix(ENC_X0 - 99.98, 60.55 + 79.98 + ENC_DY, 1.8)      # u = 0 at x 126.25, v = 0 at the back edge, PCB bottom 1.8
    usb = imp('Dachshund70-USB.step')
    usb.transform2 = world_matrix(-121.0, USB_FACE_Y + 98.68, USB_Z)              # flat on its ledge, receptacle face 1.0 inside the back face
    ns = {}
    exec(compile(open(HERE + 'make_models.py').read(), 'make_models.py', 'exec'), ns)
    mb = ns['battery'](design, root)
    ms = ns['screen'](design, root)
    mb.occ.transform2 = world_matrix_rz(BATT_CX, BATT_CY, BATT_Z0)
    ms.occ.transform2 = local_matrix(144.5, 3.1, 11.5)
    return asm, enc, usb, mb.occ, ms.occ
