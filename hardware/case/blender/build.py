# Blender (headless): import the tessellated assembly, assign materials, save base.blend
import bpy, bmesh, json, os, re, sys, math
S = os.path.dirname(os.path.abspath(__file__))
MESH = os.environ.get("MESH", os.path.join(S, "mesh"))
ACCENT = os.environ.get("ACCENT", "orange")

bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene
sc.unit_settings.scale_length = 1.0

def srgb(h):
    h = h.lstrip("#"); c = [int(h[i:i+2], 16) / 255 for i in (0, 2, 4)]
    return [x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c] + [1]

def mat(name, col, rough=0.5, metal=0.0, **kw):
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = srgb(col) if isinstance(col, str) else col
    b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metal
    for k, v in kw.items():
        b.inputs[k].default_value = v
    return m

def objm(nt, tc):
    """Object coordinates in metres (the STL meshes are in millimetres with a 0.001 object scale)."""
    v = nt.nodes.new("ShaderNodeVectorMath"); v.operation = "SCALE"; v.inputs["Scale"].default_value = 0.001
    nt.links.new(tc.outputs["Object"], v.inputs[0]); return v.outputs[0]

def add_bump(m, scale=900.0, strength=0.06, dist=0.0002, rough_var=None):
    nt = m.node_tree; b = nt.nodes["Principled BSDF"]
    tc = nt.nodes.new("ShaderNodeTexCoord")
    n = nt.nodes.new("ShaderNodeTexNoise"); n.inputs["Scale"].default_value = scale
    n.inputs["Detail"].default_value = 8
    nt.links.new(objm(nt, tc), n.inputs["Vector"])
    bp = nt.nodes.new("ShaderNodeBump"); bp.inputs["Strength"].default_value = strength
    bp.inputs["Distance"].default_value = dist
    nt.links.new(n.outputs["Fac"], bp.inputs["Height"])
    nt.links.new(bp.outputs["Normal"], b.inputs["Normal"])

accent_hex = "#E8763E" if ACCENT == "orange" else "#2F6B3A"
M = {
    "ecoat": mat("Ecoat warm light grey", "#D9D4CB", 0.62, 0.0, **{"Coat Weight": 0.08, "Coat Roughness": 0.5}),
    "accent": mat("Anodised accent", accent_hex, 0.42, 0.45),
    "knob": mat("Anodised knob", accent_hex, 0.42, 0.5),
    "brass": mat("Polished brass", "#E9C27A", 0.07, 1.0),
    "steel": mat("Stainless steel", "#C9CBCC", 0.22, 1.0),
    "plate": mat("Frosted polycarbonate", "#F2F2EE", 0.42, 0.0, **{"Transmission Weight": 0.92, "IOR": 1.58}),
    "pcb": mat("PCB black mask", "#151716", 0.45, 0.0),
    "copper": mat("Connector metal", "#BFC2C4", 0.3, 1.0),
    "switch": mat("Switch housing", "#2A2826", 0.4, 0.0),
    "switchtop": mat("Switch top smoke", "#3A3A3C", 0.25, 0.0, **{"Transmission Weight": 0.5}),
    "rubber": mat("Rubber (white)", "#E9E6DF", 0.8, 0.0, **{"Subsurface Weight": 0.1}),
    "paper": mat("E-paper", "#CFCBC2", 0.85, 0.0),
    "guide": mat("Light guide", "#F4F4F0", 0.3, 0.0, **{"Transmission Weight": 0.85}),
    "plastic": mat("Dark plastic", "#222222", 0.5, 0.0),
    "battery": mat("Battery pouch", "#A8A9AA", 0.35, 0.8),
    # GMK Kaiju-style doubleshot ABS: cream alphas, warm grey mods, red-orange accents
    "cap_alpha": mat("Kaiju cream", "#E7DDC7", 0.38, 0.0),
    "cap_mod": mat("Kaiju grey", "#77736D", 0.38, 0.0),
    "cap_accent": mat("Kaiju red-orange", "#E2502A", 0.38, 0.0),
    "leg_dark": mat("Legend charcoal", "#2C2A29", 0.38, 0.0),
    "leg_cream": mat("Legend cream", "#EFE7D6", 0.38, 0.0),
}
add_bump(M["ecoat"], 1600, 0.05)
add_bump(M["accent"], 2500, 0.03)
add_bump(M["plate"], 2000, 0.15)
for k in ("cap_alpha", "cap_mod", "cap_accent", "knob"):
    add_bump(M[k], 2600, 0.02)  # faint moulding / blasting texture

