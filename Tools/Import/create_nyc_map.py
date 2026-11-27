"""Build /Game/ZS/Maps/L_NYC: the New York survival level. Ours (versioned): weather, lighting, gameplay actors.
The art (the owner's post-apocalyptic NY scene, imported by import_postapoc_ny.py into the git-ignored
/Game/NYC_PostApoc) is streamed in from /Game/NYC_PostApoc/Maps/L_NYC_Art.

Weather: a grey, rainy afternoon - overcast storm clouds, a dim soft sun, light fog off the river, lightning, and
AZSRain following the camera. Gameplay (round-based, start indoors): the player starts in a two-room safehouse at the
north end of the street (built from the owner's industrial modular kit, /Game/IndustrialKit, see
import_industrial_kit.py); zombies crawl up through the floor grates in its back room. A boarded door (AZSBuyDoor,
run cash) opens the street, whose spawn points belong to the "Street" zone. Unlimited ammo crates, invisible walls
around the play area and a NavMesh volume. Re-run to rebuild.
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
PLAY_MAX = unreal.Vector2D(3000, 7950)   # north end: the safehouse (x 0..1600, y 6100..7700) and the street around it
GROUND_Z = -100
PLAYER_START = (800, 6800, 0, -90)    # x, y, z, yaw: inside the safehouse, facing its door to the street
# Flat street cells (x, y, floor z) around the edges of the play area.
SPAWNS = [(-1000, -1500, -50), (1500, -5750, -40), (2750, 0, -110), (2750, 1500, -110), (2750, 3000, -110),
          (2500, 4500, -110), (0, 5500, -110), (-500, 3750, -110), (-500, 1750, -30), (-500, 250, -30),
          (1250, -3000, -40), (2250, -2500, -110), (1000, -4250, -30), (1750, -5750, -40)]
# A few more out on the street near the safehouse (open once the door is bought).
SPAWNS += [(-800, 6500, -130), (-600, 7600, -150), (2600, 6800, -130), (2400, 7700, -125)]
CRATES = [(-250, 0, -30, 90), (2000, 2750, -110, 0), (1000, -4000, -40, 90)]   # x, y, floor z, yaw

# ---- Safehouse (start room) ----
KIT = "/Game/IndustrialKit/%s/StaticMeshes/%s_LOD0"
SH_MIN = (0, 6100)          # inner footprint, 2 m kit grid: 8 x 8 tiles
SH_TILES = (8, 8)
SH_FLOOR = -100             # floor top
SH_DIVIDER_ROW = 5          # wall between the main room (rows 0-4) and the back room (rows 5-7)
SH_DOOR_TILE = 6            # south wall tile with the buyable door (x 1200..1400; a ruin stands in front of x < 900)
DOOR_COST = 350

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
sx0, sy0 = SH_MIN
sx1, sy1 = sx0 + SH_TILES[0] * 200, sy0 + SH_TILES[1] * 200
rain.set_editor_property("shelter_boxes", [unreal.Box(unreal.Vector(sx0 - 60, sy0 - 60, SH_FLOOR - 60), unreal.Vector(sx1 + 60, sy1 + 60, SH_FLOOR + 420))])
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
    sp.set_editor_property("zone", "Street")   # used once the safehouse door is bought


def build_safehouse():
    """Two rooms on the 2 m kit grid: walls are doubled (inner + outer shell) so both sides are finished, a doorway
    gap in the south wall for the buy door, an open doorway in the divider, ceiling, roof, pillars and lights."""
    if not zs.eal.does_asset_exist(KIT % ("SM_Wall_Concrete_2x3", "SM_Wall_Concrete_2x3")):
        zs.log("WARNING: /Game/IndustrialKit missing - run import_industrial_kit.py (no safehouse)")
        return
    meshes = {}

    def mesh(name):
        if name not in meshes:
            meshes[name] = unreal.load_asset(KIT % (name, name))
        return meshes[name]

    folder = "Safehouse"

    def put(name, x, y, z, yaw, scale=(1, 1, 1), label=None):
        m = mesh(name)
        if m is None:
            raise RuntimeError("kit mesh missing: " + KIT % (name, name))
        a = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, z), unreal.Rotator(0, 0, yaw))
        a.static_mesh_component.set_static_mesh(m)
        a.set_actor_scale3d(unreal.Vector(*scale))
        a.set_folder_path(folder)
        if label:
            a.set_actor_label(label)
        return a

    nx, ny = SH_TILES
    z = SH_FLOOR
    # Floor (top at SH_FLOOR), ceiling, and a roof slab (floor tiles again, above the ceiling).
    for ix in range(nx):
        for iy in range(ny):
            cx, cy = sx0 + 100 + ix * 200, sy0 + 100 + iy * 200
            back = iy >= SH_DIVIDER_ROW
            name = "SM_Floor_Concrete_2x2"
            if back and (ix, iy) in ((1, 6), (6, 6)):
                name = "SM_Floor_Grate_2x2"          # the zombies' way in
            elif (ix + iy * 3) % 7 == 0:
                name = "SM_Floor_Drain_2x2"
            elif back and (ix + iy) % 3 == 0:
                name = "SM_Floor_TreadPlate_2x2"
            put(name, cx, cy, z - 3, 0)
            ceil = ["SM_Ceiling_Tiles_2x2", "SM_Ceiling_Beams_2x2", "SM_Ceiling_Pipes_2x2", "SM_Ceiling_Vent_2x2"][(ix * 7 + iy * 3) % 4]
            put(ceil, cx, cy, z + 300, 0)
            put("SM_Floor_Concrete_2x2", cx, cy, z + 375, 0)

    # Perimeter: (tile centre, inward yaw, outward offset) per side. Kit walls face +Y locally.
    def side_tiles(side):
        if side == "S":
            return [((sx0 + 100 + i * 200, sy0), 0, (0, -28)) for i in range(nx)]
        if side == "N":
            return [((sx0 + 100 + i * 200, sy1), 180, (0, 28)) for i in range(nx)]
        if side == "W":
            return [((sx0, sy0 + 100 + i * 200), -90, (-28, 0)) for i in range(ny)]
        return [((sx1, sy0 + 100 + i * 200), 90, (28, 0)) for i in range(ny)]

    layout = {
        "S": ["Concrete", "Window", "Window", "Pipes", "Concrete", "Window", "DOOR", "Concrete"],
        "N": ["Concrete", "Pipes", "Concrete", "Corrugated", "Corrugated", "Concrete", "Pipes", "Concrete"],
        "W": ["Concrete", "Window", "Tile", "Window", "Concrete", "Corrugated", "Concrete", "Pipes"],
        "E": ["Concrete", "Window", "Concrete", "Window", "Pipes", "Concrete", "Corrugated", "Concrete"],
    }
    for side, kinds in layout.items():
        for ((x, y), yaw, (ox, oy)), kind in zip(side_tiles(side), kinds):
            if kind == "DOOR":
                # Lintel over a 2 m x 2.2 m opening, both shells.
                put("SM_Wall_Concrete_2x3", x, y, z + 220, yaw, (1, 1, 0.27))
                put("SM_Wall_Concrete_2x3", x + ox, y + oy, z + 220, yaw + 180, (1, 1, 0.27))
                put("SM_Wall_Concrete_2x3", x + ox, y + oy, z + 300, yaw + 180, (1, 1, 0.3))
                continue
            put("SM_Wall_%s_2x3" % kind, x, y, z, yaw)
            outer = "SM_Wall_Window_2x3" if kind == "Window" else "SM_Wall_Concrete_2x3"
            put(outer, x + ox, y + oy, z, yaw + 180)
            put("SM_Wall_Concrete_2x3", x + ox, y + oy, z + 300, yaw + 180, (1, 1, 0.3))   # parapet over the ceiling void

    # Divider between the rooms, open doorway in the middle tile.
    ydiv = sy0 + SH_DIVIDER_ROW * 200
    for ix in range(nx):
        x = sx0 + 100 + ix * 200
        if ix == 3:
            put("SM_Wall_Concrete_2x3", x, ydiv, z + 220, 0, (1, 1, 0.27))
            put("SM_Wall_Concrete_2x3", x, ydiv + 28, z + 220, 180, (1, 1, 0.27))
            continue
        put("SM_Wall_Tile_2x3" if ix % 2 else "SM_Wall_Concrete_2x3", x, ydiv, z, 0)
        put("SM_Wall_Concrete_2x3", x, ydiv + 28, z, 180)

    for (x, y) in [(sx0 + 500, sy0 + 500), (sx0 + 1100, sy0 + 500)]:
        put("SM_Pillar_Concrete_3m", x, y, z, 0)

    # Lights: warm caged bulbs in the main room, a cold broken fluorescent in the back, red emergency lamps.
    def light(x, y, lz, color, cd, radius, fixture=None, yaw=0):
        if fixture:
            put(fixture, x, y, lz, yaw)
        pl = eas.spawn_actor_from_class(unreal.PointLight, unreal.Vector(x, y, lz - 30), unreal.Rotator(0, 0, 0))
        c = pl.point_light_component
        c.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
        c.set_editor_property("intensity", cd)
        c.set_editor_property("attenuation_radius", radius)
        c.set_editor_property("light_color", unreal.Color(r=color[0], g=color[1], b=color[2], a=color[3]))
        c.set_editor_property("source_radius", 6.0)
        pl.set_folder_path(folder)
        return pl

    for (x, y) in [(sx0 + 400, sy0 + 300), (sx0 + 1200, sy0 + 300), (sx0 + 400, sy0 + 800), (sx0 + 1200, sy0 + 800)]:
        light(x, y, z + 300, (255, 196, 140, 255), 40, 900, "SM_Light_Caged_Bulb")
    light(sx0 + 800, sy0 + 1300, z + 300, (190, 215, 255, 255), 25, 900, "SM_Light_Fluorescent_Broken")
    light(sx0 + 300, sy0 + 1300, z + 300, (255, 40, 30, 255), 18, 600, "SM_Light_Emergency_Red")
    light(sx0 + 100 + SH_DOOR_TILE * 200, sy0 + 40, z + 280, (255, 40, 30, 255), 15, 500, "SM_Light_Emergency_Red")

    # Buyable door to the street (forward = out through the doorway).
    door_x = sx0 + 100 + SH_DOOR_TILE * 200
    door = eas.spawn_actor_from_class(unreal.ZSBuyDoor, unreal.Vector(door_x, sy0 - 14, z), unreal.Rotator(0, 0, -90))
    door.set_actor_label("ZSBuyDoor_Street")
    door.set_folder_path(folder)
    door.set_editor_property("cost", DOOR_COST)
    door.set_editor_property("zone", "Street")
    door.set_editor_property("label", "STREET DOOR")
    door.set_editor_property("opening_size", unreal.Vector2D(200, 220))
    door.set_editor_property("board_material", unreal.load_asset("/Game/IndustrialKit/SM_Ceiling_Beams_2x2/Materials/M_Rust_Heavy"))

    # Zombies climb up through the back-room floor grates (always open, may spawn close to the player).
    for i, (x, y) in enumerate([(sx0 + 300, sy0 + 1300), (sx0 + 1300, sy0 + 1300), (sx0 + 800, sy0 + 1500), (sx0 + 150, sy0 + 1550)]):
        sp = eas.spawn_actor_from_class(unreal.ZSSpawnPoint, unreal.Vector(x, y, z + 95), unreal.Rotator(0, 0, 0))
        sp.set_actor_label("ZSSpawnPoint_Safehouse_%d" % i)
        sp.set_folder_path(folder)
        sp.set_editor_property("min_player_distance", 300.0)
    zs.log("Safehouse built")


build_safehouse()


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
    # North end, behind the safehouse: an 82 m tower.
    place("SM_Building10", 0, xcenter=600, ymin=8150)
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

les.save_current_level()
zs.log("Saved " + MAP)
