"""Make the Megascans NYC kitbash buildings (/Game/MS_NewYorkCityKitbash_Assets/Meshes-Buildings, the owner's pack,
git-ignored) usable as level art: Nanite on (they are 0.1 - 1.9 M triangles each) and their own triangles as collision
(complex as simple), so they can line the play area. Re-run after re-importing the pack.

  UnrealEditor-Cmd.exe Zombieshooter.uproject -run=pythonscript -script=Tools/Import/prepare_kitbash_buildings.py
"""
import os
import sys

import unreal

sys.path.append(os.path.dirname(__file__))
import zs_import_common as zs  # noqa: E402

ROOT = "/Game/MS_NewYorkCityKitbash_Assets/Meshes-Buildings"

n = 0
for path in zs.eal.list_assets(ROOT, recursive=True, include_folder=False):
    sm = zs.eal.load_asset(path)
    if not isinstance(sm, unreal.StaticMesh):
        continue
    nanite = sm.get_editor_property("nanite_settings")
    nanite.enabled = True
    sm.set_editor_property("nanite_settings", nanite)
    body = sm.get_editor_property("body_setup")
    if body:
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    unreal.EditorStaticMeshLibrary.remove_collisions_with_notification(sm, True)
    zs.eal.save_asset(path, only_if_is_dirty=False)
    n += 1
zs.log("kitbash buildings prepared (Nanite + complex collision): %d" % n)
