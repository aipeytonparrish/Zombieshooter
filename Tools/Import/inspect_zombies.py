"""Print zombie mesh / animation facts needed by the C++ zombie (lengths, root motion, travel speed, slots)."""
import os
import sys
import unreal

sys.path.append(os.path.dirname(__file__))
import zs_import_common as zs  # noqa: E402

for mesh_path in ["/Game/ZombieCity01/Mesh/SK_ZombieCity01", "/Game/ZombieDoctor/Mesh/SK_ZombieDoctor"]:
    m = unreal.load_asset(mesh_path)
    if not m:
        zs.log("MISSING " + mesh_path)
        continue
    pa = m.get_editor_property("physics_asset")
    mats = m.get_editor_property("materials")
    zs.log("%s phys=%s slots=%s" % (mesh_path, pa.get_path_name() if pa else None,
           [(str(x.material_slot_name), x.material_interface.get_name() if x.material_interface else None) for x in mats]))
    b = m.get_bounds()
    zs.log("   bounds origin=%s extent=%s" % (b.origin, b.box_extent))

for folder in ["/Game/ZombieCity01/Animations", "/Game/ZombieDoctor/Animations"]:
    for a in sorted(zs.eal.list_assets(folder, recursive=False)):
        anim = zs.eal.load_asset(a)
        if not isinstance(anim, unreal.AnimSequence):
            continue
        length = anim.get_play_length()
        rm = anim.get_editor_property("enable_root_motion")
        try:
            p0 = unreal.AnimationLibrary.get_bone_pose_for_time(anim, "root", 0.0, False).translation
            p1 = unreal.AnimationLibrary.get_bone_pose_for_time(anim, "root", length, False).translation
            q0 = unreal.AnimationLibrary.get_bone_pose_for_time(anim, "pelvis", 0.0, False).translation
            q1 = unreal.AnimationLibrary.get_bone_pose_for_time(anim, "pelvis", length, False).translation
            root_d = (p1 - p0)
            pel_d = (q1 - q0)
            info = "rootD=(%.0f,%.0f,%.0f) pelvisD=(%.0f,%.0f,%.0f)" % (root_d.x, root_d.y, root_d.z, pel_d.x, pel_d.y, pel_d.z)
        except Exception as e:  # noqa: BLE001
            info = "pose err " + str(e)
        notifies = [str(e.notify_name) for e in unreal.AnimationLibrary.get_animation_notify_events(anim)]
        zs.log("%-34s len=%.2f rm=%s %s notifies=%s" % (anim.get_name(), length, rm, info, notifies))
