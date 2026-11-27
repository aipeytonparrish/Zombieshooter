"""Build /Game/ZS/Maps/L_NYC: the New York survival level. Ours (versioned): weather, lighting, gameplay actors.
The art (the owner's post-apocalyptic NY scene, imported by import_postapoc_ny.py into the git-ignored
/Game/NYC_PostApoc) is streamed in from /Game/NYC_PostApoc/Maps/L_NYC_Art.

Weather: a grey, rainy afternoon - overcast storm clouds, a dim soft sun, light fog off the river, and AZSRain
following the camera. Gameplay: player start, zombie spawn points around the edges of the street, unlimited ammo
crates, invisible walls around the play area and a NavMesh volume. Re-run to rebuild.
"""
import os
import sys
import unreal

sys.path.append(os.path.dirname(__file__))
import zs_import_common as zs  # noqa: E402

MAP = "/Game/ZS/Maps/L_NYC"
ART = "/Game/NYC_PostApoc/Maps/L_NYC_Art"
CLOUD_MI = "/Game/ZS/Materials/MI_ZS_OvercastClouds"

# ---- Play area (UE units). Picked from an in-game height map (console: ZSHeightMap -7000 5000 -9000 9000 250):
# the street runs along Y between the river (west, x < -1500) and a facade line at x = 3250. ~42 x 120 m.
PLAY_MIN = unreal.Vector2D(-1200, -6000)
PLAY_MAX = unreal.Vector2D(3000, 6000)
GROUND_Z = -100
PLAYER_START = (750, 3250, 60, -90)   # x, y, z, yaw: north end, looking down the street
# Flat street cells (x, y, floor z) around the edges of the play area.
SPAWNS = [(-1000, -1500, -50), (1500, -5750, -40), (2750, 0, -110), (2750, 1500, -110), (2750, 3000, -110),
          (2500, 4500, -110), (0, 5500, -110), (-500, 3750, -110), (-500, 1750, -30), (-500, 250, -30),
          (1250, -3000, -40), (2250, -2500, -110), (1000, -4250, -30), (1750, -5750, -40)]
CRATES = [(-250, 0, -30, 90), (2000, 2750, -110, 0), (1000, -4000, -40, 90)]   # x, y, floor z, yaw

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
mel = unreal.MaterialEditingLibrary


def make_cloud_material():
    """Overcast storm clouds: a child of the engine's simple volumetric cloud with more coverage and storm albedo."""
    if zs.eal.does_asset_exist(CLOUD_MI):
        zs.eal.delete_asset(CLOUD_MI)
    parent = unreal.load_asset("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst")
    mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(CLOUD_MI.split("/")[-1], "/Game/ZS/Materials", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    mel.set_material_instance_scalar_parameter_value(mi, "Cloud_GlobalCoverage", 0.55)
    mel.set_material_instance_scalar_parameter_value(mi, "Cloud_GlobalDensity", 0.02)
    mel.set_material_instance_scalar_parameter_value(mi, "StormClouds", 0.75)
    zs.eal.save_asset(CLOUD_MI)
    return mi


cloud_mi = make_cloud_material()

if zs.eal.does_asset_exist(MAP):
    zs.eal.delete_asset(MAP)
umap = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()), "ZS", "Maps", "L_NYC.umap")
if os.path.exists(umap):
    os.remove(umap)
if not les.new_level_from_template(MAP, "/Engine/Maps/Templates/Template_Default"):
    raise RuntimeError("new_level_from_template failed")
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()

# ---- Weather / lighting: reuse the template's sky actors ----
for a in eas.get_all_level_actors():
    label = a.get_actor_label()
    if label == "Floor" or label.startswith("SM_Template_Map_Floor"):
        eas.destroy_actor(a)
    elif isinstance(a, unreal.PlayerStart):
        x, y, z, yaw = PLAYER_START
        a.set_actor_location(unreal.Vector(x, y, z), False, False)
        a.set_actor_rotation(unreal.Rotator(0, 0, yaw), False)
    elif isinstance(a, unreal.DirectionalLight):
        a.set_actor_rotation(unreal.Rotator(-58, 35, 0), False)
        lc = a.light_component
        lc.set_intensity(6.0)                       # overcast: weak sun (template default is 10)
        lc.set_light_color(unreal.LinearColor(0.82, 0.86, 0.95, 1))
        lc.set_editor_property("light_source_angle", 12.0)   # very soft shadows
        lc.set_editor_property("cast_cloud_shadows", True)
    elif isinstance(a, unreal.SkyLight):
        sc = a.light_component
        sc.set_editor_property("real_time_capture", True)
        sc.set_intensity(1.4)
    elif isinstance(a, unreal.VolumetricCloud):
        cc = a.get_component_by_class(unreal.VolumetricCloudComponent)
        cc.set_editor_property("material", cloud_mi)
        cc.set_editor_property("layer_bottom_altitude", 0.9)
        cc.set_editor_property("layer_height", 6.0)
    elif isinstance(a, unreal.ExponentialHeightFog):
        a.set_actor_location(unreal.Vector(0, 0, GROUND_Z - 300), False, False)
        fc = a.component
        fc.set_editor_property("fog_density", 0.035)
        fc.set_editor_property("fog_height_falloff", 0.35)
        fc.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(0.22, 0.24, 0.27, 1))
        fc.set_editor_property("start_distance", 400.0)
        fc.set_editor_property("fog_max_opacity", 0.92)
        fc.set_editor_property("enable_volumetric_fog", True)
        fc.set_editor_property("volumetric_fog_extinction_scale", 1.2)

