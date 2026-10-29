"""Render PNG previews of the case assembly from the meshes gen_case.py --renders writes.

    python hardware/case/render.py <assembled.npz> <exploded.npz> <inside.npz> <out_dir>

Uses VTK (installed with CadQuery). Off-screen rendering needs EGL or OSMesa on Linux
(for example `apt install libegl1`).
"""

import os
import sys

import numpy as np
import vtk
from vtk.util.numpy_support import numpy_to_vtk, numpy_to_vtkIdTypeArray

BG_TOP, BG_BOTTOM = (0.99, 0.97, 0.93), (0.84, 0.79, 0.72)
SIZE = (1800, 1100)
SUPERSAMPLE = 2  # render this many times larger and average down (FXAA renders black on EGL)


def scene(npz):
    ren = vtk.vtkRenderer()
    win = vtk.vtkRenderWindow()
    win.SetOffScreenRendering(1)
    win.AddRenderer(ren)
    win.SetSize(SIZE[0] * SUPERSAMPLE, SIZE[1] * SUPERSAMPLE)
    ren.GradientBackgroundOn()
    ren.SetBackground(*BG_BOTTOM)
    ren.SetBackground2(*BG_TOP)
    ren.RemoveAllLights()
    for pos, intensity in (((-0.6, -1.0, 1.4), 0.7), ((1.2, 0.4, 0.8), 0.35), ((0.2, 1.0, -0.6), 0.25)):
        light = vtk.vtkLight()
        light.SetLightTypeToCameraLight()
        light.SetPosition(*pos)
        light.SetFocalPoint(0, 0, 0)
        light.SetIntensity(intensity)
        ren.AddLight(light)
    data = np.load(npz)
    for key in sorted({k[:-2] for k in data.files}):
        p, tri = data[key + "_p"], data[key + "_t"].astype(np.int64)
        pts = vtk.vtkPoints()
        pts.SetData(numpy_to_vtk(p, deep=True))
        conn = numpy_to_vtkIdTypeArray(tri.ravel(), deep=True)
        offsets = numpy_to_vtkIdTypeArray(np.arange(0, 3 * len(tri) + 1, 3, dtype=np.int64), deep=True)
        cells = vtk.vtkCellArray()
        cells.SetData(offsets, conn)
        poly = vtk.vtkPolyData()
        poly.SetPoints(pts)
        poly.SetPolys(cells)
        normals = vtk.vtkPolyDataNormals()
        normals.SetInputData(poly)
        normals.SetFeatureAngle(35)
        normals.SplittingOn()
        normals.ConsistencyOff()
        normals.AutoOrientNormalsOff()
        mapper = vtk.vtkPolyDataMapper()
        mapper.SetInputConnection(normals.GetOutputPort())
        actor = vtk.vtkActor()
        actor.SetMapper(mapper)
        prop = actor.GetProperty()
        prop.SetColor(*map(float, key.split("_")))
        prop.SetInterpolationToPhong()
        prop.SetAmbient(0.22)
        prop.SetDiffuse(0.85)
        prop.SetSpecular(0.2)
        prop.SetSpecularPower(25)
        prop.BackfaceCullingOff()
        ren.AddActor(actor)
    return ren, win


def shoot(ren, win, out, pos, focal, zoom=1.0, parallel=False, clip=None, up=(0, 0, 1)):
    actors = ren.GetActors()
    actors.InitTraversal()
    for _ in range(actors.GetNumberOfItems()):
        mapper = actors.GetNextActor().GetMapper()
        mapper.RemoveAllClippingPlanes()
        if clip:
            plane = vtk.vtkPlane()
            plane.SetOrigin(*clip[0])
            plane.SetNormal(*clip[1])
            mapper.AddClippingPlane(plane)
    cam = ren.GetActiveCamera()
    cam.SetFocalPoint(*focal)
    cam.SetPosition(*pos)
    cam.SetViewUp(*up)
    cam.SetParallelProjection(parallel)
    if parallel:
        cam.SetParallelScale(200 / zoom)
    else:
        cam.SetViewAngle(30 / zoom)
    ren.ResetCameraClippingRange()
    win.Render()
    grab = vtk.vtkWindowToImageFilter()
    grab.SetInput(win)
    shrink = vtk.vtkImageShrink3D()
    shrink.SetInputConnection(grab.GetOutputPort())
    shrink.SetShrinkFactors(SUPERSAMPLE, SUPERSAMPLE, 1)
    shrink.AveragingOn()
    png = vtk.vtkPNGWriter()
    png.SetFileName(out)
    png.SetInputConnection(shrink.GetOutputPort())
    png.Write()
    print(out)


