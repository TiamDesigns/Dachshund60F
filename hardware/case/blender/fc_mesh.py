import sys, os, json, FreeCAD, Import, MeshPart
src = os.environ["SRC"]; out = os.environ["OUT"]
doc = FreeCAD.newDocument("a")
Import.insert(src, "a")
def chain(o):
    names=[]; p=o
    while True:
        ins=[x for x in p.InList if x.TypeId in ("App::Part","App::Link","Part::Compound") or hasattr(x,"Group")]
        if not ins: break
        p=ins[0]; names.append(p.Label)
    return list(reversed(names))
man=[]; i=0
for o in doc.Objects:
    if not hasattr(o,"Shape") or o.TypeId in ("App::Part",) or o.Shape.isNull(): continue
    if hasattr(o,"Group") and o.Group: continue
    path=" ".join(chain(o))+" "+o.Label
    import re
    if re.search(r"SW_Hotswap|D_SOD|C_0[46]0|R_0[46]0|SOT-|Crystal|LED_SK|SW_SPST|PinHeader|Fuse_|L_0805",path): continue
    coarse=bool(re.search(r"SW_Cherry|Stabilizer",path))
    sh=o.Shape.copy(); sh.Placement=o.getGlobalPlacement()  # world placement
    if not sh.Solids and not sh.Faces: continue
    bb=sh.BoundBox
    m=MeshPart.meshFromShape(Shape=sh, LinearDeflection=(0.12 if coarse else 0.03), AngularDeflection=(0.6 if coarse else 0.25), Relative=False)
    if m.CountFacets==0: continue
    fn="p%04d.stl"%i; i+=1
    m.write(os.path.join(out,fn))
    col=None
    try: col=list(o.ViewObject.ShapeColor) if o.ViewObject else None
    except Exception: pass
    man.append(dict(file=fn,label=o.Label,chain=chain(o),bb=[bb.XMin,bb.YMin,bb.ZMin,bb.XMax,bb.YMax,bb.ZMax],facets=m.CountFacets))
json.dump(man,open(os.path.join(out,"manifest.json"),"w"),indent=1)
print("parts",len(man))