def add_dust(m, scale=1100.0, thresh=0.993, col="#DCD8D0"):
    """Sparse sub-millimetre specks: a random few Voronoi cells get a dusty, rough dot."""
    nt = m.node_tree; b = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
    tc = nt.nodes.new("ShaderNodeTexCoord")
    v = nt.nodes.new("ShaderNodeTexVoronoi"); v.inputs["Scale"].default_value = scale
    nt.links.new(objm(nt, tc), v.inputs["Vector"])
    sep = nt.nodes.new("ShaderNodeSeparateColor"); nt.links.new(v.outputs["Color"], sep.inputs[0])
    pick = nt.nodes.new("ShaderNodeMath"); pick.operation = "GREATER_THAN"; pick.inputs[1].default_value = thresh
    nt.links.new(sep.outputs[0], pick.inputs[0])
    near = nt.nodes.new("ShaderNodeMath"); near.operation = "LESS_THAN"; near.inputs[1].default_value = 0.2
    nt.links.new(v.outputs["Distance"], near.inputs[0])
    mask = nt.nodes.new("ShaderNodeMath"); mask.operation = "MULTIPLY"
    nt.links.new(pick.outputs[0], mask.inputs[0]); nt.links.new(near.outputs[0], mask.inputs[1])
    for inp, val in (("Base Color", srgb(col)), ("Roughness", 0.95)):
        src = b.inputs[inp]; mix = nt.nodes.new("ShaderNodeMix")
        mix.data_type = "RGBA" if inp == "Base Color" else "FLOAT"
        ia, ib = (6, 7) if inp == "Base Color" else (2, 3)
        if src.is_linked: nt.links.new(src.links[0].from_socket, mix.inputs[ia])
        else: mix.inputs[ia].default_value = src.default_value
        mix.inputs[ib].default_value = val
        nt.links.new(mask.outputs[0], mix.inputs[0])
        out = mix.outputs[2] if inp == "Base Color" else mix.outputs[0]
        nt.links.new(out, src)

for k in ("ecoat", "cap_alpha", "cap_mod", "cap_accent", "accent", "knob", "leg_dark", "leg_cream"):
    add_dust(M[k])

