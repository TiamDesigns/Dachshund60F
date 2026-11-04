# python montage.py <frames dir> <out.jpg>: hero on top, six labelled views below (needs Pillow)
import sys
from PIL import Image, ImageDraw, ImageFont

F, OUT = sys.argv[1].rstrip("/\\") + "/", sys.argv[2]
WD = int(sys.argv[3]) if len(sys.argv) > 3 else 2400
k = WD / 2400
G = round(24 * k)
hero = Image.open(F + "views-hero.png").convert("RGB").resize((WD, round(1600 * k)))
names = [("back", "Back: USB-C and power switch"), ("right-end", "Right end: knob and finger scoop"),
         ("knob", "Knob, screen and status LEDs"), ("top", "Plan"),
         ("underside", "Underside: rails and weight window"), ("weight", "Brass weight engraving")]
tw = (WD - 4 * G) // 3
th = int(tw * 1067 / 1600)
HH = round(1600 * k)
W = Image.new("RGB", (WD, HH + G + 2 * (th + G)), (236, 233, 226))
W.paste(hero, (0, 0))
try:
    font = ImageFont.truetype("segoeui.ttf", round(26 * k))
except OSError:
    font = ImageFont.load_default()
d = ImageDraw.Draw(W)
for i, (n, lab) in enumerate(names):
    im = Image.open(F + f"views-{n}.png").convert("RGB").resize((tw, th), Image.LANCZOS)
    x = G + (i % 3) * (tw + G)
    y = HH + G + (i // 3) * (th + G)
    W.paste(im, (x, y))
    d.rounded_rectangle((x + 14 * k, y + th - 54 * k, x + 28 * k + d.textlength(lab, font=font), y + th - 14 * k), 8 * k, fill=(255, 255, 255))
    d.text((x + 21 * k, y + th - 50 * k), lab, fill=(60, 58, 54), font=font)
W.save(OUT, quality=92)
