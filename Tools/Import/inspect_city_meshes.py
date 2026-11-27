"""Log bounds (min/max, local space), collision presence and triangle-ish info for the NYC kitbash and road pack
meshes, plus the RoadPack01 demo map's layout, so the city level generator can tile them correctly."""
import unreal

eal = unreal.EditorAssetLibrary
ROOTS = ["/Game/MS_NewYorkCityKitbash_Assets/Meshes-Buildings", "/Game/MS_NewYorkCityKitbash_Assets/Meshes-Assets", "/Game/RoadPack01/Meshes"]
for root in ROOTS:
    for path in sorted(eal.list_assets(root, recursive=True, include_folder=False)):
        sm = eal.load_asset(path)
        if not isinstance(sm, unreal.StaticMesh):
            continue
        b = sm.get_bounding_box()
        mn, mx = b.min, b.max
        body = sm.get_editor_property("body_setup")
        coll = "none"
        if body:
            n = unreal.EditorStaticMeshLibrary.get_simple_collision_count(sm) if hasattr(unreal, "EditorStaticMeshLibrary") else -1
            ct = body.get_editor_property("collision_trace_flag")
            coll = "%d prims %s" % (n, str(ct).split(".")[-1])
        unreal.log_warning("[ZSMesh] %s min(%d,%d,%d) max(%d,%d,%d) coll=%s" % (path.split("/")[-1].split(".")[0], mn.x, mn.y, mn.z, mx.x, mx.y, mx.z, coll))

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les.load_level("/Game/RoadPack01/Maps/RoadPack01Map")
for a in eas.get_all_level_actors():
    if isinstance(a, unreal.StaticMeshActor):
        sm = a.static_mesh_component.static_mesh
        unreal.log_warning("[ZSRoad] %s at %s rot %s scale %s" % (sm.get_name() if sm else None, a.get_actor_location(), a.get_actor_rotation().yaw, a.get_actor_scale3d()))
    else:
        unreal.log_warning("[ZSRoad] actor %s %s" % (a.get_class().get_name(), a.get_actor_location()))
