"""Build /Game/ZS/Maps/Test/L_ZSTest: engine sky/light template + movement course + NavMesh volume + zombie spawn
points + unlimited ammo crates, with the survival game mode. This map is ours (versioned). Re-run to rebuild it.
"""
import os
import sys
import unreal

sys.path.append(os.path.dirname(__file__))
import zs_import_common as zs  # noqa: E402

MAP = "/Game/ZS/Maps/Test/L_ZSTest"
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

if zs.eal.does_asset_exist(MAP):
    zs.eal.delete_asset(MAP)
if not les.new_level_from_template(MAP, "/Engine/Maps/Templates/Template_Default"):
    raise RuntimeError("new_level_from_template failed")

for a in eas.get_all_level_actors():
    label = a.get_actor_label()
    if label == "Floor" or label.startswith("SM_Template_Map_Floor"):
        eas.destroy_actor(a)
    elif isinstance(a, unreal.PlayerStart):
        a.set_actor_location(unreal.Vector(-800, 0, 100), False, False)
        a.set_actor_rotation(unreal.Rotator(0, 0, 0), False)

course = eas.spawn_actor_from_class(unreal.ZSMovementCourse, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
course.set_editor_property("spawn_targets", False)
course.set_actor_label("ZSMovementCourse")

nav = eas.spawn_actor_from_class(unreal.NavMeshBoundsVolume, unreal.Vector(3500, 2000, 100), unreal.Rotator(0, 0, 0))
origin, extent = nav.get_actor_bounds(False)
zs.log("Nav volume default extent: %s" % extent)
# Cover the whole course floor (-1500..8500 x -3200..7200) with some height.
target = unreal.Vector(5200, 5400, 800)
if extent.x > 1:
    nav.set_actor_scale3d(unreal.Vector(target.x / extent.x, target.y / extent.y, target.z / extent.z))
origin, extent = nav.get_actor_bounds(False)
zs.log("Nav volume extent now: %s at %s" % (extent, origin))

spawns = [(-1300, -3000), (3500, -3000), (8300, -3000), (-1300, 7000), (3500, 7000), (8300, 7000),
          (8300, 2000), (6000, -1500), (6000, 4000), (2500, 4600), (1500, -2000), (-1300, 3500),
          (-1400, -1600), (-1400, 1700), (900, 1800), (900, -1700), (3000, 1300), (3000, -1100)]
for i, (x, y) in enumerate(spawns):
    sp = eas.spawn_actor_from_class(unreal.ZSSpawnPoint, unreal.Vector(x, y, 95), unreal.Rotator(0, 0, 0))
    sp.set_actor_label("ZSSpawnPoint_%02d" % i)

for i, (x, y) in enumerate([(-450, 450), (2600, 5600), (6200, 1500)]):
    c = eas.spawn_actor_from_class(unreal.ZSAmmoCrate, unreal.Vector(x, y, 0), unreal.Rotator(0, 0, 0))
    c.set_actor_label("ZSAmmoCrate_%02d" % i)

world = unreal.EditorLevelLibrary.get_editor_world()
ws = world.get_world_settings()
ws.set_editor_property("default_game_mode", unreal.ZSGameMode_Survival)

les.save_current_level()
zs.log("Saved " + MAP)
