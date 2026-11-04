# blender -b base_<accent>.blend --python render.py -- <scene> <outdir> [samples] [views]
import bpy, bmesh, os, sys, math
from mathutils import Vector, Euler
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
sc.view_settings.look = "AgX - High Contrast" if SCENE == "dramatic" else "AgX - Medium High Contrast"
sc.view_settings.exposure = {"dramatic": -0.35, "views": -0.3, "simple": 0.3}.get(SCENE, 0.0)
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
    world("#1A2030", 0.05)
    plaster = surface_mat("Plaster", "#A08E7A", "#CDBFAD", scale=30, bump=0.9, rough=0.92)
    wood = surface_mat("Oak desk", "#432A18", "#8E6640", scale=6, bump=0.10, rough=0.45, voronoi=False,
                       stretch=(0.25, 16.0, 1.0))
    def box(name, size, loc, m):
        me = bpy.data.meshes.new(name); bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0); bm.to_mesh(me)
        o = link(bpy.data.objects.new(name, me)); o.scale = size; o.location = loc; me.materials.append(m); return o
    box("Desk", (2.0, 1.2, 0.03), (0, -0.30, -0.015), wood)
    box("Wall", (3.0, 0.05, 2.0), (0, 0.30 + 0.025, 1.0), plaster)
    # Low warm sun from the front left, through a window with blinds (a gobo card)
    d = Vector((0.62, 0.62, -0.48)).normalized()
    sun = bpy.data.lights.new("Sun", "SUN"); sun.energy = 6.5; sun.angle = math.radians(0.8)
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
    # window: two panes x in [0.30,0.47] and [0.50,0.67], y in [0.25,0.80]; blinds every 0.035 in y
    inx = mth("MULTIPLY", mth("GREATER_THAN", x, 0.30), mth("LESS_THAN", x, 0.67))
    mull = mth("MULTIPLY", mth("GREATER_THAN", x, 0.47), mth("LESS_THAN", x, 0.50))
    iny = mth("MULTIPLY", mth("GREATER_THAN", y, 0.25), mth("LESS_THAN", y, 0.80))
    slat = mth("LESS_THAN", mth("FRACT", mth("MULTIPLY", y, 1 / 0.022)), 0.42)
    openf = mth("MULTIPLY", mth("MULTIPLY", inx, iny), mth("MULTIPLY", mth("SUBTRACT", 1.0, mull), mth("SUBTRACT", 1.0, slat)))
    nt.links.new(openf, mix.inputs[0]); nt.links.new(bk.outputs[0], mix.inputs[1]); nt.links.new(tr.outputs[0], mix.inputs[2])
    nt.links.new(mix.outputs[0], out.inputs["Surface"])
    bpy.ops.mesh.primitive_plane_add(size=1.0)
    gp = bpy.context.object; gp.data.materials.append(g); gp.scale = (2.6, 2.6, 1)
    gp.location = Vector((0.0, 0.05, 0.25)) - d * 1.4
    gp.rotation_euler = (-d).to_track_quat("Z", "Y").to_euler()
    gp.rotation_euler.rotate_axis("Z", math.radians(8))
    gp.visible_camera = False; gp.visible_glossy = False
    area("Fill", (0.9, -0.9, 0.4), CTR, 1.0, 4, (0.7, 0.8, 1.0))
    # Props: ceramic mug, a notebook, two loose keycaps
    mug_m = simple_mat("Glazed ceramic", "#ECE6DA", 0.1, **{"Coat Weight": 0.6})
    bpy.ops.mesh.primitive_cylinder_add(radius=0.042, depth=0.095, vertices=128, location=(0.13, 0.215, 0.0475))
    mug = bpy.context.object; mug.data.materials.append(mug_m)
    bv = mug.modifiers.new("bv", "BEVEL"); bv.width = 0.003; bv.segments = 4
    for p in mug.data.polygons: p.use_smooth = True
    bpy.ops.mesh.primitive_torus_add(major_radius=0.026, minor_radius=0.0065, location=(0.13 + 0.050, 0.215, 0.05),
                                     rotation=(math.radians(90), 0, 0), major_segments=64, minor_segments=24)
    bpy.context.object.data.materials.append(mug_m)
    coffee = simple_mat("Coffee", "#24140A", 0.04)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.039, depth=0.002, vertices=128, location=(0.13, 0.215, 0.084))
    bpy.context.object.data.materials.append(coffee)
    nb = simple_mat("Notebook", "#36443A", 0.8)
    o = box("Notebook", (0.15, 0.21, 0.012), (0.41, 0.07, 0.006), nb); o.rotation_euler.z = math.radians(-14)
    # Felt desk mat with rounded corners under the board
    MAT_T = 0.004
    bm = bmesh.new(); W2, H2, r = 0.36, 0.17, 0.028; pts = []
    for cx, cy, a0 in ((W2 - r, H2 - r, 0), (-W2 + r, H2 - r, 90), (-W2 + r, -H2 + r, 180), (W2 - r, -H2 + r, 270)):
        for i in range(13):
            a = math.radians(a0 + i * 7.5); pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    vs = [bm.verts.new((x, y, 0)) for x, y in pts]; f = bm.faces.new(vs)
    ext = bmesh.ops.extrude_face_region(bm, geom=[f])
    for v in ext["geom"]:
        if isinstance(v, bmesh.types.BMVert): v.co.z = MAT_T
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new("Mat"); bm.to_mesh(me)
    mat_o = link(bpy.data.objects.new("Felt mat", me)); mat_o.location = (-0.05, -0.05, 0.0)
    mat_o.rotation_euler.z = math.radians(-1.5)
    bv = mat_o.modifiers.new("bv", "BEVEL"); bv.width = 0.0015; bv.segments = 3; bv.limit_method = "ANGLE"
    felt = bpy.data.materials.new("Wool felt"); felt.use_nodes = True; nt = felt.node_tree
    fb = nt.nodes["Principled BSDF"]; fb.inputs["Base Color"].default_value = srgb("#4E4C49")
    fb.inputs["Roughness"].default_value = 1.0; fb.inputs["Sheen Weight"].default_value = 0.8
    fb.inputs["Sheen Roughness"].default_value = 0.45; fb.inputs["Sheen Tint"].default_value = srgb("#B9B4AC")
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    n1 = nt.nodes.new("ShaderNodeTexNoise"); n1.inputs["Scale"].default_value = 2200; n1.inputs["Detail"].default_value = 10
    n2 = nt.nodes.new("ShaderNodeTexNoise"); n2.inputs["Scale"].default_value = 60; n2.inputs["Detail"].default_value = 4
    nt.links.new(geo.outputs["Position"], n1.inputs["Vector"]); nt.links.new(geo.outputs["Position"], n2.inputs["Vector"])
    cr = nt.nodes.new("ShaderNodeValToRGB"); cr.color_ramp.elements[0].color = srgb("#46443F"); cr.color_ramp.elements[1].color = srgb("#58564F")
    nt.links.new(n2.outputs["Fac"], cr.inputs["Fac"]); nt.links.new(cr.outputs["Color"], fb.inputs["Base Color"])
    bp = nt.nodes.new("ShaderNodeBump"); bp.inputs["Strength"].default_value = 0.35; bp.inputs["Distance"].default_value = 0.0004
    nt.links.new(n1.outputs["Fac"], bp.inputs["Height"]); nt.links.new(bp.outputs["Normal"], fb.inputs["Normal"])
    me.materials.append(felt)
    root.location.z += MAT_T
    # More set dressing behind the board: a potted plant and a framed print leaning on the wall
    terra = surface_mat("Terracotta", "#9C5235", "#B8694A", scale=120, bump=0.3, rough=0.85)
    bpy.ops.mesh.primitive_cone_add(vertices=96, radius1=0.036, radius2=0.047, depth=0.075, location=(-0.12, 0.225, 0.0375))
    pot = bpy.context.object; pot.data.materials.append(terra)
    for p in pot.data.polygons: p.use_smooth = True
    soil = simple_mat("Soil", "#2B1D14", 0.95)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.044, depth=0.002, vertices=64, location=(-0.12, 0.225, 0.07))
    bpy.context.object.data.materials.append(soil)
    leaf = simple_mat("Leaf", "#3F6B3A", 0.35, **{"Subsurface Weight": 0.15, "Coat Weight": 0.3})
    import random; random.seed(7)
    for i in range(14):
        a = i * 2.399; tilt = math.radians(18 + 22 * random.random())
        h = 0.07 + 0.05 * random.random()
        bpy.ops.mesh.primitive_uv_sphere_add(segments=24, ring_count=12, radius=1.0)
        lf = bpy.context.object; lf.data.materials.append(leaf)
        for p in lf.data.polygons: p.use_smooth = True
        lf.scale = (0.011, 0.0035, h / 2)
        lf.rotation_euler = Euler((tilt, 0, a))
        lf.location = Vector((-0.12, 0.225, 0.07)) + Euler((tilt, 0, a)).to_matrix() @ Vector((0, 0, h / 2))
    frame_m = simple_mat("Walnut frame", "#3A2618", 0.4)
    fr = box("Frame", (0.22, 0.014, 0.29), (0.34, 0.285, 0.143), frame_m); fr.rotation_euler.x = math.radians(-6)
    art = bpy.data.materials.new("Print"); art.use_nodes = True; nt = art.node_tree
    pb = nt.nodes["Principled BSDF"]; pb.inputs["Roughness"].default_value = 0.9
    tc2 = nt.nodes.new("ShaderNodeTexCoord"); wv = nt.nodes.new("ShaderNodeTexWave")
    wv.inputs["Scale"].default_value = 0.5; wv.inputs["Distortion"].default_value = 9.0; wv.wave_type = "RINGS"
    nt.links.new(tc2.outputs["Object"], wv.inputs["Vector"])
    cr = nt.nodes.new("ShaderNodeValToRGB"); cr.color_ramp.interpolation = "CONSTANT"
    cr.color_ramp.elements[0].color = srgb("#E2D8C4"); cr.color_ramp.elements[1].color = srgb("#C48A68")
    cr.color_ramp.elements[1].position = 0.62
    e3 = cr.color_ramp.elements.new(0.82); e3.color = srgb("#8A9C88")
    nt.links.new(wv.outputs["Fac"], cr.inputs["Fac"]); nt.links.new(cr.outputs["Color"], pb.inputs["Base Color"])
    pr2 = box("Print", (0.18, 0.002, 0.25), (0.34, 0.2775, 0.143), art); pr2.rotation_euler.x = math.radians(-6)
    pen_m = simple_mat("Pen", "#1C1C1C", 0.3, 0.6)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.0045, depth=0.14, vertices=32, location=(0.40, 0.05, 0.0165),
                                        rotation=(0, math.radians(90), math.radians(62)))
    bpy.context.object.data.materials.append(pen_m)
    copy_cap("Esc", (0.19, -0.17, 0.0), Euler((0, 0, math.radians(32))), floor=MAT_T)
    cam = camera("Cam", (-0.40, -0.62, 0.24), (0.03, 0.0, 0.02), lens=55, fstop=6.3)
    shot(cam, "01-dramatic", (2400, 1600))


