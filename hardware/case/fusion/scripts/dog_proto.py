import math, sys
from PIL import Image, ImageDraw

def cmr(pts, per=10, closed=False, alpha=0.5):
    n_ = len(pts)
    if closed:
        q = [pts[-1]] + list(pts) + [pts[0], pts[1]]
        segs = range(1, n_ + 1)
    else:
        q = [(2 * pts[0][0] - pts[1][0], 2 * pts[0][1] - pts[1][1])] + list(pts) + [(2 * pts[-1][0] - pts[-2][0], 2 * pts[-1][1] - pts[-2][1])]
        segs = range(1, len(q) - 2)
    out = []
    for i_ in segs:
        p0, p1, p2, p3 = q[i_ - 1], q[i_], q[i_ + 1], q[i_ + 2]
        t0 = 0.0
        t1 = t0 + math.hypot(p1[0] - p0[0], p1[1] - p0[1]) ** alpha
        t2 = t1 + math.hypot(p2[0] - p1[0], p2[1] - p1[1]) ** alpha
        t3 = t2 + math.hypot(p3[0] - p2[0], p3[1] - p2[1]) ** alpha
        for k_ in range(per):
            t = t1 + (t2 - t1) * k_ / per
            a1 = [(t1 - t) / (t1 - t0) * p0[j] + (t - t0) / (t1 - t0) * p1[j] for j in (0, 1)]
            a2 = [(t2 - t) / (t2 - t1) * p1[j] + (t - t1) / (t2 - t1) * p2[j] for j in (0, 1)]
            a3 = [(t3 - t) / (t3 - t2) * p2[j] + (t - t2) / (t3 - t2) * p3[j] for j in (0, 1)]
            b1 = [(t2 - t) / (t2 - t0) * a1[j] + (t - t0) / (t2 - t0) * a2[j] for j in (0, 1)]
            b2 = [(t3 - t) / (t3 - t1) * a2[j] + (t - t1) / (t3 - t1) * a3[j] for j in (0, 1)]
            out.append(tuple((t2 - t) / (t2 - t1) * b1[j] + (t - t1) / (t2 - t1) * b2[j] for j in (0, 1)))
    if not closed:
        out.append(pts[-1])
    return out

OUT = eval(open(sys.argv[1]).read())
SC = 18
W, H = 60 * SC, 36 * SC
im = Image.new('RGB', (W, H), (30, 30, 30))
d = ImageDraw.Draw(im)
px = lambda p: (p[0] * SC + 20, H - (p[1] * SC + 20))
poly = [px(p) for p in cmr(OUT['outline'], 12, True)]
d.polygon(poly, fill=(150, 150, 150))
for nm, pts in OUT['lines'].items():
    d.line([px(p) for p in cmr(pts, 12)], fill=(30, 30, 30), width=3)
e = OUT['eye']
c = px(e[:2]); r = e[2] * SC
d.ellipse([c[0] - r, c[1] - r, c[0] + r, c[1] + r], fill=(30, 30, 30))
for p in OUT['outline']:
    q = px(p); d.ellipse([q[0] - 2, q[1] - 2, q[0] + 2, q[1] + 2], fill=(255, 80, 80))
im.save(sys.argv[2])
