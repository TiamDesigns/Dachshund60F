"""Export fab files for every board, ready to upload to JLCPCB.

    python hardware/export_fab.py [out_dir]

Writes to out_dir (default hardware/fab/):

- <board>-gerbers.zip  for the mainboard, USB-C daughterboard, dongle and plate
- <board>-bom.csv and <board>-cpl.csv  for JLCPCB assembly of the three PCBs

Each zip holds the copper, mask, paste, silkscreen and outline Gerbers plus
Excellon drill files (plated and non-plated holes separate, mm, with drill
maps). Zones are plotted as saved, so refill them in KiCad (B) and save before
exporting after any board change.

The BOM takes every part with an LCSC or MPN field (the MPN alone means JLC has
to source it globally). Parts with neither aren't assembled: the key switches,
stabilizers, display header J4 and test pads. The hot-swap sockets are on the
switch footprints, which sit on the front with their pads on the back, so the
CPL moves them to the bottom layer at the socket's own centre.

Set KICAD_CLI if kicad-cli isn't at the default Windows install path.
"""

import csv
import os
import subprocess
import sys
import tempfile
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
KICAD_CLI = os.environ.get("KICAD_CLI", r"C:\Program Files\KiCad\10.0\bin\kicad-cli.exe")
BOARDS = ["Dachshund60F", "Dachshund60F-USB", "Dachshund60F-Dongle", "Dachshund60F-Plate"]
ASSEMBLED = ["Dachshund60F", "Dachshund60F-USB", "Dachshund60F-Dongle"]
# Kailh socket centre relative to the switch footprint's origin (midpoint of its two pads), KiCad axes.
SOCKET_FOOTPRINT = "SW_Hotswap_Kailh_MX"
SOCKET_OFFSET = (-0.6215, -3.81)
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


def assembly(name, out_dir):
    sch = os.path.join(HERE, name, f"{name}.kicad_sch")
    board = os.path.join(HERE, name, f"{name}.kicad_pcb")
    with tempfile.TemporaryDirectory() as tmp:
        bom_raw = os.path.join(tmp, "bom.csv")
        pos_raw = os.path.join(tmp, "pos.csv")
        run("sch", "export", "bom", "--fields", "Reference,Value,Footprint,LCSC,MPN",
            "--labels", "Reference,Value,Footprint,LCSC,MPN", "--group-by", "LCSC,MPN,Footprint",
            "--ref-range-delimiter", "", "-o", bom_raw, sch)
        run("pcb", "export", "pos", "--format", "csv", "--units", "mm", "--side", "both", "-o", pos_raw, board)
        bom = list(csv.DictReader(open(bom_raw, encoding="utf-8")))
        pos = {r["Ref"]: r for r in csv.DictReader(open(pos_raw, encoding="utf-8"))}

    placed, skipped = [], []
    with open(os.path.join(out_dir, f"{name}-bom.csv"), "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["Comment", "Designator", "Footprint", "LCSC"])
        for r in bom:
            refs = r["Reference"].split(",")
            if not (r["LCSC"] or r["MPN"]):
                skipped += refs
                continue
            w.writerow([r["MPN"] or r["Value"], ",".join(refs), r["Footprint"].split(":")[-1], r["LCSC"]])
            placed += refs

    with open(os.path.join(out_dir, f"{name}-cpl.csv"), "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["Designator", "Mid X", "Mid Y", "Layer", "Rotation"])
        for ref in placed:
            p = pos[ref]
            x, y, rot, side = float(p["PosX"]), float(p["PosY"]), float(p["Rot"]), p["Side"]
            if SOCKET_FOOTPRINT in p["Package"]:
                if rot % 360:
                    raise SystemExit(f"{ref}: rotated socket ({rot}); add the rotated offset")
                # The pos file's Y axis points up, KiCad's down.
                x, y, side = x + SOCKET_OFFSET[0], y - SOCKET_OFFSET[1], "bottom"
            w.writerow([ref, f"{x:.4f}", f"{y:.4f}", "Bottom" if side == "bottom" else "Top", f"{rot % 360:g}"])
    print(f"{name}: BOM and CPL for {len(placed)} parts; not assembled: {', '.join(skipped) or 'none'}")


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "fab")
    os.makedirs(out_dir, exist_ok=True)
    for name in BOARDS:
        export(name, out_dir)
    for name in ASSEMBLED:
        assembly(name, out_dir)


if __name__ == "__main__":
    main()