# Fingerprints on the brass weight's face: whorl ridges inside soft ovals raise the roughness and dull the colour
def fingerprints(m, prints, ridge=0.00046, rough_add=0.5, base_rough=0.06):
    nt = m.node_tree; bb = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
    tc = nt.nodes.new("ShaderNodeTexCoord"); acc = None
    for (cx, cy, rot, sx, sy, amt) in prints:
        sub = nt.nodes.new("ShaderNodeVectorMath"); sub.operation = "SUBTRACT"; sub.inputs[1].default_value = (cx, cy, 0)
        nt.links.new(objm(nt, tc), sub.inputs[0])
        mp = nt.nodes.new("ShaderNodeMapping"); mp.vector_type = "POINT"
        mp.inputs["Rotation"].default_value = (0, 0, math.radians(rot))
        mp.inputs["Scale"].default_value = (0.008 / sx, 0.008 / sy, 1.0)
        nt.links.new(sub.outputs[0], mp.inputs["Vector"])
        sxy = nt.nodes.new("ShaderNodeSeparateXYZ"); nt.links.new(mp.outputs[0], sxy.inputs[0])
        cxy = nt.nodes.new("ShaderNodeCombineXYZ")
        nt.links.new(sxy.outputs[0], cxy.inputs[0]); nt.links.new(sxy.outputs[1], cxy.inputs[1])
        ln = nt.nodes.new("ShaderNodeVectorMath"); ln.operation = "LENGTH"; nt.links.new(cxy.outputs[0], ln.inputs[0])
        wob = nt.nodes.new("ShaderNodeTexNoise"); wob.inputs["Scale"].default_value = 700; wob.inputs["Detail"].default_value = 3
        nt.links.new(cxy.outputs[0], wob.inputs["Vector"])
        # loop pattern: a whorl dragged sideways (length plus a share of x), wobbled
        lx = nt.nodes.new("ShaderNodeMath"); lx.operation = "MULTIPLY_ADD"; lx.inputs[1].default_value = 0.45
        nt.links.new(sxy.outputs[0], lx.inputs[0]); nt.links.new(ln.outputs["Value"], lx.inputs[2])
        ph = nt.nodes.new("ShaderNodeMath"); ph.operation = "MULTIPLY_ADD"; ph.inputs[1].default_value = 0.0011
        nt.links.new(wob.outputs["Fac"], ph.inputs[0]); nt.links.new(lx.outputs[0], ph.inputs[2])
        k = nt.nodes.new("ShaderNodeMath"); k.operation = "MULTIPLY"; k.inputs[1].default_value = 2 * math.pi / ridge
        nt.links.new(ph.outputs[0], k.inputs[0])
        sn = nt.nodes.new("ShaderNodeMath"); sn.operation = "SINE"; nt.links.new(k.outputs[0], sn.inputs[0])
        rid = nt.nodes.new("ShaderNodeMath"); rid.operation = "GREATER_THAN"; rid.inputs[1].default_value = 0.1
        nt.links.new(sn.outputs[0], rid.inputs[0])
        fall = nt.nodes.new("ShaderNodeMapRange"); fall.inputs[1].default_value = 0.0080; fall.inputs[2].default_value = 0.0045
        nt.links.new(ln.outputs["Value"], fall.inputs[0])
        edge = nt.nodes.new("ShaderNodeTexNoise"); edge.inputs["Scale"].default_value = 150
        nt.links.new(cxy.outputs[0], edge.inputs["Vector"])
        fe = nt.nodes.new("ShaderNodeMath"); fe.operation = "MULTIPLY"
        nt.links.new(fall.outputs[0], fe.inputs[0]); nt.links.new(edge.outputs["Fac"], fe.inputs[1])
        fe2 = nt.nodes.new("ShaderNodeMath"); fe2.operation = "MULTIPLY"; fe2.inputs[1].default_value = 1.8 * amt; fe2.use_clamp = True
        nt.links.new(fe.outputs[0], fe2.inputs[0])
        one = nt.nodes.new("ShaderNodeMath"); one.operation = "MULTIPLY"
        nt.links.new(rid.outputs[0], one.inputs[0]); nt.links.new(fe2.outputs[0], one.inputs[1])
        # a faint oily haze between the ridges too
        hz = nt.nodes.new("ShaderNodeMath"); hz.operation = "MULTIPLY_ADD"; hz.inputs[1].default_value = 0.25
        nt.links.new(fe2.outputs[0], hz.inputs[0]); nt.links.new(one.outputs[0], hz.inputs[2])
        if acc is None: acc = hz.outputs[0]
        else:
            mx = nt.nodes.new("ShaderNodeMath"); mx.operation = "MAXIMUM"
            nt.links.new(acc, mx.inputs[0]); nt.links.new(hz.outputs[0], mx.inputs[1]); acc = mx.outputs[0]
    rr = nt.nodes.new("ShaderNodeMath"); rr.operation = "MULTIPLY_ADD"; rr.inputs[1].default_value = rough_add
    rr.inputs[2].default_value = base_rough; nt.links.new(acc, rr.inputs[0]); nt.links.new(rr.outputs[0], bb.inputs["Roughness"])
    dim = nt.nodes.new("ShaderNodeMix"); dim.data_type = "RGBA"; dim.blend_type = "MULTIPLY"
    nt.links.new(acc, dim.inputs[0]); dim.inputs[6].default_value = bb.inputs["Base Color"].default_value
    dim.inputs[7].default_value = (0.8, 0.76, 0.7, 1); nt.links.new(dim.outputs[2], bb.inputs["Base Color"])

