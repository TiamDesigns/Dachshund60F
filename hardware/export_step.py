"""Export STEP models of every board for case design (Fusion 360 and so on).

    python hardware/export_step.py

Writes to hardware/step/ (committed; rerun after any board change and commit
the result when you want a snapshot):

- Dachshund60F.step           mainboard with all parts
- Dachshund60F-Plate.step     switch plate
- Dachshund60F-Assembly.step  mainboard and plate, already stacked
- Dachshund60F-USB.step       USB-C daughterboard
- Dachshund60F-Dongle.step    2.4 GHz dongle

The mainboard, plate and assembly share one origin: the centre of the
mainboard outline, so the separate files line up in X and Y. In every file the
board's bottom face is at Z = 0 and its top at Z = 1.6 mm. Stacked as built,
the plate's bottom face sits 5.0 mm above Z = 0 (3.4 mm above the PCB's top),
which puts the plate's top 5.0 mm above the PCB, the MX switch spec.

Set KICAD_CLI if kicad-cli isn't at the default Windows install path.
"""

import os
import re
import subprocess
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "step")
KICAD_CLI = os.environ.get("KICAD_CLI", r"C:\Program Files\KiCad\10.0\bin\kicad-cli.exe")

MAINBOARD = os.path.join(HERE, "Dachshund60F", "Dachshund60F.kicad_pcb")
PLATE = os.path.join(HERE, "Dachshund60F-Plate", "Dachshund60F-Plate.kicad_pcb")

# Centre of the mainboard outline (19..337.5 x 39..160.1 mm).
MAIN_ORIGIN = "178.25x99.55mm"
# Gap between the PCB's top face and the plate's bottom face.
PLATE_LIFT = 3.4


def export(board, out, *extra):
    cmd = [KICAD_CLI, "pcb", "export", "step", "--subst-models", "--force", "-o", out, *extra, board]
    run = subprocess.run(cmd, capture_output=True, text=True)
    missing = sorted(set(re.findall(r"Could not add 3D model for (\S+)\.", run.stdout + run.stderr)))
    if run.returncode or not os.path.exists(out):
        raise SystemExit(f"export failed for {board}:\n{run.stdout}{run.stderr}")
    size = os.path.getsize(out) / 1e6
    note = f"  (no 3D model: {', '.join(missing)})" if missing else ""
    print(f"{os.path.relpath(out, HERE)}: {size:.1f} MB{note}")


def assembly(plate_step, out):
    """Mainboard plus the plate, attached as a 3D model on a dummy footprint."""
    s = open(MAINBOARD, encoding="utf-8").read()
    # The copy lives in a temp folder, so resolve project-relative model paths now.
    s = s.replace("${KIPRJMOD}", os.path.dirname(MAINBOARD).replace("\\", "/"))
    plate = plate_step.replace("\\", "/")
    fp = f"""
\t(footprint "Assembly:Plate"
\t\t(layer "F.Cu")
\t\t(at 0 0)
\t\t(property "Reference" "PLATE1" (at 0 0 0) (layer "F.Fab") (hide yes)
\t\t\t(effects (font (size 1 1) (thickness 0.15))))
\t\t(property "Value" "Plate" (at 0 0 0) (layer "F.Fab") (hide yes)
\t\t\t(effects (font (size 1 1) (thickness 0.15))))
\t\t(attr exclude_from_pos_files exclude_from_bom)
\t\t(model "{plate}"
\t\t\t(offset (xyz 0 0 {PLATE_LIFT}))
\t\t\t(scale (xyz 1 1 1))
\t\t\t(rotate (xyz 0 0 0)))
\t)"""
    i = s.index("\n\t(footprint ")
    s = s[:i] + fp + s[i:]
    with tempfile.TemporaryDirectory() as tmp:
        board = os.path.join(tmp, "Dachshund60F-Assembly.kicad_pcb")
        open(board, "w", encoding="utf-8").write(s)
        export(board, out, "--user-origin", MAIN_ORIGIN)


def main():
    os.makedirs(OUT, exist_ok=True)
    export(MAINBOARD, os.path.join(OUT, "Dachshund60F.step"), "--user-origin", MAIN_ORIGIN)
    plate = os.path.join(OUT, "Dachshund60F-Plate.step")
    export(PLATE, plate, "--user-origin", MAIN_ORIGIN)
    # The assembly places the plate by KiCad coordinates, so it needs a copy
    # exported with KiCad's own origin.
    with tempfile.TemporaryDirectory() as tmp:
        plate_abs = os.path.join(tmp, "plate.step")
        subprocess.run([KICAD_CLI, "pcb", "export", "step", "--force", "--user-origin", "0x0mm",
                        "-o", plate_abs, PLATE], capture_output=True, check=True)
        assembly(plate_abs, os.path.join(OUT, "Dachshund60F-Assembly.step"))
    for name in ("Dachshund60F-USB", "Dachshund60F-Dongle"):
        export(os.path.join(HERE, name, f"{name}.kicad_pcb"), os.path.join(OUT, f"{name}.step"))


if __name__ == "__main__":
    main()
