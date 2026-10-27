"""Assembly STEP (and meshes for renders): the case parts plus the board STEPs from hardware/step/.

Used by gen_case.py. The boards are read with OCCT's XCAF reader, so their colours and part names
carry over, and are placed as components, not copied.
"""

import os

from OCP.BRepMesh import BRepMesh_IncrementalMesh
from OCP.gp import gp_Ax1, gp_Dir, gp_Pnt, gp_Trsf, gp_Vec
from OCP.IFSelect import IFSelect_RetDone
from OCP.Interface import Interface_Static
from OCP.Quantity import Quantity_Color, Quantity_TOC_RGB
from OCP.RWMesh import RWMesh_FaceIterator
from OCP.STEPCAFControl import STEPCAFControl_Reader, STEPCAFControl_Writer
from OCP.STEPControl import STEPControl_AsIs
from OCP.TCollection import TCollection_ExtendedString
from OCP.TDataStd import TDataStd_Name
from OCP.TDF import TDF_LabelSequence
from OCP.TDocStd import TDocStd_Document
from OCP.TopLoc import TopLoc_Location
from OCP.XCAFApp import XCAFApp_Application
from OCP.XCAFDoc import XCAFDoc_ColorSurf, XCAFDoc_DocumentTool
from OCP.XCAFPrs import XCAFPrs_DocumentExplorer, XCAFPrs_DocumentExplorerFlags_OnlyLeafNodes

import gen_case as g


def _new_doc():
    app = XCAFApp_Application.GetApplication_s()
    doc = TDocStd_Document(TCollection_ExtendedString("XmlOcaf"))
    app.InitDocument(doc)
    return doc


def _read_into(doc, path):
    """Read a STEP file into doc; return the labels of the shapes it added at top level."""
    shapes = XCAFDoc_DocumentTool.ShapeTool_s(doc.Main())
    before = TDF_LabelSequence()
    shapes.GetFreeShapes(before)
    known = {before.Value(i + 1).Tag() for i in range(before.Length())}
    r = STEPCAFControl_Reader()
    r.SetColorMode(True)
    r.SetNameMode(True)
    if r.ReadFile(path) != IFSelect_RetDone:
        raise SystemExit(f"can't read {path}")
    r.Transfer(doc)
    after = TDF_LabelSequence()
    shapes.GetFreeShapes(after)
    return [after.Value(i + 1) for i in range(after.Length()) if after.Value(i + 1).Tag() not in known]


def _tilt():
    rot = gp_Trsf()
    rot.SetRotation(gp_Ax1(gp_Pnt(0, 0, 0), gp_Dir(1, 0, 0)), g.math.radians(g.TILT))
    lift = gp_Trsf()
    lift.SetTranslation(gp_Vec(0, 0, g.LIFT))
    return TopLoc_Location(lift.Multiplied(rot))


def _usb_placement():
    """The daughterboard STEP has KiCad's origin with Y flipped; put its connector in the back wall."""
    t = gp_Trsf()
    x, y = g.usb_world(0, 0)
    t.SetTranslation(gp_Vec(x, y, g.USB_Z))
    return TopLoc_Location(t)


def _offset(dz):
    t = gp_Trsf()
    t.SetTranslation(gp_Vec(0, 0, dz))
    return TopLoc_Location(t)


def build(parts, colors, explode=None, skip=()):
    """XCAF doc of the whole assembly. explode maps part names (and "Board", "USB") to Z offsets;
    skip leaves those out."""
    explode = explode or {}
    doc = _new_doc()
    shapes = XCAFDoc_DocumentTool.ShapeTool_s(doc.Main())
    color_tool = XCAFDoc_DocumentTool.ColorTool_s(doc.Main())

    board = [] if "Board" in skip else _read_into(doc, g.BOARD_STEP)
    usb = _read_into(doc, g.USB_STEP)

    top = shapes.NewShape()
    TDataStd_Name.Set_s(top, TCollection_ExtendedString("Dachshund60F case assembly"))
    for lbl in board:
        comp = shapes.AddComponent(top, lbl, _offset(explode.get("Board", 0)).Multiplied(_tilt()))
        TDataStd_Name.Set_s(comp, TCollection_ExtendedString("Mainboard and plate"))
    for lbl in usb:
        comp = shapes.AddComponent(top, lbl, _offset(explode.get("USB", 0)).Multiplied(_usb_placement()))
        TDataStd_Name.Set_s(comp, TCollection_ExtendedString("USB-C daughterboard"))
    for name, shape in parts.items():
        if name in skip:
            continue
        lbl = shapes.AddShape(shape.wrapped, False)
        TDataStd_Name.Set_s(lbl, TCollection_ExtendedString(name))
        r, gr, b = colors[name]
        color_tool.SetColor(lbl, Quantity_Color(r, gr, b, Quantity_TOC_RGB), XCAFDoc_ColorSurf)
        comp = shapes.AddComponent(top, lbl, _offset(explode.get(name, 0)))
        TDataStd_Name.Set_s(comp, TCollection_ExtendedString(name))
    shapes.UpdateAssemblies()
    return doc


def write(parts, colors, step_path):
    doc = build(parts, colors)

    # Leave out the 2D curves on each face: CAD tools rebuild them, and they nearly triple the size.
    Interface_Static.SetIVal_s("write.surfacecurve.mode", 0)
    w = STEPCAFControl_Writer()
    w.SetColorMode(True)
    w.SetNameMode(True)
    w.Transfer(doc, STEPControl_AsIs)
    w.Write(step_path)
    print(f"{os.path.relpath(step_path, g.HW)}: {os.path.getsize(step_path) / 1e6:.1f} MB")
    return doc


def write_meshes(doc, path, deflection=0.08):
    """Mesh every shape in doc and save the triangles grouped by colour to an .npz (for render.py)."""
    import numpy as np

    shapes = XCAFDoc_DocumentTool.ShapeTool_s(doc.Main())
    free = TDF_LabelSequence()
    shapes.GetFreeShapes(free)
    for i in range(free.Length()):
        BRepMesh_IncrementalMesh(shapes.GetShape_s(free.Value(i + 1)), deflection, False, 0.3, True)
    groups = {}
    ex = XCAFPrs_DocumentExplorer(doc, XCAFPrs_DocumentExplorerFlags_OnlyLeafNodes)
    while ex.More():
        node = ex.Current()
        it = RWMesh_FaceIterator(node.RefLabel, node.Location, True, node.Style)
        while it.More():
            if not it.IsEmptyMesh():
                rgb = it.FaceColor().GetRGB() if it.HasFaceColor() else None
                key = "%.3f_%.3f_%.3f" % ((rgb.Red(), rgb.Green(), rgb.Blue()) if rgb else (0.6, 0.6, 0.6))
                lo = it.NodeLower()
                pts = np.array([it.NodeTransformed(k).Coord() for k in range(lo, it.NodeUpper() + 1)])
                tris = np.array([it.TriangleOriented(k).Get() for k in range(it.ElemLower(), it.ElemUpper() + 1)])
                grp = groups.setdefault(key, [[], [], 0])
                grp[0].append(pts)
                grp[1].append(tris - lo + grp[2])
                grp[2] += len(pts)
            it.Next()
        ex.Next()
    arrays = {}
    for key, (pts, tris, _) in groups.items():
        arrays[key + "_p"] = np.vstack(pts).astype(np.float32)
        arrays[key + "_t"] = np.vstack(tris).astype(np.int32)
    np.savez_compressed(path, **arrays)
