"""Export fab files (Gerbers and drill files) for every board, ready to upload to JLCPCB.

    python hardware/export_fab.py [out_dir]

Writes one zip per board to out_dir (default hardware/fab/):

- Dachshund60F-gerbers.zip         mainboard
- Dachshund60F-USB-gerbers.zip     USB-C daughterboard
- Dachshund60F-Dongle-gerbers.zip  2.4 GHz dongle
- Dachshund60F-Plate-gerbers.zip   FR4 switch plate

Each zip holds the copper, mask, paste, silkscreen and outline Gerbers plus
Excellon drill files (plated and non-plated holes separate, mm, with drill
maps). Zones are plotted as saved, so refill them in KiCad (B) and save before
exporting after any board change.

Set KICAD_CLI if kicad-cli isn't at the default Windows install path.
"""

import os
import subprocess
import sys
import tempfile
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
KICAD_CLI = os.environ.get("KICAD_CLI", r"C:\Program Files\KiCad\10.0\bin\kicad-cli.exe")
BOARDS = ["Dachshund60F", "Dachshund60F-USB", "Dachshund60F-Dongle", "Dachshund60F-Plate"]
LAYERS = "F.Cu,B.Cu,F.Paste,B.Paste,F.Silkscreen,B.Silkscreen,F.Mask,B.Mask,Edge.Cuts"


def run(*args):
    r = subprocess.run([KICAD_CLI, *args], capture_output=True, text=True)
    if r.returncode:
        raise SystemExit(f"kicad-cli {' '.join(args)} failed:\n{r.stdout}{r.stderr}")


def export(name, out_dir):
    board = os.path.join(HERE, name, f"{name}.kicad_pcb")
    with tempfile.TemporaryDirectory() as tmp:
        tmp = tmp + os.sep
        run("pcb", "export", "gerbers", "--layers", LAYERS, "--subtract-soldermask", "-o", tmp, board)
        run("pcb", "export", "drill", "--format", "excellon", "--excellon-units", "mm",
            "--excellon-separate-th", "--generate-map", "--map-format", "gerberx2", "-o", tmp, board)
        out = os.path.join(out_dir, f"{name}-gerbers.zip")
        with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
            for f in sorted(os.listdir(tmp)):
                z.write(os.path.join(tmp, f), f)
        print(f"{os.path.relpath(out)}: {len(z.namelist())} files")


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "fab")
    os.makedirs(out_dir, exist_ok=True)
    for name in BOARDS:
        export(name, out_dir)


if __name__ == "__main__":
    main()
