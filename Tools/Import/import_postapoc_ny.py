"""Import the owner's post-apocalyptic New York scene (glTF exported by blender_export_postapoc_ny.py) into
/Game/NYC_PostApoc (git-ignored: derived from a third-party asset) and place it in the art level
/Game/NYC_PostApoc/Maps/L_NYC_Art, which our gameplay map L_NYC streams in.

Static meshes get Nanite and use their own triangles for collision (complex as simple), so the player and the
zombie navmesh can walk on the street, debris and wrecks.

  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=Tools/Import/import_postapoc_ny.py -- <path to PostApocNY.gltf>
"""
import os
import sys
import unreal

sys.path.append(os.path.dirname(__file__))
import zs_import_common as zs  # noqa: E402

DEFAULT_SRC = os.path.join(os.path.expanduser("~"), "Desktop", "Enviorments Assets", "_extract", "PostApocNY", "export", "PostApocNY.gltf")
args = [a for a in sys.argv[1:] if a.lower().endswith(".gltf")]
SRC = args[0] if args else DEFAULT_SRC
DEST = "/Game/NYC_PostApoc"
ART_MAP = DEST + "/Maps/L_NYC_Art"

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

POST_ONLY = "--postprocess-only" in sys.argv


def postprocess():
    """Complex-as-simple collision on every imported mesh, then force the physics data to rebuild (setting the flag
    alone leaves stale cooked collision, and traces miss)."""
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
    zs.log("collision post-processed on %d meshes" % count)


BACKGROUND = ("NY_City", "NY_CityscapeAcrossRiver", "NY_Buildingsoutofscene", "NY_BuildingBG", "NY_Bridge")


def tune_art_level():
    """Distant backdrop meshes (skyline, far buildings, the 700 m river ground) don't cast shadows: they never
    shadow the play area and huge Nanite shadow casters made virtual shadow maps hang the GPU."""
    les.load_level(ART_MAP)
    n = 0
    for a in eas.get_all_level_actors():
        if not isinstance(a, unreal.StaticMeshActor):
            continue
        _, ext = a.get_actor_bounds(False)
        if a.get_actor_label().startswith(BACKGROUND) or max(ext.x, ext.y) > 15000:
            a.static_mesh_component.set_editor_property("cast_shadow", False)
            n += 1
    les.save_current_level()
    zs.log("shadow casting off on %d backdrop actors" % n)


if POST_ONLY:
    postprocess()
    tune_art_level()
    raise SystemExit(0)

if not os.path.exists(SRC):
    raise RuntimeError("glTF not found: " + SRC)

content = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
umap = os.path.join(content, "NYC_PostApoc", "Maps", "L_NYC_Art.umap")
if zs.eal.does_asset_exist(ART_MAP):
    zs.eal.delete_asset(ART_MAP)
if os.path.exists(umap):
    os.remove(umap)
if not les.new_level(ART_MAP):
    raise RuntimeError("new_level failed for " + ART_MAP)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()

# Pipelines: glTF material conversion + generic assets (Nanite on) + level (spawn actors into this level).
pipes = []
try:
    pipes.append(unreal.InterchangeGLTFPipeline())
except Exception as e:  # noqa: BLE001
    zs.log("no glTF pipeline: %s" % e)
assets = unreal.InterchangeGenericAssetsPipeline()
mesh = assets.get_editor_property("mesh_pipeline")
mesh.set_editor_property("build_nanite", True)
mesh.set_editor_property("import_skeletal_meshes", False)
mesh.set_editor_property("generate_lightmap_u_vs", False)
mesh.set_editor_property("collision", False)
pipes.append(assets)
pipes.append(unreal.InterchangeGenericLevelPipeline())

params = unreal.ImportAssetParameters()
params.is_automated = True
params.replace_existing = True
params.import_level = world.get_current_level() if hasattr(world, "get_current_level") else None
try:
    params.override_pipelines = pipes
except Exception as e:  # noqa: BLE001
    zs.log("override_pipelines not accepted (%s); using the project's default scene pipelines" % e)

mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
src = unreal.InterchangeManager.create_source_data(SRC)
zs.log("Importing scene " + SRC)
ok = mgr.import_scene(DEST, src, params)
mgr.wait_until_all_tasks_done(True)
zs.log("import_scene returned %s" % ok)
# A commandlet does not save the new packages by itself (textures and materials would be lost).
zs.eal.save_directory(DEST, only_if_is_dirty=False, recursive=True)

postprocess()

actors = eas.get_all_level_actors()
zs.log("actors in art level: %d" % len(actors))
les.save_current_level()
zs.log("Saved " + ART_MAP)
tune_art_level()
