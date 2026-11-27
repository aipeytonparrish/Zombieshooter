"""Let the owner's industrial kit materials render on instanced meshes (AZSKitInstances draws the compound with ISMs;
without bUsedWithInstancedStaticMeshes a material falls back to a flat default). Run after import_industrial_kit.py.

  UnrealEditor-Cmd.exe Zombieshooter.uproject -run=pythonscript -script=Tools/Import/kit_instancing_flags.py
"""
import os
import sys

import unreal

sys.path.append(os.path.dirname(__file__))
import zs_import_common as zs  # noqa: E402

done = set()
for path in zs.eal.list_assets("/Game/IndustrialKit", recursive=True, include_folder=False):
    asset = zs.eal.load_asset(path)
    if not isinstance(asset, unreal.StaticMesh):
        continue
    for sm in asset.get_editor_property("static_materials"):
        mi = sm.get_editor_property("material_interface")
        if not mi:
            continue
        base = mi.get_base_material()
        key = base.get_path_name()
        if key in done:
            continue
        done.add(key)
        if not key.startswith("/Game/"):
            # Engine / plugin parents (the kit's glTF materials use /InterchangeAssets/gltf/M_Default): never write
            # engine content; the editor sets the usage flag on those by itself when an instanced mesh needs it.
            zs.log("skipping engine material " + key)
            continue
        if not base.get_editor_property("used_with_instanced_static_meshes"):
            base.set_editor_property("used_with_instanced_static_meshes", True)
            unreal.MaterialEditingLibrary.recompile_material(base)
            zs.eal.save_loaded_asset(base)
zs.log("kit materials flagged for instancing: %d" % len(done))
