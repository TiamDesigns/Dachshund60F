# blender -b base_<accent>.blend --python render.py -- <scene> <outdir> [samples] [views]
import bpy, bmesh, os, sys, math
from mathutils import Vector, Euler, Matrix
argv = sys.argv[sys.argv.index("--") + 1:]
SCENE, OUT = argv[0], argv[1]
SAMPLES = int(argv[2]) if len(argv) > 2 else 256
ONLY = argv[3].split(",") if len(argv) > 3 else None
os.makedirs(OUT, exist_ok=True)
sc = bpy.context.scene
root = bpy.data.objects["Board"]

# ---------- render settings ----------
sc.render.engine = "CYCLES"
pr = bpy.context.preferences.addons["cycles"].preferences
for t in ("HIP", "OPTIX", "CUDA", "ONEAPI"):
    try:
        pr.compute_device_type = t; pr.get_devices()
        devs = [d for d in pr.devices if d.type == t]
        if devs:
            for d in pr.devices: d.use = d.type == t and "610M" not in d.name
            sc.cycles.device = "GPU"; print("GPU", t, [d.name for d in pr.devices if d.use]); break
    except Exception as ex:
        print("no", t, ex)
sc.cycles.samples = SAMPLES
sc.cycles.use_adaptive_sampling = True
sc.cycles.use_denoising = True
sc.cycles.max_bounces = 10; sc.cycles.transmission_bounces = 10; sc.cycles.glossy_bounces = 6
sc.view_settings.view_transform = "AgX"
sc.view_settings.look = "AgX - Medium High Contrast"
sc.view_settings.exposure = {"dramatic": 0.0, "views": -0.85}.get(SCENE, 0.0)
sc.cycles.filter_width = 1.2
sc.cycles.denoising_prefilter = "ACCURATE"
sc.render.image_settings.file_format = "PNG"
sc.render.film_transparent = False

def srgb(h):
    h = h.lstrip("#"); c = [int(h[i:i+2], 16) / 255 for i in (0, 2, 4)]
    return [x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c] + [1]

def link(o):
    sc.collection.objects.link(o); return o

# ---------- board bounds ----------
def world_bounds():
    lo = Vector((1e9,) * 3); hi = Vector((-1e9,) * 3)
    for o in root.children:
        for c in o.bound_box:
            w = o.matrix_world @ Vector(c)
            lo = Vector(map(min, lo, w)); hi = Vector(map(max, hi, w))
    return lo, hi

# ---------- materials for the set ----------
def surface_mat(name, c1, c2, scale=40.0, bump=0.25, rough=0.8, voronoi=True, stretch=(1, 1, 1)):
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree
    b = nt.nodes["Principled BSDF"]; b.inputs["Roughness"].default_value = rough
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    tc = nt.nodes.new("ShaderNodeMapping"); tc.inputs["Scale"].default_value = stretch
    nt.links.new(geo.outputs["Position"], tc.inputs["Vector"])
    n = nt.nodes.new("ShaderNodeTexNoise"); n.inputs["Scale"].default_value = scale
    n.inputs["Detail"].default_value = 12; n.inputs["Roughness"].default_value = 0.62
    nt.links.new(tc.outputs["Vector"], n.inputs["Vector"])
    n2 = nt.nodes.new("ShaderNodeTexNoise"); n2.inputs["Scale"].default_value = scale * 0.08
    n2.inputs["Detail"].default_value = 4
    nt.links.new(tc.outputs["Vector"], n2.inputs["Vector"])
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].color = srgb(c1); ramp.color_ramp.elements[1].color = srgb(c2)
    ramp.color_ramp.elements[0].position = 0.35; ramp.color_ramp.elements[1].position = 0.7
    nt.links.new(n2.outputs["Fac"], ramp.inputs["Fac"])
    nt.links.new(ramp.outputs["Color"], b.inputs["Base Color"])
    h = n.outputs["Fac"]
    if voronoi:
        v = nt.nodes.new("ShaderNodeTexVoronoi"); v.inputs["Scale"].default_value = scale * 0.6
        nt.links.new(tc.outputs["Vector"], v.inputs["Vector"])
        mx = nt.nodes.new("ShaderNodeMath"); mx.operation = "MULTIPLY_ADD"
        mx.inputs[1].default_value = 0.35
        nt.links.new(v.outputs["Distance"], mx.inputs[0]); nt.links.new(n.outputs["Fac"], mx.inputs[2])
        h = mx.outputs[0]
    bp = nt.nodes.new("ShaderNodeBump"); bp.inputs["Strength"].default_value = bump
    bp.inputs["Distance"].default_value = 0.0015
    nt.links.new(h, bp.inputs["Height"]); nt.links.new(bp.outputs["Normal"], b.inputs["Normal"])
    return m

def simple_mat(name, col, rough=0.5, metal=0.0, **kw):
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = srgb(col); b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metal
    for k, v in kw.items(): b.inputs[k].default_value = v
    return m

