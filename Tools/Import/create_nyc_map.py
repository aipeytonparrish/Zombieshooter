"""Build /Game/ZS/Maps/L_NYC: the New York survival level. Ours (versioned): weather, lighting, gameplay actors.
The art (the owner's post-apocalyptic NY scene, imported by import_postapoc_ny.py into the git-ignored
/Game/NYC_PostApoc) is streamed in from /Game/NYC_PostApoc/Maps/L_NYC_Art.

Weather: a grey, rainy afternoon - overcast storm clouds, a dim soft sun, light fog off the river, lightning, and
AZSRain following the camera. Gameplay (round-based, start indoors, classic style): the player starts in the start room
of a big compound at the north end of the street (built from the owner's industrial modular kit by nyc_compound.py:
hallway, offices, storage, locker room, generator room, a 6 m warehouse and a walled courtyard). Buy doors (run cash)
open each area; zombies spawn outside the walls and come in through boarded windows (AZSBarricade). The street and the
courtyard are the two outdoor fight areas. Unlimited ammo crates, invisible walls and a NavMesh volume. Re-run to
rebuild.
"""
import os
import sys
import unreal

sys.path.append(os.path.dirname(__file__))
import zs_import_common as zs  # noqa: E402
import nyc_compound  # noqa: E402
import nyc_dressing  # noqa: E402

MAP = "/Game/ZS/Maps/L_NYC"
ART = "/Game/NYC_PostApoc/Maps/L_NYC_Art"
CLOUD_MI = "/Game/ZS/Materials/MI_ZS_OvercastClouds"

# ---- Play area (UE units). Picked from an in-game height map (console: ZSHeightMap -7000 5000 -9000 9000 250):
# the street runs along Y between the river (west, x < -1500) and a facade line at x = 3250. ~42 x 120 m.
PLAY_MIN = unreal.Vector2D(-1200, -6000)
PLAY_MAX = unreal.Vector2D(3000, 7950)   # the street's north end; the compound (nyc_compound.py) continues north to y 14100
GROUND_Z = -100
PLAYER_START = nyc_compound.PLAYER_START   # in the start room, facing the hallway door
# Flat street cells (x, y, floor z) around the edges of the play area.
SPAWNS = [(-1000, -1500, -50), (1500, -5750, -40), (2750, 0, -110), (2750, 1500, -110), (2750, 3000, -110),
          (2500, 4500, -110), (0, 5500, -110), (-500, 3750, -110), (-500, 1750, -30), (-500, 250, -30),
          (1250, -3000, -40), (2250, -2500, -110), (1000, -4250, -30), (1750, -5750, -40)]
# A few more out on the street near the safehouse (open once the door is bought).
SPAWNS += [(-800, 6500, -130), (-600, 7600, -150), (2600, 6800, -130), (2400, 7700, -125)]
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
unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(["/Game/ZS/Maps"], True)
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
# NavMesh over the street and the compound (x -1500..3500, y -6100..14300).
nav = eas.spawn_actor_from_class(unreal.NavMeshBoundsVolume, unreal.Vector(1000, 4100, GROUND_Z + 300), unreal.Rotator(0, 0, 0))
nav.set_actor_scale3d(unreal.Vector(25.5, 102.5, 9))   # default brush is 200 cm
nav.set_actor_label("NavMeshBounds")