# Grey, desaturated, cool grade with gentle vignette (auto exposure left at the project default).
ppv = eas.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 300), unreal.Rotator(0, 0, 0))
ppv.set_actor_label("PPV_RainyDay")
ppv.set_editor_property("unbound", True)
s = ppv.get_editor_property("settings")
for prop, val in [
    ("color_saturation", unreal.Vector4(0.82, 0.84, 0.88, 1.0)),
    ("color_gain", unreal.Vector4(0.95, 0.98, 1.04, 1.0)),
    ("color_contrast", unreal.Vector4(1.05, 1.05, 1.05, 1.0)),
    ("vignette_intensity", 0.5), ("bloom_intensity", 0.5),
    ("auto_exposure_bias", -1.0),
]:
    s.set_editor_property("override_" + prop, True)
    s.set_editor_property(prop, val)
ppv.set_editor_property("settings", s)

rain = eas.spawn_actor_from_class(unreal.ZSRain, unreal.Vector(0, 0, 500), unreal.Rotator(0, 0, 0))
rain.set_actor_label("ZSRain")
rain.set_editor_property("drop_count", 4000)
rain.set_editor_property("opacity", 0.35)
rain.set_editor_property("color", unreal.LinearColor(0.5, 0.53, 0.58, 1))

# ---- Gameplay ----
cx = (PLAY_MIN.x + PLAY_MAX.x) * 0.5
cy = (PLAY_MIN.y + PLAY_MAX.y) * 0.5
ex = (PLAY_MAX.x - PLAY_MIN.x) * 0.5
ey = (PLAY_MAX.y - PLAY_MIN.y) * 0.5
nav = eas.spawn_actor_from_class(unreal.NavMeshBoundsVolume, unreal.Vector(cx, cy, GROUND_Z + 200), unreal.Rotator(0, 0, 0))
nav.set_actor_scale3d(unreal.Vector(ex / 100.0 + 2, ey / 100.0 + 2, 15))   # default brush is 200 cm
nav.set_actor_label("NavMeshBounds")

# Invisible walls around the play area (block the player and zombies, shape the navmesh).
cube = unreal.load_object(None, "/Engine/BasicShapes/Cube.Cube")
for i, (x, y, sx, sy) in enumerate([
    (cx, PLAY_MIN.y - 50, ex * 2 + 200, 100), (cx, PLAY_MAX.y + 50, ex * 2 + 200, 100),
    (PLAY_MIN.x - 50, cy, 100, ey * 2 + 200), (PLAY_MAX.x + 50, cy, 100, ey * 2 + 200),
]):
    w = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, GROUND_Z + 1500), unreal.Rotator(0, 0, 0))
    w.static_mesh_component.set_static_mesh(cube)
    w.set_actor_scale3d(unreal.Vector(sx / 100.0, sy / 100.0, 40))
    w.set_actor_hidden_in_game(True)
    w.static_mesh_component.set_editor_property("cast_shadow", False)
    w.set_actor_label("PlayAreaWall_%d" % i)

for i, (x, y, z) in enumerate(SPAWNS):
    sp = eas.spawn_actor_from_class(unreal.ZSSpawnPoint, unreal.Vector(x, y, z + 95), unreal.Rotator(0, 0, 0))
    sp.set_actor_label("ZSSpawnPoint_%02d" % i)
for i, (x, y, z, yaw) in enumerate(CRATES):
    c = eas.spawn_actor_from_class(unreal.ZSAmmoCrate, unreal.Vector(x, y, z), unreal.Rotator(0, 0, yaw))
    c.set_actor_label("ZSAmmoCrate_%02d" % i)

ws = world.get_world_settings()
ws.set_editor_property("default_game_mode", unreal.ZSGameMode_Survival)

if zs.eal.does_asset_exist(ART):
    unreal.EditorLevelUtils.add_level_to_world(world, ART, unreal.LevelStreamingAlwaysLoaded)
    les.set_current_level_by_name("L_NYC")
    zs.log("NYC art level streamed in: " + ART)
else:
    zs.log("WARNING: " + ART + " missing - run import_postapoc_ny.py (the map will be empty)")

les.save_current_level()
zs.log("Saved " + MAP)