def sweep(name, width, depth, height, radius, mat, y0=0.0):
    """Seamless cyc: floor from y=-depth/2 to back, curving up into a wall."""
    bm = bmesh.new(); prof = []
    yb = y0 + depth / 2 - radius
    for i in range(12): prof.append((y0 - depth / 2 + i * (yb - (y0 - depth / 2)) / 11, 0.0))
    for i in range(1, 17):
        a = -math.pi / 2 + i * (math.pi / 2) / 16
        prof.append((yb + radius * math.cos(a), radius + radius * math.sin(a)))
    for i in range(1, 8): prof.append((yb + radius, radius + i * (height - radius) / 7))
    rows = []
    for x in (-width / 2, width / 2):
        rows.append([bm.verts.new((x, y, z)) for y, z in prof])
    for i in range(len(prof) - 1):
        bm.faces.new((rows[0][i], rows[1][i], rows[1][i + 1], rows[0][i + 1]))
    me = bpy.data.meshes.new(name); bm.to_mesh(me)
    o = bpy.data.objects.new(name, me); link(o)
    for p in me.polygons: p.use_smooth = True
    sub = o.modifiers.new("sub", "SUBSURF"); sub.levels = sub.render_levels = 2
    me.materials.append(mat); return o

def area(name, loc, target, size, energy, color=(1, 1, 1), shape="RECTANGLE", sy=None):
    l = bpy.data.lights.new(name, "AREA"); l.energy = energy; l.color = color; l.shape = shape
    l.size = size; l.size_y = sy or size
    o = link(bpy.data.objects.new(name, l)); o.location = loc
    o.rotation_euler = (Vector(target) - Vector(loc)).to_track_quat("-Z", "Y").to_euler(); return o

def camera(name, loc, target, lens=50, fstop=None, focus=None, ortho=None, shift=(0, 0)):
    c = bpy.data.cameras.new(name); c.lens = lens; c.clip_start = 0.005; c.clip_end = 50
    c.shift_x, c.shift_y = shift
    if ortho: c.type = "ORTHO"; c.ortho_scale = ortho
    if fstop:
        c.dof.use_dof = True; c.dof.aperture_fstop = fstop
        c.dof.focus_distance = focus or (Vector(target) - Vector(loc)).length
    o = link(bpy.data.objects.new(name, c)); o.location = loc
    o.rotation_euler = (Vector(target) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
    return o

def world(col, strength):
    w = bpy.data.worlds.new("W"); w.use_nodes = True; sc.world = w
    bg = w.node_tree.nodes["Background"]; bg.inputs["Color"].default_value = srgb(col)
    bg.inputs["Strength"].default_value = strength

def shot(cam, name, res=(2400, 1600)):
    if ONLY and name not in ONLY: return
    sc.camera = cam; sc.render.resolution_x, sc.render.resolution_y = res
    sc.render.resolution_percentage = 100
    sc.render.filepath = os.path.join(OUT, name + ".png")
    bpy.ops.render.render(write_still=True); print("wrote", sc.render.filepath)

def copy_cap(keyword, loc, rot, floor=0.0):
    """Loose keycap prop: copy a cap and its legends, move them to loc."""
    parts = [o for o in root.children if o.get("key") == keyword and o["role"] in ("keycap", "legend")]
    if not parts: return
    lo, hi = Vector((1e9,) * 3), Vector((-1e9,) * 3)
    for o in parts:
        for c in o.bound_box:
            w = o.matrix_world @ Vector(c); lo = Vector(map(min, lo, w)); hi = Vector(map(max, hi, w))
    ctr = Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, lo.z))
    e = link(bpy.data.objects.new("prop_" + keyword, None)); e.location = ctr
    for o in parts:
        n = o.copy(); link(n); n.parent = None; n.matrix_world = o.matrix_world.copy()
        n.parent = e; n.matrix_parent_inverse = e.matrix_world.inverted()
    # undo the board's 4 deg tilt so the cap sits level, then rest its lowest vertex on the desk
    e.rotation_euler = Euler((-math.radians(4.0), 0, 0))
    e.rotation_euler.rotate(rot)
    e.location = loc
    bpy.context.view_layer.update()
    zmin = min((n.matrix_world @ v.co).z for n in e.children for v in n.data.vertices)
    e.location.z -= zmin - floor + 0.0001

lo, hi = world_bounds()
print("bounds", lo, hi)
CTR = Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, 0.012))