VIEWS = [
    # name, camera position, focal point, zoom, parallel, clip plane (origin, normal)[, view up]
    ("01-front-left", (-300, -560, 330), (0, 0, 10), 1.3, False, None),
    ("02-back-right", (330, 560, 260), (20, 0, 14), 1.3, False, None),
    ("03-side-profile", (-1000, 0, 16), (0, 0, 16), 2.6, True, None),
    ("04-bottom", (-150, -330, -520), (0, 0, 0), 1.3, False, None),
    ("05-sidebar-and-roller", (380, -230, 260), (140, 20, 22), 3.0, False, None),
    ("06-usb-c", (100, 400, 60), (0, 72, 14), 4.5, False, None),
    ("07-section-gasket", (-1000, 0, 16), (39.81, 0, 16), 2.6, True, ((39.81, 0, 0), (1, 0, 0))),
    ("08-section-front", (-1000, 0, 10), (39.81, -64, 8), 9.0, True, ((39.81, 0, 0), (1, 0, 0))),
    ("09-section-back", (-1000, 0, 24), (39.81, 64, 22), 9.0, True, ((39.81, 0, 0), (1, 0, 0))),
    ("12-sidebar-top", (140, 0, 1000), (140, 0, 30), 2.9, True, None, (0, 1, 0)),
    ("13-section-sidebar", (-1000, 0, 25), (150, 10, 22), 4.5, True, ((150, 0, 0), (1, 0, 0))),
    ("14-section-roller", (-1000, 0, 25), (151.8, 30, 22), 6.0, True, ((151.8, 0, 0), (1, 0, 0))),
    ("15-section-leds", (143, -1000, 22), (143, -32, 20), 6.0, True, ((0, -31.9, 0), (0, 1, 0))),
    ("16-section-encoder", (145, -1000, 26), (145, 48.6, 24), 5.0, True, ((0, 48.64, 0), (0, 1, 0))),
    ("17-power-switch", (520, -160, 120), (166, -12, 11), 6.0, False, None),
    ("18-section-switch", (164, -1000, 11), (164, -11.7, 11), 9.0, True, ((0, -11.67, 0), (0, 1, 0))),
    ("19-roller-end", (560, 230, 150), (166, 50, 20), 5.0, False, None),
    ("20-right-side", (1000, 0, 16), (168, 0, 14), 2.4, True, None),
    ("21-thumb-scoop", (40, -330, 120), (84, -62, 14), 4.0, False, None),
    ("22-bottom-plan", (0, -1.6, -1000), (0, -1.6, 0), 1.25, True, None, (0, -1, 0)),
    ("23-weight", (0, -1.6, -1000), (0, -1.6, 0), 3.4, True, None, (0, -1, 0)),
    ("10-inside", (-240, -420, 420), (0, 10, 5), 1.4, False, None),
]


def render(assembled, exploded, inside, out):
    os.makedirs(out, exist_ok=True)
    ren, win = scene(assembled)
    for name, *view in VIEWS[:-1]:
        shoot(ren, win, os.path.join(out, name + ".png"), *view)
    ren, win = scene(inside)
    name, *view = VIEWS[-1]
    shoot(ren, win, os.path.join(out, name + ".png"), *view)
    ren, win = scene(exploded)
    shoot(ren, win, os.path.join(out, "11-exploded.png"), (-330, -620, 420), (0, 0, 35), 1.1)


if __name__ == "__main__":
    render(*sys.argv[1:5])
