# Blender renders (case rev 2)

Preview renders of the full assembly (`../step/Dachshund70-Case-Assembly.step`), made in Blender 5.2 with Cycles.
They show the preliminary look; they are not a spec. The Fusion model is still the source of truth.

| File | What |
| --- | --- |
| `renders/01-dramatic.jpg` | Low warm sun through window blinds: wool felt mat, oak desk standing off a plaster wall, plant, framed print, mug, linen notebook and a loose keycap, shallow depth of field |
| `renders/02-views.jpg` | Hero view plus back, right end, knob close-up, plan, underside and the brass weight, on deep green paper |

Finishes: warm light-grey matte e-coat case, GMK Kaiju-style keycaps (cream alphas, warm grey mods, red-orange
Esc and Enter), anodised ring, knob and encoder cover (orange `#FF9A66`, or green `#2F6B3A` with `ACCENT=green`),
polished brass weight with three fingerprints, stainless screws, white rubber rails, frosted polycarbonate plate,
and a few specks of dust. The e-ink panel shows `eink.png` (its picture area `eink_pic.png`).

## Re-rendering

Blender can't read STEP, so FreeCAD tessellates the assembly first (one STL per part plus `manifest.json`;
hotswap sockets and SMD parts are skipped, switches are meshed coarsely). Keep the work folder out of the repo.
The frames render at 1.5× and `post.py` downsamples, sharpens and adds the lens effects (slight bloom, chromatic
aberration, vignette and grain).

```bash
SRC="$PWD/hardware/case/step/Dachshund70-Case-Assembly.step" OUT=/path/to/work/mesh freecadcmd hardware/case/blender/fc_mesh.py
MESH=/path/to/work/mesh OUT=/path/to/work ACCENT=orange blender -b --factory-startup --python hardware/case/blender/build.py
blender -b /path/to/work/base_orange.blend --python hardware/case/blender/render.py -- dramatic /path/to/work/out 512
blender -b /path/to/work/base_orange.blend --python hardware/case/blender/render.py -- views /path/to/work/out 320
python hardware/case/blender/post.py /path/to/work/out/01-dramatic.png hardware/case/blender/renders/01-dramatic.jpg 3600
for n in hero back right-end knob top underside weight; do
  python hardware/case/blender/post.py /path/to/work/out/views-$n.png /path/to/work/post/views-$n.png $([ $n = hero ] && echo 3600 || echo 2400)
done
python hardware/case/blender/montage.py /path/to/work/post hardware/case/blender/renders/02-views.jpg 3600
```

`GOBO_X` (default `-0.12`) slides the window shadow sideways in the dramatic scene. The case parts in the
assembly STEP are unnamed compounds; `build.py` maps them by name (`COMPOUND` = bottom, `001` = top, `002` = ring,
`003` = encoder cover, `004` = knob, `005` = LED guide, `006` = screen frame, `007` = switch cap, `008` = weight,
`013`/`014` = rails, the rest are screws). If the Fusion export order changes, update that map.