# =====================================================================
if SCENE == "dramatic":
    world("#8C93A0", 0.10)
    def node_mat(name):
        m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree
        return m, nt, next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
    def N(nt, t, **inp):
        n = nt.nodes.new(t)
        for k, v in inp.items(): n.inputs[k].default_value = v
        return n
    def box(name, size, loc, m, bevel=0.0):
        me = bpy.data.meshes.new(name); bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0); bm.to_mesh(me)
        o = link(bpy.data.objects.new(name, me)); o.scale = size; o.location = loc; me.materials.append(m)
        if bevel:
            bpy.context.view_layer.update()
            b = o.modifiers.new("bv", "BEVEL"); b.width = bevel; b.segments = 3; b.use_clamp_overlap = True
        return o

    # --- walnut desk: planks, flat-sawn cathedral grain (rings round a buried pith), pores, satin oil finish ---
    wood, nt, wb = node_mat("Walnut desk")
    def W(op, a, b=None, c=None):
        n = nt.nodes.new("ShaderNodeMath"); n.operation = op
        for i, v in enumerate((a, b, c)):
            if v is None: continue
            if isinstance(v, (int, float)): n.inputs[i].default_value = v
            else: nt.links.new(v, n.inputs[i])
        return n.outputs[0]
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    sp = nt.nodes.new("ShaderNodeSeparateXYZ"); nt.links.new(geo.outputs["Position"], sp.inputs[0])
    X, Y = sp.outputs["X"], sp.outputs["Y"]
    pid = W("FLOOR", W("DIVIDE", W("ADD", Y, 0.5), 0.145))            # plank index, 145 mm boards
    wn = nt.nodes.new("ShaderNodeTexWhiteNoise"); wn.noise_dimensions = "1D"; nt.links.new(pid, wn.inputs["W"])
    rnd = wn.outputs["Value"]
    cx = nt.nodes.new("ShaderNodeCombineXYZ")
    nt.links.new(W("MULTIPLY", X, 1.3), cx.inputs[0]); nt.links.new(W("MULTIPLY", pid, 3.7), cx.inputs[1])
    dn = N(nt, "ShaderNodeTexNoise", Scale=1.0, Detail=3.0); nt.links.new(cx.outputs[0], dn.inputs["Vector"])
    dn2 = N(nt, "ShaderNodeTexNoise", Scale=2.6, Detail=2.0); nt.links.new(cx.outputs[0], dn2.inputs["Vector"])
    yd = W("ADD", W("ADD", Y, W("MULTIPLY", W("SUBTRACT", dn.outputs["Fac"], 0.5), 0.05)), W("MULTIPLY", rnd, 0.09))
    dd = W("ADD", 0.012, W("MULTIPLY", dn2.outputs["Fac"], 0.06))  # pith depth below the surface
    rr = W("SQRT", W("ADD", W("MULTIPLY", yd, yd), W("MULTIPLY", dd, dd)))
    fine = N(nt, "ShaderNodeTexNoise", Scale=60.0, Detail=6.0); nt.links.new(geo.outputs["Position"], fine.inputs["Vector"])
    ring = W("FRACT", W("ADD", W("MULTIPLY", rr, 340.0), W("MULTIPLY", fine.outputs["Fac"], 0.8)))
    late = nt.nodes.new("ShaderNodeMapRange"); late.interpolation_type = "SMOOTHERSTEP"
    nt.links.new(ring, late.inputs[0]); late.inputs[1].default_value = 0.55; late.inputs[2].default_value = 0.92
    tone = N(nt, "ShaderNodeTexNoise", Scale=5.0, Detail=4.0); nt.links.new(geo.outputs["Position"], tone.inputs["Vector"])
    base = nt.nodes.new("ShaderNodeValToRGB"); e = base.color_ramp.elements
    e[0].position, e[0].color = 0.3, srgb("#4A2D1C"); e[1].position, e[1].color = 0.75, srgb("#7A5236")
    nt.links.new(W("ADD", W("MULTIPLY", tone.outputs["Fac"], 0.7), W("MULTIPLY", rnd, 0.3)), base.inputs["Fac"])
    mx = nt.nodes.new("ShaderNodeMix"); mx.data_type = "RGBA"
    nt.links.new(W("MULTIPLY", late.outputs[0], 0.75), mx.inputs[0]); nt.links.new(base.outputs["Color"], mx.inputs[6])
    mx.inputs[7].default_value = srgb("#2B190F")
    # open pores: short dark streaks running with the grain
    pm = N(nt, "ShaderNodeMapping"); pm.inputs["Scale"].default_value = (0.04, 1.0, 1.0)
    nt.links.new(geo.outputs["Position"], pm.inputs["Vector"])
    pore = N(nt, "ShaderNodeTexNoise", Scale=1400.0, Detail=2.0); nt.links.new(pm.outputs[0], pore.inputs["Vector"])
    pmask = W("GREATER_THAN", pore.outputs["Fac"], 0.66)
    mx2 = nt.nodes.new("ShaderNodeMix"); mx2.data_type = "RGBA"
    nt.links.new(W("MULTIPLY", pmask, 0.5), mx2.inputs[0]); nt.links.new(mx.outputs[2], mx2.inputs[6])
    mx2.inputs[7].default_value = srgb("#1E110A")
    nt.links.new(mx2.outputs[2], wb.inputs["Base Color"])
    nt.links.new(W("ADD", 0.38, W("MULTIPLY", pmask, 0.25)), wb.inputs["Roughness"])
    wb.inputs["Coat Weight"].default_value = 0.3; wb.inputs["Coat Roughness"].default_value = 0.3
    bp = N(nt, "ShaderNodeBump", Strength=0.25, Distance=0.0003)
    nt.links.new(W("ADD", W("MULTIPLY", pmask, -1.0), W("MULTIPLY", late.outputs[0], 0.3)), bp.inputs["Height"])
    nt.links.new(bp.outputs["Normal"], wb.inputs["Normal"])

    # --- lime plaster wall: mottled colour, trowel undulation, sand grain ---
    plaster, nt, plb = node_mat("Lime plaster")
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    mot = N(nt, "ShaderNodeTexNoise", Scale=3.5, Detail=8.0, Roughness=0.6)
    trw = N(nt, "ShaderNodeTexNoise", Scale=14.0, Detail=10.0, Roughness=0.55, Distortion=0.6)
    snd = N(nt, "ShaderNodeTexNoise", Scale=900.0, Detail=4.0)
    for t in (mot, trw, snd): nt.links.new(geo.outputs["Position"], t.inputs["Vector"])
    cr = nt.nodes.new("ShaderNodeValToRGB"); e = cr.color_ramp.elements
    e[0].position, e[0].color = 0.35, srgb("#A99886"); e[1].position, e[1].color = 0.68, srgb("#D4C8B6")
    nt.links.new(mot.outputs["Fac"], cr.inputs["Fac"]); nt.links.new(cr.outputs["Color"], plb.inputs["Base Color"])
    plb.inputs["Roughness"].default_value = 0.95
    b1 = N(nt, "ShaderNodeBump", Strength=0.6, Distance=0.004); nt.links.new(trw.outputs["Fac"], b1.inputs["Height"])
    b2 = N(nt, "ShaderNodeBump", Strength=0.35, Distance=0.0004); nt.links.new(snd.outputs["Fac"], b2.inputs["Height"])
    nt.links.new(b1.outputs["Normal"], b2.inputs["Normal"]); nt.links.new(b2.outputs["Normal"], plb.inputs["Normal"])
    floor_m = simple_mat("Floor", "#2A231E", 0.8)
    # Desk pulled 6 cm off the wall: a gap with the floor in shadow below
    box("Desk", (1.6, 1.15, 0.032), (0, 0.24 - 0.575, -0.016), wood, bevel=0.002)
    box("Wall", (3.0, 0.05, 2.6), (0, 0.30 + 0.025, 0.55), plaster)
    box("Floor", (3.0, 3.0, 0.02), (0, 0.0, -0.76), floor_m)

    # --- low warm sun through blinds (gobo card); hard-edged shadows ---
    d = Vector((0.62, 0.62, -0.48)).normalized()
    sun = bpy.data.lights.new("Sun", "SUN"); sun.energy = 4.6; sun.angle = math.radians(0.25)
    sun.color = (1.0, 0.78, 0.55)
    so = link(bpy.data.objects.new("Sun", sun)); so.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()
    g = bpy.data.materials.new("Gobo"); g.use_nodes = True; nt = g.node_tree
    for n in list(nt.nodes): nt.nodes.remove(n)
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    tr = nt.nodes.new("ShaderNodeBsdfTransparent"); bk = nt.nodes.new("ShaderNodeBsdfDiffuse")
    bk.inputs["Color"].default_value = (0, 0, 0, 1)
    mix = nt.nodes.new("ShaderNodeMixShader")
    tcn = nt.nodes.new("ShaderNodeTexCoord")
    sep = nt.nodes.new("ShaderNodeSeparateXYZ"); nt.links.new(tcn.outputs["Generated"], sep.inputs[0])
    def mth(op, a, b=None):
        n = nt.nodes.new("ShaderNodeMath"); n.operation = op
        for i, v in enumerate((a, b)):
            if v is None: continue
            if isinstance(v, (int, float)): n.inputs[i].default_value = v
            else: nt.links.new(v, n.inputs[i])
        return n.outputs[0]
    x, y = sep.outputs["X"], sep.outputs["Y"]
    GX = float(os.environ.get("GOBO_X", "-0.12"))  # slides the window so the mullion misses the board
    inx = mth("MULTIPLY", mth("GREATER_THAN", x, 0.24 + GX), mth("LESS_THAN", x, 0.73 + GX))
    mull = mth("MULTIPLY", mth("GREATER_THAN", x, 0.475 + GX), mth("LESS_THAN", x, 0.495 + GX))
    iny = mth("MULTIPLY", mth("GREATER_THAN", y, 0.22), mth("LESS_THAN", y, 0.84))
    slat = mth("LESS_THAN", mth("FRACT", mth("MULTIPLY", y, 1 / 0.026)), 0.30)
    openf = mth("MULTIPLY", mth("MULTIPLY", inx, iny), mth("MULTIPLY", mth("SUBTRACT", 1.0, mull), mth("SUBTRACT", 1.0, slat)))
    nt.links.new(openf, mix.inputs[0]); nt.links.new(bk.outputs[0], mix.inputs[1]); nt.links.new(tr.outputs[0], mix.inputs[2])
    nt.links.new(mix.outputs[0], out.inputs["Surface"])
    bpy.ops.mesh.primitive_plane_add(size=1.0)
    gp = bpy.context.object; gp.data.materials.append(g); gp.scale = (2.6, 2.6, 1)
    gp.location = Vector((0.0, 0.05, 0.25)) - d * 1.4
    gp.rotation_euler = (-d).to_track_quat("Z", "Y").to_euler()
    gp.rotation_euler.rotate_axis("Z", math.radians(8))
    gp.visible_camera = False; gp.visible_glossy = False
    area("Fill", (0.9, -0.9, 0.4), CTR, 1.2, 7, (0.85, 0.9, 1.0))
    area("Bounce", (-0.2, -1.0, 0.15), CTR, 1.6, 4, (1.0, 0.86, 0.7))

    # --- wool felt mat, rounded corners: fibres, mottling, a soft cut edge ---
    MAT_T = 0.004
    bm = bmesh.new(); W2, H2, r = 0.36, 0.17, 0.028; pts = []
    for cx, cy, a0 in ((W2 - r, H2 - r, 0), (-W2 + r, H2 - r, 90), (-W2 + r, -H2 + r, 180), (W2 - r, -H2 + r, 270)):
        for i in range(13):
            a = math.radians(a0 + i * 7.5); pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    vs = [bm.verts.new((px, py, 0)) for px, py in pts]; f = bm.faces.new(vs)
    ext = bmesh.ops.extrude_face_region(bm, geom=[f])
    for v in ext["geom"]:
        if isinstance(v, bmesh.types.BMVert): v.co.z = MAT_T
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new("Mat"); bm.to_mesh(me)
    mat_o = link(bpy.data.objects.new("Felt mat", me)); mat_o.location = (-0.03, -0.045, 0.0)
    mat_o.rotation_euler.z = math.radians(-1.5)
    bv = mat_o.modifiers.new("bv", "BEVEL"); bv.width = 0.0012; bv.segments = 4; bv.limit_method = "ANGLE"
    felt, nt, fb = node_mat("Wool felt")
    fb.inputs["Roughness"].default_value = 1.0; fb.inputs["Sheen Weight"].default_value = 1.0
    fb.inputs["Sheen Roughness"].default_value = 0.4; fb.inputs["Sheen Tint"].default_value = srgb("#CFCAC0")
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    fib = N(nt, "ShaderNodeTexNoise", Scale=2400.0, Detail=14.0, Roughness=0.75, Distortion=2.5)
    mot = N(nt, "ShaderNodeTexNoise", Scale=90.0, Detail=6.0)
    pill = N(nt, "ShaderNodeTexVoronoi", Scale=520.0)
    hair = N(nt, "ShaderNodeTexVoronoi", Scale=1800.0); hair.feature = "DISTANCE_TO_EDGE"
    for t in (fib, mot, pill, hair): nt.links.new(geo.outputs["Position"], t.inputs["Vector"])
    cr = nt.nodes.new("ShaderNodeValToRGB"); e = cr.color_ramp.elements
    e[0].position, e[0].color = 0.28, srgb("#34332F"); e[1].position, e[1].color = 0.74, srgb("#64625B")
    mm = N(nt, "ShaderNodeMath"); mm.operation = "MULTIPLY_ADD"; mm.inputs[1].default_value = 0.65
    nt.links.new(fib.outputs["Fac"], mm.inputs[0]); nt.links.new(mot.outputs["Fac"], mm.inputs[2])
    nt.links.new(mm.outputs[0], cr.inputs["Fac"]); nt.links.new(cr.outputs["Color"], fb.inputs["Base Color"])
    hh = N(nt, "ShaderNodeMath"); hh.operation = "MULTIPLY_ADD"; hh.inputs[1].default_value = 0.8
    nt.links.new(hair.outputs["Distance"], hh.inputs[0]); nt.links.new(fib.outputs["Fac"], hh.inputs[2])
    pp = N(nt, "ShaderNodeMath"); pp.operation = "MULTIPLY_ADD"; pp.inputs[1].default_value = -0.5
    nt.links.new(pill.outputs["Distance"], pp.inputs[0]); nt.links.new(hh.outputs[0], pp.inputs[2])
    bp = N(nt, "ShaderNodeBump", Strength=1.6, Distance=0.0010)
    nt.links.new(pp.outputs[0], bp.inputs["Height"]); nt.links.new(bp.outputs["Normal"], fb.inputs["Normal"])
    me.materials.append(felt)
    # black edge stitching: short thread dashes 6 mm in from the edge, every 4 mm
    thread = simple_mat("Black thread", "#070707", 0.92)
    ins, step = 0.007, 0.005
    w2, h2, rr2 = W2 - ins, H2 - ins, r - ins
    path = []
    for cx, cy, a0 in ((w2 - rr2, h2 - rr2, 0), (-w2 + rr2, h2 - rr2, 90), (-w2 + rr2, -h2 + rr2, 180), (w2 - rr2, -h2 + rr2, 270)):
        for i in range(25):
            a = math.radians(a0 + i * 3.75); path.append(Vector((cx + rr2 * math.cos(a), cy + rr2 * math.sin(a), 0)))
    dense = []
    for i in range(len(path)):
        p0, p1 = path[i], path[(i + 1) % len(path)]
        n = max(1, int((p1 - p0).length / 0.0005))
        dense += [p0.lerp(p1, k / n) for k in range(n)]
    bm2 = bmesh.new(); acc = 0.0; prev = dense[0]
    for p in dense[1:]:
        acc += (p - prev).length; prev = p
        if acc >= step:
            acc = 0.0
            i = dense.index(p); t = (dense[(i + 1) % len(dense)] - dense[i - 1]).normalized()
            ang = math.atan2(t.y, t.x)
            mtx = (Matrix.Translation((p.x, p.y, MAT_T + 0.0001)) @ Matrix.Rotation(ang, 4, "Z")
                   @ Matrix.Diagonal((0.0034, 0.0011, 0.0009, 1.0)))
            bmesh.ops.create_cube(bm2, size=1.0, matrix=mtx)
    me2 = bpy.data.meshes.new("Stitching"); bm2.to_mesh(me2)
    st = link(bpy.data.objects.new("Stitching", me2)); st.parent = mat_o; me2.materials.append(thread)
    sb = st.modifiers.new("bv", "BEVEL"); sb.width = 0.0002; sb.segments = 2
    root.location.z += MAT_T

    # --- props ---
    mug_m = simple_mat("Glazed ceramic", "#ECE6DA", 0.1, **{"Coat Weight": 0.6})
    bm3 = bmesh.new()
    bmesh.ops.create_cone(bm3, cap_ends=True, segments=128, radius1=0.042, radius2=0.042, depth=0.095)
    top = max(bm3.faces, key=lambda f: f.calc_center_median().z); bmesh.ops.delete(bm3, geom=[top], context="FACES")
    me3 = bpy.data.meshes.new("Mug"); bm3.to_mesh(me3)
    mug = link(bpy.data.objects.new("Mug", me3)); mug.location = (0.15, 0.17, 0.0475); me3.materials.append(mug_m)
    so_ = mug.modifiers.new("wall", "SOLIDIFY"); so_.thickness = 0.0045; so_.offset = -1
    b2 = mug.modifiers.new("bv", "BEVEL"); b2.width = 0.0012; b2.segments = 3; b2.limit_method = "ANGLE"
    for p in me3.polygons: p.use_smooth = True
    bpy.ops.mesh.primitive_torus_add(major_radius=0.026, minor_radius=0.0065, location=(0.15 + 0.050, 0.17, 0.05),
                                     rotation=(math.radians(90), 0, 0), major_segments=64, minor_segments=24)
    bpy.context.object.data.materials.append(mug_m)
    coffee, nt, cb = node_mat("Coffee with crema")
    tcc = nt.nodes.new("ShaderNodeTexCoord"); ln = nt.nodes.new("ShaderNodeVectorMath"); ln.operation = "LENGTH"
    nt.links.new(tcc.outputs["Object"], ln.inputs[0])
    ns = N(nt, "ShaderNodeTexNoise", Scale=400.0, Detail=6.0); nt.links.new(tcc.outputs["Object"], ns.inputs["Vector"])
    rim = nt.nodes.new("ShaderNodeMapRange"); rim.inputs[1].default_value = 0.028; rim.inputs[2].default_value = 0.0375
    nt.links.new(ln.outputs[0], rim.inputs[0])
    cc = nt.nodes.new("ShaderNodeValToRGB"); e = cc.color_ramp.elements
    e[0].position, e[0].color = 0.2, srgb("#2A150A"); e[1].position, e[1].color = 0.9, srgb("#9A6234")
    mxc = N(nt, "ShaderNodeMath"); mxc.operation = "MULTIPLY_ADD"; mxc.inputs[1].default_value = 0.35
    nt.links.new(ns.outputs["Fac"], mxc.inputs[0]); nt.links.new(rim.outputs[0], mxc.inputs[2])
    nt.links.new(mxc.outputs[0], cc.inputs["Fac"]); nt.links.new(cc.outputs["Color"], cb.inputs["Base Color"])
    cb.inputs["Roughness"].default_value = 0.08
    bpy.ops.mesh.primitive_cylinder_add(radius=0.0374, depth=0.001, vertices=128, location=(0.15, 0.17, 0.0885))
    bpy.context.object.data.materials.append(coffee)
    # Cloth-bound notebook: linen weave cover, cream page block, an elastic band
    linen, nt, lb = node_mat("Linen cover")
    tco = nt.nodes.new("ShaderNodeTexCoord")
    wx = N(nt, "ShaderNodeTexWave", Scale=420.0, Distortion=0.6); wx.bands_direction = "X"
    wy = N(nt, "ShaderNodeTexWave", Scale=420.0, Distortion=0.6); wy.bands_direction = "Y"
    sl = N(nt, "ShaderNodeTexNoise", Scale=60.0)
    for t in (wx, wy, sl): nt.links.new(tco.outputs["Object"], t.inputs["Vector"])
    wm = N(nt, "ShaderNodeMath"); wm.operation = "MULTIPLY"
    nt.links.new(wx.outputs["Fac"], wm.inputs[0]); nt.links.new(wy.outputs["Fac"], wm.inputs[1])
    cr = nt.nodes.new("ShaderNodeValToRGB"); e = cr.color_ramp.elements
    e[0].color = srgb("#2F3B31"); e[1].color = srgb("#4C5E4D")
    nt.links.new(sl.outputs["Fac"], cr.inputs["Fac"]); nt.links.new(cr.outputs["Color"], lb.inputs["Base Color"])
    lb.inputs["Roughness"].default_value = 0.85; lb.inputs["Sheen Weight"].default_value = 0.4
    bp = N(nt, "ShaderNodeBump", Strength=0.5, Distance=0.0003)
    nt.links.new(wm.outputs[0], bp.inputs["Height"]); nt.links.new(bp.outputs["Normal"], lb.inputs["Normal"])
    pages, nt, pb = node_mat("Pages")
    pb.inputs["Base Color"].default_value = srgb("#EDE4D0"); pb.inputs["Roughness"].default_value = 0.9
    tcp = nt.nodes.new("ShaderNodeTexCoord")
    pw = N(nt, "ShaderNodeTexWave", Scale=180.0); pw.bands_direction = "Z"; nt.links.new(tcp.outputs["Object"], pw.inputs["Vector"])
    bp = N(nt, "ShaderNodeBump", Strength=0.3, Distance=0.0002)
    nt.links.new(pw.outputs["Fac"], bp.inputs["Height"]); nt.links.new(bp.outputs["Normal"], pb.inputs["Normal"])
    nb = link(bpy.data.objects.new("Notebook", None)); nb.location = (0.42, 0.10, 0.0); nb.rotation_euler.z = math.radians(-14)
    for nm, sz, lc, m in (("Cover bottom", (0.150, 0.212, 0.0016), (0, 0, 0.0008), linen),
                          ("Page block", (0.144, 0.206, 0.0110), (-0.002, 0, 0.0071), pages),
                          ("Cover top", (0.150, 0.212, 0.0016), (0, 0, 0.0134), linen),
                          ("Elastic", (0.006, 0.2135, 0.0150), (0.058, 0, 0.0071), simple_mat("Elastic", "#1E1E1E", 0.6))):
        o = box(nm, sz, lc, m, bevel=0.0007); o.parent = nb
    pen_m = simple_mat("Pen", "#1C1C1C", 0.3, 0.6)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.0045, depth=0.14, vertices=48, location=(0.41, 0.07, 0.019),
                                        rotation=(0, math.radians(90), math.radians(62)))
    bpy.context.object.data.materials.append(pen_m)
    # Plant in a terracotta pot, and a framed print resting on the desk, leaning on the wall
    terra = surface_mat("Terracotta", "#9C5235", "#B8694A", scale=120, bump=0.3, rough=0.85)
    bpy.ops.mesh.primitive_cone_add(vertices=96, radius1=0.036, radius2=0.047, depth=0.075, location=(-0.24, 0.17, 0.0375))
    pot = bpy.context.object; pot.data.materials.append(terra)
    for p in pot.data.polygons: p.use_smooth = True
    bpy.ops.mesh.primitive_cylinder_add(radius=0.044, depth=0.002, vertices=64, location=(-0.24, 0.17, 0.07))
    bpy.context.object.data.materials.append(simple_mat("Soil", "#2B1D14", 0.95))
    leaf = simple_mat("Leaf", "#3F6B3A", 0.35, **{"Subsurface Weight": 0.15, "Coat Weight": 0.3})
    import random; random.seed(7)
    for i in range(14):
        a = i * 2.399; tilt = math.radians(18 + 22 * random.random()); h = 0.07 + 0.05 * random.random()
        bpy.ops.mesh.primitive_uv_sphere_add(segments=24, ring_count=12, radius=1.0)
        lf = bpy.context.object; lf.data.materials.append(leaf)
        for p in lf.data.polygons: p.use_smooth = True
        lf.scale = (0.011, 0.0035, h / 2); lf.rotation_euler = Euler((tilt, 0, a))
        lf.location = Vector((-0.24, 0.17, 0.07)) + Euler((tilt, 0, a)).to_matrix() @ Vector((0, 0, h / 2))
    frame_m = simple_mat("Walnut frame", "#3A2618", 0.4)
    art = bpy.data.materials.new("Print"); art.use_nodes = True; nt = art.node_tree
    pbsdf = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED"); pbsdf.inputs["Roughness"].default_value = 0.9
    tc2 = nt.nodes.new("ShaderNodeTexCoord"); wv2 = nt.nodes.new("ShaderNodeTexWave")
    wv2.inputs["Scale"].default_value = 0.5; wv2.inputs["Distortion"].default_value = 9.0; wv2.wave_type = "RINGS"
    nt.links.new(tc2.outputs["Object"], wv2.inputs["Vector"])
    cr2 = nt.nodes.new("ShaderNodeValToRGB"); cr2.color_ramp.interpolation = "CONSTANT"
    cr2.color_ramp.elements[0].color = srgb("#E2D8C4"); cr2.color_ramp.elements[1].color = srgb("#C48A68")
    cr2.color_ramp.elements[1].position = 0.62
    e3 = cr2.color_ramp.elements.new(0.82); e3.color = srgb("#8A9C88")
    nt.links.new(wv2.outputs["Fac"], cr2.inputs["Fac"]); nt.links.new(cr2.outputs["Color"], pbsdf.inputs["Base Color"])
    # pivot on the desk 1 cm in from the back edge; lean the top back onto the wall
    H, lean = 0.29, math.asin((0.30 - 0.23 - 0.014) / 0.29)
    pv = link(bpy.data.objects.new("Frame pivot", None)); pv.location = (-0.02, 0.23, 0.0); pv.rotation_euler.x = -lean
    fr = box("Frame", (0.22, 0.014, H), (0, 0.007, H / 2), frame_m, bevel=0.002); fr.parent = pv
    pr2 = box("Print", (0.18, 0.002, H - 0.04), (0, -0.0005, H / 2), art); pr2.parent = pv
    glass = simple_mat("Picture glass", "#FFFFFF", 0.01, **{"Transmission Weight": 1.0, "IOR": 1.52})
    gl = box("Glass", (0.192, 0.0015, H - 0.028), (0, -0.0022, H / 2), glass); gl.parent = pv; gl.visible_shadow = False
    copy_cap("Esc", (0.20, -0.16, 0.0), Euler((0, 0, math.radians(32))), floor=MAT_T)
    # Board fills the frame; shallow depth of field drops the props out of focus
    cam = camera("Cam", (-0.40, -0.63, 0.25), (0.02, 0.0, 0.012), lens=55, fstop=6.3)
    cam.data.dof.focus_distance = (Vector((-0.01, -0.03, 0.02)) - cam.location).length
    shot(cam, "01-dramatic", (3600, 2400))