# weight face: x -0.0925..0.0925, y about -0.011..0.019 (object coordinates)
fingerprints(M["brass"], [(0.052, 0.006, 25, 0.0085, 0.0105, 1.0), (-0.061, 0.002, -35, 0.0080, 0.0100, 0.9),
                          (0.010, 0.010, 80, 0.0070, 0.0090, 0.6)])
add_dust(M["brass"], 1100, 0.982)

# E-ink picture: an image mapped across the panel's bounding box (top of the image = back of the board)
ei = mat("E-ink", "#E4E1DA", 0.9, 0.0); nt = ei.node_tree; eb = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
tx = nt.nodes.new("ShaderNodeTexImage"); tx.image = bpy.data.images.load(os.path.join(S, "eink.png"))
tx.image.colorspace_settings.name = "sRGB"; tx.interpolation = "Cubic"
tcg = nt.nodes.new("ShaderNodeTexCoord"); nt.links.new(tcg.outputs["Generated"], tx.inputs["Vector"])
nt.links.new(tx.outputs["Color"], eb.inputs["Base Color"])
M["eink"] = ei
ep = ei.copy(); ep.name = "E-ink picture"
next(n for n in ep.node_tree.nodes if n.type == "TEX_IMAGE").image = bpy.data.images.load(os.path.join(S, "eink_pic.png"))
M["eink_pic"] = ep

ACCENT_KEYS = {"Esc", "Enter"}
MOD_KEYS = {"Tab", "Caps Lock", "Shift", "Shift 2", "Ctrl", "Ctrl 2", "Win", "Alt", "Fn",
            "Backspace", "Delete", "Spacebar", "↑", "←", "↓", "→",
            "F1", "F2", "F3", "F4", "F9", "F10", "F11", "F12"}

def keyname(chain, label):
    for c in chain:
        mm = re.match(r"^(.+?) \(R\d, [\d.]+u\)$", c)
        if mm: return mm.group(1).encode("latin-1", "ignore").decode("utf-8", "ignore") if "â" in mm.group(1) else mm.group(1)
    if any("Spacebar" in c for c in chain) and label.startswith(("cap", "legend")): return "Spacebar"
    return None

CASE = {"COMPOUND": "bottom", "COMPOUND001": "top", "COMPOUND002": "ring", "COMPOUND003": "enc_cover",
        "COMPOUND004": "knob", "COMPOUND005": "led_guide", "COMPOUND006": "screen_frame",
        "COMPOUND007": "switch_cap", "COMPOUND008": "weight", "COMPOUND013": "rail", "COMPOUND014": "rail"}

