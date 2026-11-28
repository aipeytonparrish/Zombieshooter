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


# The scene's "Buildings out of scene" are rough low-poly shells (flat shard triangles) meant to stay off camera; in a
# playable street they sit right along the play area. They are removed and L_NYC puts Megascans kitbash buildings there.
REMOVE = ("NY_Buildingsoutofscene",)


def tune_art_level():
    """Distant backdrop meshes (skyline, far buildings, the 700 m river ground) don't cast shadows: they never
    shadow the play area and huge Nanite shadow casters made virtual shadow maps hang the GPU. Removes REMOVE."""
    les.load_level(ART_MAP)
    n = 0
    removed = 0
    for a in eas.get_all_level_actors():
        if not isinstance(a, unreal.StaticMeshActor):
            continue
        if a.get_actor_label().startswith(REMOVE):
            eas.destroy_actor(a)
            removed += 1
            continue
        _, ext = a.get_actor_bounds(False)
        if a.get_actor_label().startswith(BACKGROUND) or max(ext.x, ext.y) > 15000:
            a.static_mesh_component.set_editor_property("cast_shadow", False)
            n += 1
    les.save_current_level()
    zs.log("shadow casting off on %d backdrop actors, removed %d placeholder shells" % (n, removed))


# Materials whose textures are missing from the scene's texture folder (or were procedural in Blender) import as flat
# white: the brownstone windows ("glass" + "room" interiors), cables, tanks... Give them plausible flat looks.
#   name: (base colour, metallic, roughness, emissive or None)
MATERIAL_FIXES = {
    "glass": ((0.015, 0.018, 0.022), 0.6, 0.08, None),            # dark, glossy window panes
    "room": ((0.012, 0.011, 0.010), 0.0, 0.9, (0.004, 0.0035, 0.003)),   # dim rooms behind the windows
    "Material_037": ((0.02, 0.022, 0.025), 0.4, 0.2, None),
    "Material_052": ((0.02, 0.022, 0.025), 0.4, 0.2, None),
    "Material_074": ((0.08, 0.08, 0.08), 0.5, 0.5, None),
    "cables": ((0.015, 0.015, 0.015), 0.0, 0.6, None),
    "cables_001": ((0.015, 0.015, 0.015), 0.0, 0.6, None),
    "cables_002": ((0.015, 0.015, 0.015), 0.0, 0.6, None),
    "cables_003": ((0.015, 0.015, 0.015), 0.0, 0.6, None),
    "Rusted_Blue__001": ((0.08, 0.11, 0.14), 0.5, 0.7, None),
    "Rusted_Blue__002": ((0.08, 0.11, 0.14), 0.5, 0.7, None),
    "Beige_Plastic_001": ((0.42, 0.38, 0.3), 0.0, 0.6, None),
    "Beige_Plastic_002": ((0.42, 0.38, 0.3), 0.0, 0.6, None),
    "RustyWaterStorageTank": ((0.22, 0.11, 0.05), 0.6, 0.75, None),
    "LAMP_LIGHT": ((0.6, 0.55, 0.45), 0.0, 0.4, (0.6, 0.45, 0.25)),
    # Unlit "rooms behind the window" pictures: daylight photos glowing at full brightness on a stormy day.
    "wP_Rooms_v1_R00_S_day_1k": ((0.08, 0.08, 0.085), 0.0, 0.9, None),
}


def fix_materials():
    mel = unreal.MaterialEditingLibrary
    base = DEST + "/PostApocNY/Materials/"
    n = 0
    for name, (color, metal, rough, emissive) in MATERIAL_FIXES.items():
        m = unreal.load_asset(base + name)
        if not isinstance(m, unreal.MaterialInstanceConstant):
            continue
        mel.set_material_instance_vector_parameter_value(m, "BaseColorFactor", unreal.LinearColor(color[0], color[1], color[2], 1))
        mel.set_material_instance_scalar_parameter_value(m, "MetallicFactor", metal)
        mel.set_material_instance_scalar_parameter_value(m, "RoughnessFactor", rough)
        if emissive:
            mel.set_material_instance_vector_parameter_value(m, "EmissiveFactor", unreal.LinearColor(emissive[0], emissive[1], emissive[2], 1))
        zs.eal.save_asset(base + name, only_if_is_dirty=False)
        n += 1
    zs.log("fixed %d untextured materials" % n)
    fix_rubble()


RUBBLE_TEX = "/Game/MS_NewYorkCityKitbash_Assets/Textures/T_Concrete03_%s"


def fix_rubble():
    """The scene's debris piles rendered as near-black shards (the glTF base material multiplies by their vertex colours,
    which are near black). Re-parent their material to the Megascans kitbash concrete, which ignores vertex colour."""
    mel = unreal.MaterialEditingLibrary
    concrete = unreal.load_asset("/Game/MS_NewYorkCityKitbash_Assets/Materials/M_Concrete03")
    color = unreal.load_asset(RUBBLE_TEX % "Base_color")
    normal = unreal.load_asset(RUBBLE_TEX % "Normal_OpenGL")
    white = unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture")
    if not color:
        zs.log("WARNING: kitbash concrete textures missing - rubble left as imported")
        return
    n = 0
    for path in zs.eal.list_assets(DEST + "/PostApocNY/Materials", recursive=False, include_folder=False):
        if "debrispilelowres" not in path:
            continue
        m = unreal.load_asset(path)
        if not isinstance(m, unreal.MaterialInstanceConstant):
            continue
        if concrete:
            mel.set_material_instance_parent(m, concrete)
            zs.eal.save_asset(path, only_if_is_dirty=False)
            n += 1
            continue
        mel.set_material_instance_texture_parameter_value(m, "BaseColorTexture", color)
        if normal:
            mel.set_material_instance_texture_parameter_value(m, "NormalTexture", normal)
        mel.set_material_instance_texture_parameter_value(m, "MetallicRoughnessTexture", white)
        mel.set_material_instance_texture_parameter_value(m, "OcclusionTexture", white)
        mel.set_material_instance_vector_parameter_value(m, "BaseColorFactor", unreal.LinearColor(0.62, 0.6, 0.57, 1))
        mel.set_material_instance_scalar_parameter_value(m, "MetallicFactor", 0.0)
        mel.set_material_instance_scalar_parameter_value(m, "RoughnessFactor", 1.0)
        mel.set_material_instance_vector_parameter_value(m, "BaseColorTexture_OffsetScale", unreal.LinearColor(0, 0, 3, 3))
        mel.set_material_instance_vector_parameter_value(m, "NormalTexture_OffsetScale", unreal.LinearColor(0, 0, 3, 3))
        zs.eal.save_asset(path, only_if_is_dirty=False)
        n += 1
    zs.log("rubble materials fixed: %d" % n)


if POST_ONLY:
    postprocess()
    tune_art_level()
    fix_materials()
    raise SystemExit(0)
if "--tune-only" in sys.argv:
    tune_art_level()
    raise SystemExit(0)
if "--materials-only" in sys.argv:
    fix_materials()
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
fix_materials()
