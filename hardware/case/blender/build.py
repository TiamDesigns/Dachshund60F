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

def add_bump(m, scale=900.0, strength=0.06, dist=0.0002, rough_var=None):
    nt = m.node_tree; b = nt.nodes["Principled BSDF"]
    tc = nt.nodes.new("ShaderNodeTexCoord")
    n = nt.nodes.new("ShaderNodeTexNoise"); n.inputs["Scale"].default_value = scale
    n.inputs["Detail"].default_value = 8
    nt.links.new(tc.outputs["Object"], n.inputs["Vector"])
    bp = nt.nodes.new("ShaderNodeBump"); bp.inputs["Strength"].default_value = strength
    bp.inputs["Distance"].default_value = dist
    nt.links.new(n.outputs["Fac"], bp.inputs["Height"])
    nt.links.new(bp.outputs["Normal"], b.inputs["Normal"])

accent_hex = "#FF9A66" if ACCENT == "orange" else "#2F6B3A"
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
    "rubber": mat("Rubber", "#1A1A1A", 0.85, 0.0),
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
    if e["label"] == "E-paper 2.13 GDEY0213B74 with driver board005" or (role == "screen" and e["facets"] > 200 and e["bb"][5] > 24.9):
        mk = "paper"
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
print("roles", roles)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(os.environ.get("OUT", S), f"base_{ACCENT}.blend"))
