"""Blender (5.x) script: prepare the owner's "Post-apocalyptic NY" render scene for Unreal and export it as glTF.

Run headless (auto-run scripts in the .blend stay disabled):
  blender -b --disable-autoexec <path>/Post-apocalyptic_NY.blend --python Tools/Import/blender_export_postapoc_ny.py -- <textures dir> <out dir>

What it does (the .blend itself is never saved):
  - relinks the textures (the file points at the original author's PC) to the shipped textures folder;
  - drops render-only parts: the sky dome, GScatter grass systems and other particle systems, the camera;
  - makes collection instances (trees) real and converts curves (power lines) to meshes;
  - decimates very heavy meshes (debris piles are 312k tris each) to a game budget;
  - applies all modifiers and merges meshes per collection and 25 m grid cell (15k objects -> a few hundred), so
    Unreal gets a manageable number of (Nanite) meshes;
  - exports glTF (separate .bin + textures).
"""
import os
import sys
import bpy

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
TEX = argv[0] if len(argv) > 0 else ""
OUT = argv[1] if len(argv) > 1 else os.path.join(os.path.dirname(bpy.data.filepath), "export")
MAX_TRIS = 1500000         # per mesh after decimation (Nanite renders the rest at full detail)
DROP_COLLECTIONS = {"Background Sky", "GScatter"}

os.makedirs(OUT, exist_ok=True)


def log(*a):
    print("ZSX", *a, flush=True)


# ---- Relink textures ----
if TEX:
    bpy.ops.file.find_missing_files(directory=TEX, find_all=True)
missing = [i.name for i in bpy.data.images if i.source == 'FILE' and not i.packed_file and not os.path.exists(bpy.path.abspath(i.filepath))]
log("images missing after relink:", len(missing), missing[:20])

# ---- Drop render-only content ----
def all_objects(coll):
    objs = set(coll.objects)
    for ch in coll.children:
        objs |= all_objects(ch)
    return objs


scene = bpy.context.scene
drop = set()
for c in bpy.data.collections:
    if c.name in DROP_COLLECTIONS:
        drop |= all_objects(c)
drop |= {o for o in scene.objects if o.type in ('CAMERA', 'LIGHT')}
for o in drop:
    bpy.data.objects.remove(o, do_unlink=True)
log("dropped objects:", len(drop))

for o in scene.objects:
    for m in [m for m in o.modifiers if m.type == 'PARTICLE_SYSTEM']:
        o.modifiers.remove(m)

# ---- Make instances real, curves to meshes ----
bpy.ops.object.select_all(action='DESELECT')
for o in scene.objects:
    if o.type == 'EMPTY' and o.instance_type == 'COLLECTION' and o.instance_collection:
        o.select_set(True)
if bpy.context.selected_objects:
    bpy.context.view_layer.objects.active = bpy.context.selected_objects[0]
    bpy.ops.object.duplicates_make_real(use_base_parent=True, use_hierarchy=True)
bpy.ops.object.select_all(action='DESELECT')
curves = [o for o in scene.objects if o.type == 'CURVE' and o.visible_get()]
for o in curves:
    o.select_set(True)
if curves:
    bpy.context.view_layer.objects.active = curves[0]
    bpy.ops.object.convert(target='MESH')
log("curves converted:", len(curves))

# ---- Decimate heavy meshes ----
dec = 0
for o in scene.objects:
    if o.type != 'MESH' or not o.data:
        continue
    tris = sum(len(p.vertices) - 2 for p in o.data.polygons)
    if tris > MAX_TRIS:
        m = o.modifiers.new("ZS_Decimate", 'DECIMATE')
        m.ratio = max(0.02, MAX_TRIS / tris)
        dec += 1
log("decimated meshes:", dec)

# ---- Apply modifiers and merge by collection + grid cell ----
import math
from collections import defaultdict

BG_COLLECTIONS = {"Buildings out of scene", "Cityscape Across River", "Building BG", "Building BG 1", "Building BG 2", "Bridge"}


def top_collection(o):
    c = o.users_collection[0] if o.users_collection else scene.collection
    # walk up to the child of the scene collection
    parent_of = {}
    for pc in [scene.collection] + list(bpy.data.collections):
        for ch in pc.children:
            parent_of[ch.name] = pc
    while c.name in parent_of and parent_of[c.name] != scene.collection:
        c = parent_of[c.name]
    return c.name


bpy.ops.object.select_all(action='DESELECT')
meshes = [o for o in scene.objects if o.type == 'MESH' and o.visible_get()]
for o in meshes:
    o.select_set(True)
bpy.context.view_layer.objects.active = meshes[0]
bpy.ops.object.make_single_user(object=True, obdata=True)
bpy.ops.object.convert(target='MESH')   # applies modifiers (decimate, geometry nodes, edge split...)
groups = defaultdict(list)
for o in [o for o in scene.objects if o.type == 'MESH' and o.visible_get()]:
    top = top_collection(o)
    cell = 100.0 if top in BG_COLLECTIONS else 25.0
    w = o.matrix_world.translation
    groups[(top, math.floor(w.x / cell), math.floor(w.y / cell))].append(o)
merged = 0
for key, objs in groups.items():
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    if len(objs) > 1:
        bpy.ops.object.join()
    ob = bpy.context.view_layer.objects.active
    ob.name = "NY_%s_%d_%d" % (key[0].replace(" ", ""), key[1], key[2])
    mw = ob.matrix_world.copy()
    ob.parent = None
    ob.matrix_world = mw
    merged += 1
# empties left over from hierarchies are not needed
for o in [o for o in scene.objects if o.type == 'EMPTY']:
    bpy.data.objects.remove(o, do_unlink=True)
log("merged into", merged, "meshes")

# ---- Export ----
path = os.path.join(OUT, "PostApocNY.gltf")
bpy.ops.export_scene.gltf(
    filepath=path,
    export_format='GLTF_SEPARATE',
    export_texture_dir="textures",
    export_apply=True,
    export_yup=True,
    export_cameras=False,
    export_lights=False,
    export_animations=False,
    export_extras=False,
    use_visible=True,
)
meshes = [o for o in scene.objects if o.type == 'MESH' and o.visible_get()]
log("exported", path, "visible meshes:", len(meshes))
