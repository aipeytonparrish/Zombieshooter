"""Build /Game/ZS/Maps/L_Bunker: the hub between runs. A self-building room (AZSBunkerLayout), the five stations
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
if not les.new_level(MAP):
    raise RuntimeError("new_level failed")

layout = eas.spawn_actor_from_class(unreal.ZSBunkerLayout, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
layout.set_actor_label("ZSBunkerLayout")

start = eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-750, 0, 100), unreal.Rotator(0, 0, 0))
start.set_actor_label("PlayerStart")

T = unreal.ZSStationType
# (type, x, y, yaw): a station's front (sign side) faces local -X.
stations = [
    (T.MAP_TABLE, 250, 0, 0),
    (T.ARMORY, -150, -560, -90),
    (T.MARKET, 450, -560, -90),
    (T.CHARACTER, -150, 560, 90),
    (T.MISSIONS, 450, 560, 90),
]
for kind, x, y, yaw in stations:
    st = eas.spawn_actor_from_class(unreal.ZSStation, unreal.Vector(x, y, 50), unreal.Rotator(0, 0, yaw))
    st.set_editor_property("type", kind)
    st.set_actor_label("ZSStation_" + str(kind).split(".")[-1])

# Indoors: keep exposure steady so the room doesn't flare when you look at a lamp.
ppv = eas.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 200), unreal.Rotator(0, 0, 0))
ppv.set_editor_property("unbound", True)
s = ppv.get_editor_property("settings")
s.set_editor_property("override_auto_exposure_min_brightness", True)
s.set_editor_property("auto_exposure_min_brightness", 4.5)
s.set_editor_property("override_auto_exposure_max_brightness", True)
s.set_editor_property("auto_exposure_max_brightness", 5.5)
ppv.set_editor_property("settings", s)

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
ws = world.get_world_settings()
ws.set_editor_property("default_game_mode", unreal.ZSBunkerGameMode)

les.save_current_level()
zs.log("Saved " + MAP)
