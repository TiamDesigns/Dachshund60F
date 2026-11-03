import adsk.core, adsk.fusion, os
REPO = 'C:/path/to/Dachshund70/'   # EDIT: the repo root (with a trailing slash)
HERE = REPO + 'hardware/case/fusion/scripts/'
SC = HERE + 'shots/'
os.makedirs(SC, exist_ok=True)


def shot(name, eye, target, up=(0, 0, 1), ext=None, w=1600, h=900, persp=False):
    app = adsk.core.Application.get()
    vp = app.activeViewport
    cam = vp.camera
    cam.isSmoothTransition = False
    cam.cameraType = adsk.core.CameraTypes.PerspectiveCameraType if persp else adsk.core.CameraTypes.OrthographicCameraType
    cam.eye = adsk.core.Point3D.create(*[v / 10 for v in eye])
    cam.target = adsk.core.Point3D.create(*[v / 10 for v in target])
    cam.upVector = adsk.core.Vector3D.create(*up)
    vp.camera = cam
    if ext:
        cam = vp.camera
        cam.viewExtents = ext / 10
        vp.camera = cam
    import time
    for _ in range(6):
        adsk.doEvents(); time.sleep(0.4)
    vp.refresh()
    vp.saveAsImageFile(SC + name + '.png', w, h)
    print('saved', name)
