"""Dump component hierarchy (class, parent socket, relative transform, mesh) of the FP packs' demo character Blueprints."""
import os
import sys
import unreal

sys.path.append(os.path.dirname(__file__))
import zs_import_common as zs  # noqa: E402

BPS = [
    "/Game/InfimaGames/TacticalFPSAnimations/Common/Framework/Characters/BP_TFA_BaseCharacter",
    "/Game/InfimaGames/TacticalFPSAnimations/Common/Framework/Weapons/BP_TFA_BaseWeapon",
    "/Game/ProFPSAnimationBundleV2/PistolAnimationV2/Blueprints/BP_DemoCharacter",
]

sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
for path in BPS:
    bp = unreal.load_asset(path)
    if not bp:
        zs.log("MISSING " + path)
        continue
    zs.log("==== " + path)
    handles = sub.k2_gather_subobject_data_for_blueprint(bp)
    for h in handles:
        data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(h)
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
        if not obj:
            continue
        line = "%s (%s)" % (obj.get_name(), obj.get_class().get_name())
        if isinstance(obj, unreal.SceneComponent):
            loc = obj.get_editor_property("relative_location")
            rot = obj.get_editor_property("relative_rotation")
            parent = obj.get_attach_parent()
            sock = obj.get_attach_socket_name()
            line += " loc=(%.2f,%.2f,%.2f) rot=(p%.1f,y%.1f,r%.1f) parent=%s socket=%s" % (
                loc.x, loc.y, loc.z, rot.pitch, rot.yaw, rot.roll, parent.get_name() if parent else None, sock)
            try:
                line += " fov=%.1f" % obj.get_editor_property("field_of_view")
            except Exception:  # noqa: BLE001
                pass
        if isinstance(obj, unreal.SkinnedMeshComponent):
            m = obj.get_editor_property("skeletal_mesh_asset") if hasattr(obj, "skeletal_mesh_asset") else None
            try:
                m = obj.get_skinned_asset()
            except Exception:  # noqa: BLE001
                pass
            line += " mesh=%s" % (m.get_path_name() if m else None)
            try:
                ac = obj.get_editor_property("anim_class")
                line += " anim=%s" % (ac.get_name() if ac else None)
            except Exception:  # noqa: BLE001
                pass
        zs.log(line)