# =====================================================================
elif SCENE == "views":
    # Deep green seamless paper, turned to face each camera so there is never a void behind the board
    world("#24352F", 0.5)
    paper = bpy.data.materials.new("Deep green paper"); paper.use_nodes = True; nt = paper.node_tree
    pb = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
    pb.inputs["Base Color"].default_value = srgb("#2E4339"); pb.inputs["Roughness"].default_value = 0.9
    geo = nt.nodes.new("ShaderNodeNewGeometry"); gr = nt.nodes.new("ShaderNodeTexNoise")
    gr.inputs["Scale"].default_value = 1800.0; gr.inputs["Detail"].default_value = 3.0
    nt.links.new(geo.outputs["Position"], gr.inputs["Vector"])
    bpn = nt.nodes.new("ShaderNodeBump"); bpn.inputs["Strength"].default_value = 0.08; bpn.inputs["Distance"].default_value = 0.0002
    nt.links.new(gr.outputs["Fac"], bpn.inputs["Height"]); nt.links.new(bpn.outputs["Normal"], pb.inputs["Normal"])
    pb.inputs["Emission Color"].default_value = srgb("#2E4339")
    # the curve and wall glow a little (the floor doesn't), so the backdrop never falls off to black
    nz = nt.nodes.new("ShaderNodeSeparateXYZ"); nt.links.new(geo.outputs["Normal"], nz.inputs[0])
    em = nt.nodes.new("ShaderNodeMapRange"); em.inputs[1].default_value = 1.0; em.inputs[2].default_value = 0.3
    em.inputs[3].default_value = 0.15; em.inputs[4].default_value = 1.1
    nt.links.new(nz.outputs["Z"], em.inputs[0]); nt.links.new(em.outputs[0], pb.inputs["Emission Strength"])
    cyc = sweep("Cyc", 12.0, 3.0, 3.0, 0.6, paper, y0=0.6)
    wash = area("Wash", (0, -0.6, 0.9), (0, 2.1, 0.8), 3.0, 30, (1.0, 0.98, 0.95)); wash.parent = cyc
    key = area("Key", (-0.55, -0.55, 0.75), CTR, 0.6, 14, (1.0, 0.96, 0.9))
    rim = area("Rim", (0.6, 0.55, 0.45), CTR, 0.6, 8, (1.0, 0.95, 0.88))
    def face(cam):
        f = cam.rotation_euler.to_quaternion() @ Vector((0, 0, -1))
        cyc.rotation_euler.z = math.atan2(-f.x, f.y) if Vector((f.x, f.y)).length > 0.2 else 0.0
    v = {
        "hero": camera("hero", (-0.36, -0.52, 0.30), (0.0, 0.0, 0.01), lens=50, fstop=5.6),
        "top": camera("top", (0, 0, 0.9), (0, 0.0001, 0), lens=50, ortho=0.39),
        "back": camera("back", (0.05, 0.78, 0.14), (0.0, 0.0, 0.012), lens=50),
        "right-end": camera("right-end", (0.62, -0.12, 0.10), (0.11, 0.0, 0.02), lens=55),
        "knob": camera("knob", (0.32, -0.20, 0.16), (0.14, 0.01, 0.022), lens=85, fstop=6.3),
    }
    for k in ("hero", "top", "back", "right-end", "knob"):
        face(v[k]); shot(v[k], "views-" + k, (3600, 2400) if k == "hero" else (2400, 1600))
    # underside: flip the board onto its keycaps
    root.rotation_euler = (math.radians(180), 0, 0)
    bpy.context.view_layer.update(); lo2, hi2 = world_bounds()
    root.location.z -= lo2.z; root.location.y -= (lo2.y + hi2.y) / 2
    bpy.context.view_layer.update()
    cam = camera("under", (-0.30, -0.48, 0.42), (0.0, 0.0, 0.03), lens=50, fstop=5.6)
    face(cam); shot(cam, "views-underside", (2400, 1600))
    # weight close-up: the polished face mirrors whatever is above it, so give it one big, even, dim softbox
    # (a smooth gold gradient) and let the engraving's walls catch a low side light
    for o in (key, rim): o.hide_render = True
    camw = camera("weight", (0.0, 0.20, 0.30), (0.0, 0.0, 0.035), lens=85, fstop=4.5)
    face(camw)
    area("Weight soft", (0.0, -0.30, 0.40), (0.0, 0.0, 0.035), 0.6, 0.45, (1.0, 0.96, 0.9), sy=0.18)
    area("Weight rake", (0.55, 0.05, 0.08), (0.0, 0.0, 0.035), 0.25, 4, (1.0, 0.95, 0.88))
    shot(camw, "views-weight", (2400, 1600))

# =====================================================================
