"""Build /Game/ZS/Maps/L_Bunker: the hub between runs. The Bunker pack's demo level streamed in (or, without the
pack, a self-building placeholder room, AZSBunkerLayout), the five stations
(map table, armory, market, locker, mission board), a player start and the bunker game mode.
This map is ours (versioned). Re-run to rebuild it.
"""
import os
import sys
import unreal

sys.path.append(os.path.dirname(__file__))
import zs_import_common as zs  # noqa: E402

MAP = "/Game/ZS/Maps/L_Bunker"
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

if zs.eal.does_asset_exist(MAP):
    zs.eal.delete_asset(MAP)
# delete_asset can leave the file behind in a commandlet; remove it so new_level can create it again.
umap = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()), "ZS", "Maps", "L_Bunker.umap")
if os.path.exists(umap):
    os.remove(umap)
# Let the asset registry notice the file is gone, or new_level refuses ("an asset already exists").
unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(["/Game/ZS/Maps"], True)
if not les.new_level(MAP):
    raise RuntimeError("new_level failed")

ART = "/Game/Bunker/Maps/Demo"   # "Bunker" Fab pack demo level (gitignored third-party content)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
T = unreal.ZSStationType

if zs.eal.does_asset_exist(ART):
    # Dressed bunker: stream the pack's demo level in (always loaded) and line the stations up along its main
    # corridor (runs +Y from the pack's own player start at 2580,-2500; floor ~z580, walls ~x2000 / x2855).
    floor = 600
    # (type, x, y, yaw): a station's front (sign side) faces local -X.
    stations = [
        (T.ARMORY, 2290, -1850, 180),
        (T.MARKET, 2290, -1150, 180),
        (T.CHARACTER, 2290, -450, 180),
        (T.MISSIONS, 2290, 150, 180),
        (T.MAP_TABLE, 2560, 420, 90),
    ]
else:
    # Fallback: the self-building placeholder room.
    layout = eas.spawn_actor_from_class(unreal.ZSBunkerLayout, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    layout.set_actor_label("ZSBunkerLayout")
    start = eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-750, 0, 100), unreal.Rotator(0, 0, 0))
    start.set_actor_label("PlayerStart")
    floor = 0
    stations = [
        (T.MAP_TABLE, 250, 0, 0),
        (T.ARMORY, -150, -560, -90),
        (T.MARKET, 450, -560, -90),
        (T.CHARACTER, -150, 560, 90),
        (T.MISSIONS, 450, 560, 90),
    ]
    # Indoors: keep exposure steady so the room doesn't flare when you look at a lamp.
    ppv = eas.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 200), unreal.Rotator(0, 0, 0))
    ppv.set_editor_property("unbound", True)
    s = ppv.get_editor_property("settings")
    s.set_editor_property("override_auto_exposure_min_brightness", True)
    s.set_editor_property("auto_exposure_min_brightness", 4.5)
    s.set_editor_property("override_auto_exposure_max_brightness", True)
    s.set_editor_property("auto_exposure_max_brightness", 5.5)
    ppv.set_editor_property("settings", s)

# Mood: the pack's demo lighting reads washed out (blown whites, flat greys). One unbound volume over everything:
# a stop darker, steadier exposure, more contrast, a little desaturated and cooler, vignette and grain.
mood = eas.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, floor + 200), unreal.Rotator(0, 0, 0))
mood.set_actor_label("ZS_BunkerMood")
mood.set_editor_property("unbound", True)
mood.set_editor_property("priority", 10.0)
m = mood.get_editor_property("settings")
for prop, value in (
        ("auto_exposure_bias", -1.0),
        ("auto_exposure_speed_up", 1.5),
        ("auto_exposure_speed_down", 1.0),
        ("color_contrast", unreal.Vector4(1.18, 1.18, 1.18, 1.0)),
        ("color_saturation", unreal.Vector4(0.8, 0.82, 0.85, 1.0)),
        ("color_gain", unreal.Vector4(0.92, 0.98, 1.04, 1.0)),
        ("color_gamma", unreal.Vector4(0.96, 0.96, 0.96, 1.0)),
        ("vignette_intensity", 0.55),
        ("film_grain_intensity", 0.22),
        ("bloom_intensity", 0.35),
        ("tone_curve_amount", 1.0)):
    m.set_editor_property("override_" + prop, True)
    m.set_editor_property(prop, value)
mood.set_editor_property("settings", m)

persistent = None
for kind, x, y, yaw in stations:
    st = eas.spawn_actor_from_class(unreal.ZSStation, unreal.Vector(x, y, floor + 50), unreal.Rotator(0, 0, yaw))
    st.set_editor_property("type", kind)
    st.set_actor_label("ZSStation_" + str(kind).split(".")[-1])
    persistent = st.get_level()

if floor:
    # Add the art level last, then make the persistent level current again so only L_Bunker is saved
    # (never write into the pack's own Demo map).
    unreal.EditorLevelUtils.add_level_to_world(world, ART, unreal.LevelStreamingAlwaysLoaded)
    les.set_current_level_by_name("L_Bunker")
    zs.log("Bunker art level streamed in: " + ART)

ws = world.get_world_settings()
ws.set_editor_property("default_game_mode", unreal.ZSBunkerGameMode)

les.save_current_level()
zs.log("Saved " + MAP)