def classify(e):
    lab, ch = e["label"], e["chain"]; path = " ".join(ch) + " " + lab
    if lab in CASE:
        role = CASE[lab]
        return role, {"bottom": "ecoat", "top": "ecoat", "ring": "accent", "enc_cover": "knob", "knob": "knob",
                      "led_guide": "guide", "screen_frame": "plastic", "switch_cap": "accent", "weight": "brass",
                      "rail": "rubber"}[role]
    if lab.startswith("COMPOUND"): return "screw", "steel"
    k = keyname(ch, lab)
    if k is not None and (lab.startswith("cap") or lab.startswith("legend")):
        grp = "accent" if k in ACCENT_KEYS else ("mod" if k in MOD_KEYS else "alpha")
        if lab.startswith("cap"): return "keycap", "cap_" + grp
        return "legend", ("leg_dark" if grp == "alpha" else "leg_cream")
    if "Plate_PCB" in lab: return "plate", "plate"
    if lab.endswith("_PCB"): return "pcb", "pcb"
    if "Battery" in path: return "battery", "battery"
    if "E-paper" in path: return "screen", "paper"
    if "TYPE-C" in path or "PEC12R" in path or "JST" in path: return "conn", "copper"
    if "SW_Cherry" in path: return "switch", "switch"
    if "Stabilizer" in path: return "stab", "plastic"
    return "misc", "plastic"

man = json.load(open(os.path.join(MESH, "manifest.json")))
root = bpy.data.objects.new("Board", None); sc.collection.objects.link(root)
col = bpy.data.collections.new("Assembly"); sc.collection.children.link(col)
roles = {}
for e in man:
    bpy.ops.wm.stl_import(filepath=os.path.join(MESH, e["file"]), global_scale=0.001)
    o = bpy.context.selected_objects[0]
    for c in o.users_collection: c.objects.unlink(o)
    col.objects.link(o)
    role, mk = classify(e)
    if role == "screen" and (e["bb"][3] - e["bb"][0]) > 23 and (e["bb"][4] - e["bb"][1]) > 47:
        mk = "eink" if (e["bb"][3] - e["bb"][0]) > 26 else "eink_pic"  # the panel, and its picture area
    k = keyname(e["chain"], e["label"])
    o.name = (f"{role}:{k}:{e['label']}" if k else f"{role}:{e['label']}")[:60]
    o["role"] = role; o["key"] = k or ""
    o.data.materials.clear(); o.data.materials.append(M[mk])
    o.parent = root
    roles[role] = roles.get(role, 0) + 1
    bm = bmesh.new(); bm.from_mesh(o.data)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-7)
    bm.to_mesh(o.data); bm.free()
    for p in o.data.polygons: p.use_smooth = True
    o.data.set_sharp_from_angle(angle=math.radians(32))
    if role in ("knob", "keycap", "top", "bottom", "enc_cover", "weight", "legend", "ring", "plate", "screw"):
        wn = o.modifiers.new("wn", "WEIGHTED_NORMAL"); wn.keep_sharp = True; wn.mode = "FACE_AREA"
    if role == "legend":  # lift legends a hair off the cap so they never z-fight
        o.location.z += 0.00002
root.location.z = 0.001  # rubber rails stand 1 mm proud of the floor

# Status LEDs: five 3 mm holes under the screen (found in the top block's mesh); the left three glow amber
amber = bpy.data.materials.new("LED amber"); amber.use_nodes = True
ab = next(n for n in amber.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
ab.inputs["Base Color"].default_value = (1.0, 0.45, 0.06, 1); ab.inputs["Emission Color"].default_value = (1.0, 0.30, 0.015, 1)
ab.inputs["Emission Strength"].default_value = 14.0; ab.inputs["Roughness"].default_value = 0.4
off = mat("LED off (diffuser)", "#2A2724", 0.35, 0.0)
for i, x in enumerate((0.1317, 0.1381, 0.1445, 0.1509, 0.1573)):
    bpy.ops.mesh.primitive_cylinder_add(radius=0.00145, depth=0.0016, vertices=32, location=(x, -0.03175, 0.0212))
    led = bpy.context.object; led.name = f"led:{i}"; led["role"] = "led"
    led.rotation_euler.x = math.radians(4.0)
    led.data.materials.append(amber if i < 3 else off)
    for c in led.users_collection: c.objects.unlink(led)
    col.objects.link(led); led.parent = root
print("roles", roles)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(os.environ.get("OUT", S), f"base_{ACCENT}.blend"))
