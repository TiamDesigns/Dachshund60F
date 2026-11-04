# python post.py <in.png> <out> [width]: downsample, sharpen, and add subtle lens effects
# (a touch of bloom, lateral chromatic aberration, vignette and grain). Needs Pillow and numpy.
import sys
import numpy as np
from PIL import Image, ImageFilter

src, dst = sys.argv[1], sys.argv[2]
width = int(sys.argv[3]) if len(sys.argv) > 3 else 2400
im = Image.open(src).convert("RGB")
im = im.resize((width, round(im.height * width / im.width)), Image.LANCZOS)
im = im.filter(ImageFilter.UnsharpMask(radius=1.1, percent=55, threshold=2))
a = np.asarray(im).astype(np.float32) / 255.0
h, w, _ = a.shape

# bloom: blurred highlights added back faintly
hi = np.clip((a - 0.82) / 0.18, 0, 1) * a
glow = np.asarray(Image.fromarray((hi * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(w / 110))).astype(np.float32) / 255
a = 1 - (1 - a) * (1 - glow * 0.35)

# lateral chromatic aberration: red scaled out, blue in, by a fraction of a pixel at the corners
def rescale(ch, s):
    img = Image.fromarray((ch * 255).astype(np.uint8))
    nw, nh = round(w * s), round(h * s)
    img = img.resize((nw, nh), Image.BICUBIC)
    l, t = (nw - w) // 2, (nh - h) // 2
    if s >= 1:
        img = img.crop((l, t, l + w, t + h))
    else:
        c = Image.new("L", (w, h)); c.paste(img, (-l, -t)); img = c
    return np.asarray(img).astype(np.float32) / 255
a[..., 0] = rescale(a[..., 0], 1.0014)
a[..., 1] = rescale(a[..., 1], 1.0007)  # blue stays put: all scales >= 1, so no empty border

# vignette
yy, xx = np.mgrid[0:h, 0:w]
r = np.sqrt(((xx - w / 2) / (w / 2)) ** 2 + ((yy - h / 2) / (h / 2)) ** 2) / np.sqrt(2)
a *= (1 - 0.16 * np.clip(r, 0, 1) ** 2.2)[..., None]

# fine monochrome grain
rng = np.random.default_rng(7)
a += rng.normal(0, 0.006, (h, w, 1)).astype(np.float32)
out = Image.fromarray((np.clip(a, 0, 1) * 255 + 0.5).astype(np.uint8))
out.save(dst, quality=93)