# =====================================================================
elif SCENE == "views":
    world("#A9BBAA", 0.3)
    sage = surface_mat("Sage paper", "#A9BBAA", "#B6C6B5", scale=90, bump=0.12, rough=0.95)
    sweep("Cyc", 8.0, 1.6, 1.6, 0.35, sage, y0=0.25)
    area("Wash", (0, -0.6, 0.9), (0, 1.05, 0.5), 2.5, 14, (1.0, 0.98, 0.95))
    area("Key", (-0.55, -0.55, 0.75), CTR, 0.9, 16, (1.0, 0.96, 0.9))
    area("Rim", (0.6, 0.55, 0.45), CTR, 0.6, 9, (1.0, 0.95, 0.88))
    area("Top", (0.0, -0.1, 1.1), CTR, 1.4, 2)
    d = 0.75
    v = {
        "hero": camera("hero", (-0.36, -0.52, 0.30), (0.0, 0.0, 0.01), lens=50, fstop=5.6),
        "front": camera("front", (0, -0.85, 0.06), (0, 0, 0.012), lens=70),
        "top": camera("top", (0, 0, 0.9), (0, 0.0001, 0), lens=50, ortho=0.39),
        "back": camera("back", (0.05, 0.78, 0.14), (0.0, 0.0, 0.012), lens=50),
        "right-end": camera("right-end", (0.62, -0.12, 0.10), (0.11, 0.0, 0.02), lens=55),
        "knob": camera("knob", (0.32, -0.20, 0.16), (0.14, 0.01, 0.022), lens=85, fstop=6.3),
    }
    for k in ("hero", "front", "top", "back", "right-end", "knob"):
        shot(v[k], "views-" + k, (2400, 1600) if k == "hero" else (1600, 1067))
    # underside: flip the board onto its keycaps
    root.rotation_euler = (math.radians(180), 0, 0)
    bpy.context.view_layer.update(); lo2, hi2 = world_bounds()
    root.location.z -= lo2.z; root.location.y -= (lo2.y + hi2.y) / 2
    bpy.context.view_layer.update()
    cam = camera("under", (-0.30, -0.48, 0.42), (0.0, 0.0, 0.03), lens=50, fstop=5.6)
    shot(cam, "views-underside", (1600, 1067))
    camw = camera("weight", (0.0, 0.20, 0.30), (0.0, 0.0, 0.035), lens=85, fstop=4.5)
    shot(camw, "views-weight", (1600, 1067))

# =====================================================================
elif SCENE == "simple":
    world("#FFFFFF", 0.25)
    white = simple_mat("White cyc", "#F3F2EF", 0.9)
    sweep("Cyc", 5.0, 3.0, 2.0, 0.5, white, y0=0.9)
    area("Soft", (0.0, -0.4, 1.2), CTR, 1.6, 22)
    area("Side", (-0.9, -0.3, 0.5), CTR, 1.0, 12)
    area("Side2", (0.9, -0.3, 0.5), CTR, 1.0, 2)
    cam = camera("plan", (0, -0.16, 1.0), (0, 0.0, 0.0), lens=85, ortho=0.42)
    shot(cam, "03-simple", (2400, 1600))