# Invisible walls around the play area (block the player and zombies, shape the navmesh).
cube = unreal.load_object(None, "/Engine/BasicShapes/Cube.Cube")
for i, (x, y, sx, sy) in enumerate([
    (cx, PLAY_MIN.y - 50, ex * 2 + 200, 100),                                     # street, south end
    (PLAY_MIN.x - 50, (PLAY_MIN.y + 7700) * 0.5, 100, 7700 - PLAY_MIN.y),         # street, river side
    (PLAY_MAX.x + 50, (PLAY_MIN.y + 7700) * 0.5, 100, 7700 - PLAY_MIN.y),         # street, east facades
    (-1450, 10900, 100, 6500), (3450, 10900, 100, 6500), (1000, 14150, 5000, 100),  # around the compound
    (-1350, 7700, 300, 100), (3250, 7700, 500, 100),                               # joins
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
    sp.set_editor_property("zone", "Street")   # used once the safehouse door is bought


shelters = nyc_compound.build(eas)
rain.set_editor_property("shelter_boxes", shelters)


KB = "/Game/MS_NewYorkCityKitbash_Assets/Meshes-Buildings/%s"
KB_GROUND = -115   # building ground floor; the street is at about -110 here, the sidewalk bases sink in under it


def build_backdrop():
    """Megascans NYC kitbash buildings (prepare_kitbash_buildings.py: Nanite + collision) along the east side and the
    north end of the street, where the scene only had rough placeholder shells (removed in the art level)."""
    if not zs.eal.does_asset_exist(KB % "SM_Building12"):
        zs.log("WARNING: NYC kitbash buildings missing - no backdrop buildings")
        return

    def place(name, yaw, xmin=None, xmax=None, ymin=None, xcenter=None, shadow=True, base=True):
        """Place a building rotated by yaw (multiple of 90) so its rotated bounds meet the given edge(s).
        Returns the world (xmin, xmax, ymin, ymax) it covers."""
        sm = unreal.load_asset(KB % name)
        b = sm.get_bounding_box()
        mnx, mny, mxx, mxy = b.min.x, b.min.y, b.max.x, b.max.y
        yaw = yaw % 360
        if yaw == 0:
            ex = (mnx, mxx); ey = (mny, mxy)
        elif yaw == 90:
            ex = (-mxy, -mny); ey = (mnx, mxx)
        elif yaw == 180:
            ex = (-mxx, -mnx); ey = (-mxy, -mny)
        else:
            ex = (mny, mxy); ey = (-mxx, -mnx)
        if xmin is not None:
            x = xmin - ex[0]
        elif xmax is not None:
            x = xmax - ex[1]
        else:
            x = xcenter - (ex[0] + ex[1]) * 0.5
        y = ymin - ey[0]
        for mesh_name, z in ((name, KB_GROUND), (name + "-Base", KB_GROUND - 10)):
            if mesh_name.endswith("-Base") and not base:
                continue
            m = unreal.load_asset(KB % mesh_name)
            if not m:
                continue
            a = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, z), unreal.Rotator(0, 0, yaw))
            a.static_mesh_component.set_static_mesh(m)
            a.static_mesh_component.set_editor_property("cast_shadow", shadow)
            a.set_actor_label("KB_" + mesh_name)
            a.set_folder_path("Backdrop")
        return (x + ex[0], x + ex[1], y + ey[0], y + ey[1])

    # East side: a row of mid-rise buildings whose fronts line up at x = 3350, from y = -900 northwards.
    cursor = -900
    for name in ["SM_Building12", "SM_Building13", "SM_Building15", "SM_Building14", "SM_Building12"]:
        box = place(name, 90, xmin=3350, ymin=cursor)
        cursor = box[3] + 60
    # North end, behind the compound's courtyard: an 82 m tower.
    place("SM_Building10", 0, xcenter=1000, ymin=14300, shadow=False)   # huge Nanite shadow casters can hang the GPU
    # West of the safehouse (the street's north-west corner).
    cursor = 4400
    for name in ["SM_Building15", "SM_Building14", "SM_Building13"]:
        box = place(name, 90, xmax=-1350, ymin=cursor)
        cursor = box[3] + 60
    # Skyline across the street block (no shadows: far away, and big Nanite shadow casters are expensive).
    place("SM_Building06", 0, xcenter=10500, ymin=1500, shadow=False, base=False)
    place("SM_Building04", 0, xcenter=11500, ymin=-9000, shadow=False, base=False)
    place("SM_Building07", 90, xcenter=-2000, ymin=14000, shadow=False, base=False)
    zs.log("Backdrop buildings placed")


build_backdrop()
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

# Street furniture, fires and old blood (after the art is in, so props sit on its ground).
rooms = [(nyc_compound.wx(a), nyc_compound.wy(b), nyc_compound.wx(c), nyc_compound.wy(d))
         for name, (a, b, c, d), *_ in nyc_compound.ROOMS if name != "Courtyard"]
nyc_dressing.dress(eas, rooms)

les.save_current_level()
zs.log("Saved " + MAP)
