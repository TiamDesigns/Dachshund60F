# Blender renders (case rev 2)

Preview renders of the full assembly (`../step/Dachshund70-Case-Assembly.step`), made in Blender 5.2 with Cycles.
They show the preliminary look; they are not a spec. The Fusion model is still the source of truth.

| File | What |
| --- | --- |
| `renders/01-dramatic.jpg` | Low warm sun through window blinds: felt desk mat, oak desk, plaster wall, plant, framed print, mug, notebook and a loose keycap |
| `renders/02-views.jpg` | Hero view plus back, right end, knob close-up, plan, underside and the brass weight, on sage paper |
| `renders/03-simple-orange.jpg` | Plan view on white, orange anodised ring, knob and encoder cover |
| `renders/03-simple-green.jpg` | The same with green anodising |

Finishes: warm light-grey matte e-coat case, GMK Kaiju-style keycaps (cream alphas, warm grey mods, red-orange
Esc and Enter), anodised ring, knob and encoder cover (orange `#FF9A66` or green `#2F6B3A`), polished brass weight, stainless
screws, frosted polycarbonate plate.

## Re-rendering

Blender can't read STEP, so FreeCAD tessellates the assembly first (one STL per part plus `manifest.json`;
hotswap sockets and SMD parts are skipped, switches are meshed coarsely). Keep the output out of the repo.

```bash
SRC="$PWD/hardware/case/step/Dachshund70-Case-Assembly.step" OUT=/path/to/work/mesh freecadcmd hardware/case/blender/fc_mesh.py
MESH=/path/to/work/mesh OUT=/path/to/work ACCENT=orange blender -b --factory-startup --python hardware/case/blender/build.py
blender -b /path/to/work/base_orange.blend --python hardware/case/blender/render.py -- dramatic /path/to/work/out 1024
blender -b /path/to/work/base_orange.blend --python hardware/case/blender/render.py -- views /path/to/work/out 512
blender -b /path/to/work/base_orange.blend --python hardware/case/blender/render.py -- simple /path/to/work/out 512
```

`ACCENT=green` builds the green variant. The case parts in the assembly STEP are unnamed compounds; `build.py`
maps them by name (`COMPOUND` = bottom, `001` = top, `002` = ring, `003` = encoder cover, `004` = knob,
`005` = LED guide, `006` = screen frame, `007` = switch cap, `008` = weight, `013`/`014` = rails, the rest are
screws). If the Fusion export order changes, update that map. `02-views.jpg` is a montage of the `views-*` frames: `python hardware/case/blender/montage.py /path/to/work/out 02-views.jpg`.
