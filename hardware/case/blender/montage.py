# python montage.py <frames dir> <out.jpg>: hero on top, six labelled views below (needs Pillow)
import sys
from PIL import Image, ImageDraw, ImageFont

F, OUT = sys.argv[1].rstrip("/\\") + "/", sys.argv[2]
G = 24
hero = Image.open(F + "views-hero.png").convert("RGB").resize((2400, 1600))
names = [("back", "Back: USB-C and power switch"), ("right-end", "Right end: knob and finger scoop"),
         ("knob", "Knob, screen and status LEDs"), ("top", "Plan"),
         ("underside", "Underside: rails and weight window"), ("weight", "Brass weight engraving")]
tw = (2400 - 4 * G) // 3
th = int(tw * 1067 / 1600)
W = Image.new("RGB", (2400, 1600 + G + 2 * (th + G)), (236, 233, 226))
W.paste(hero, (0, 0))
try:
    font = ImageFont.truetype("segoeui.ttf", 26)
except OSError:
    font = ImageFont.load_default()
d = ImageDraw.Draw(W)
for i, (n, lab) in enumerate(names):
    im = Image.open(F + f"views-{n}.png").convert("RGB").resize((tw, th), Image.LANCZOS)
    x = G + (i % 3) * (tw + G)
    y = 1600 + G + (i // 3) * (th + G)
    W.paste(im, (x, y))
    d.rounded_rectangle((x + 14, y + th - 54, x + 28 + d.textlength(lab, font=font), y + th - 14), 8, fill=(255, 255, 255))
    d.text((x + 21, y + th - 50), lab, fill=(60, 58, 54), font=font)
W.save(OUT, quality=92)
