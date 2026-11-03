"""Re-export everything in hardware/case/ from the open "Dachshund70 case rev2" design.

Run it in Fusion (Utilities > Scripts, or through the Fusion MCP), after editing REPO. It writes:

    step/Dachshund70-Case-<Part>.step       one STEP per part, at its assembly position (Fusion's world frame)
    step/Dachshund70-Case-Assembly.step     everything visible: boards, plate, battery, screen, keycaps, case
    step/Dachshund70-Keycaps.step           the keycaps with the split bottom row (hidden in the assembly)
    step/Dachshund70-Keycaps-6.25u.step     the keycaps with the 6.25 u spacebar (what the assembly shows)
    fusion/Dachshund70-Case-rev2.f3d        the native archive
    dxf/*.dxf                               from export_dxf.py (run that separately)

Afterwards, replace 'Dachshund60F' by 'Dachshund70' in the STEP text if any imported part still carries the
old name (the assembly and keycap files do when the model was built from older board files).
Parts leave Fusion through a new, direct-modelling document: Fusion won't export a single body to STEP, so
each body is copied into a scratch document, exported, and the scratch document is closed unsaved.
"""
import adsk.core, adsk.fusion, os
REPO = 'C:/path/to/Dachshund70/'   # EDIT: the repo root (with a trailing slash)
CASE = REPO + 'hardware/case/'
PARTS = [('Top', 'Top shell'), ('Bottom', 'Bottom shell'), ('Ring', 'Accent ring'), ('Weight', 'HW Accent weight bar'),
         ('Encoder-Cover', 'Encoder cover'), ('Knob', 'HW Wheel'), ('Switch-Cap', 'HW Power slider cap'),
         ('LED-Guide', 'HW LED guide')]


def parts(app, root):
    tbm = adsk.fusion.TemporaryBRepManager.get()
    copies = [(fn, tbm.copy(root.bRepBodies.itemByName(bn))) for fn, bn in PARTS]   # world-space copies
    orig = app.activeDocument
    nd = app.documents.add(adsk.core.DocumentTypes.FusionDesignDocumentType)
    ndd = adsk.fusion.Design.cast(nd.products.itemByProductType('DesignProductType'))
    ndd.designType = adsk.fusion.DesignTypes.DirectDesignType
    for fn, cp in copies:
        b = ndd.rootComponent.bRepBodies.add(cp)
        b.name = 'Dachshund70-Case-' + fn
        path = CASE + 'step/Dachshund70-Case-' + fn + '.step'
        ndd.exportManager.execute(ndd.exportManager.createSTEPExportOptions(path))
        b.deleteMe()
    nd.close(False)
    orig.activate()


def keycaps(root, em):
    """Show only the 76 common caps plus one spacebar variant, export, and put every visibility back."""
    saved = []

    def setv(o, v):
        saved.append((o, o.isLightBulbOn))
        o.isLightBulbOn = v
    sp_full = root.occurrences.itemByName('Spacebar 6.25u:1')
    sp_split = root.occurrences.itemByName('Spacebar split:1')
    try:
        for b in root.bRepBodies:
            setv(b, False)
        for o in root.occurrences:
            setv(o, o.name in ('Keycaps:1', 'Spacebar 6.25u:1', 'Spacebar split:1'))
        for c in sp_full.childOccurrences:
            setv(c, c.name == 'Space (R4, 6.25u):1')
        for c in sp_split.childOccurrences:
            setv(c, False)
        sp_split.isLightBulbOn = False
        em.execute(em.createSTEPExportOptions(CASE + 'step/Dachshund70-Keycaps-6.25u.step'))
        sp_full.isLightBulbOn = False
        sp_split.isLightBulbOn = True
        for c in sp_split.childOccurrences:
            c.isLightBulbOn = (c.name == 'Keycaps (1):1')    # its 76 common caps are already hidden; three split caps show
        em.execute(em.createSTEPExportOptions(CASE + 'step/Dachshund70-Keycaps.step'))
    finally:
        for o, v in reversed(saved):
            o.isLightBulbOn = v
        for c in sp_split.childOccurrences:
            c.isLightBulbOn = True
        sp_split.isLightBulbOn = False


def run(_context: str):
    app = adsk.core.Application.get()
    design = adsk.fusion.Design.cast(app.activeProduct)
    root, em = design.rootComponent, design.exportManager
    os.makedirs(CASE + 'fusion', exist_ok=True)
    parts(app, root)
    keycaps(root, em)
    em.execute(em.createSTEPExportOptions(CASE + 'step/Dachshund70-Case-Assembly.step'))
    em.execute(em.createFusionArchiveExportOptions(CASE + 'fusion/Dachshund70-Case-rev2.f3d'))
    print('exported')
