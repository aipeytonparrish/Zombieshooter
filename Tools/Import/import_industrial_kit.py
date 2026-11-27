"""Import the owner's "Industrial Survival Horror Modular Kit" (glTF version) into /Game/IndustrialKit (git-ignored,
third-party art). Used to build the New York safehouse interior (create_nyc_map.py).

Unpack the kit zip and its Industrial_Survival_Horror_Modular_Kit_glTF.zip into a folder of their own, then:
  UnrealEditor-Cmd.exe Zombieshooter.uproject -run=pythonscript -script="Tools/Import/import_industrial_kit.py <glTF dir>"
The glTF dir is the one holding SM_*.gltf and textures/.

Every piece gets complex-as-simple collision (door and window walls need their openings to be walkable / shootable).
Kit grid: walls 2 m wide x 3 m tall, floors and ceilings 2 x 2 m, pivots at the bottom centre of the face.
"""
import glob
import os
import sys

import unreal

sys.path.append(os.path.dirname(__file__))
import zs_import_common as zs  # noqa: E402

DEST = "/Game/IndustrialKit"

args = [a for a in sys.argv[1:] if not a.startswith("-")]
SRC = " ".join(args)   # paths with spaces arrive split
if not os.path.isdir(SRC):
    raise RuntimeError("usage: import_industrial_kit.py <glTF dir> (got %r)" % SRC)

files = sorted(glob.glob(os.path.join(SRC, "SM_*.gltf")))
zs.log("Industrial kit: %d pieces from %s" % (len(files), SRC))
tasks = []
for f in files:
    t = unreal.AssetImportTask()
    t.filename = f
    t.destination_path = DEST
    t.automated = True
    t.replace_existing = True
    t.save = True
    tasks.append(t)
zs.asset_tools.import_asset_tasks(tasks)
zs.eal.save_directory(DEST, only_if_is_dirty=False, recursive=True)

count = 0
for path in zs.eal.list_assets(DEST, recursive=True, include_folder=False):
    a = zs.eal.load_asset(path)
    if not isinstance(a, unreal.StaticMesh):
        continue
    count += 1
    body = a.get_editor_property("body_setup")
    if body:
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    unreal.EditorStaticMeshLibrary.remove_collisions_with_notification(a, True)
    zs.eal.save_asset(path, only_if_is_dirty=False)
    b = a.get_bounding_box()
    unreal.log_warning("[ZSKit] %s %s min(%.0f,%.0f,%.0f) max(%.0f,%.0f,%.0f)" % (
        a.get_name(), path, b.min.x, b.min.y, b.min.z, b.max.x, b.max.y, b.max.z))
zs.log("Industrial kit: %d static meshes in %s" % (count, DEST))
