import os, sys, unreal
sys.path.append(os.path.dirname(__file__))
import zs_import_common as zs
roots = ["/Game/InfimaGames/TacticalFPSAnimations/Weapons/AssaultRifle/Animations/Character/FP", "/Game/ProFPSAnimationBundleV2/PistolAnimationV2/Animations"]
for r in roots:
    for a in sorted(zs.eal.list_assets(r, recursive=True)):
        o = zs.eal.load_asset(a)
        if isinstance(o, unreal.AnimSequence):
            t = o.get_editor_property("additive_anim_type")
            bp = o.get_editor_property("ref_pose_type") if t != unreal.AdditiveAnimationType.AAT_NONE else ""
            ref = o.get_editor_property("ref_pose_seq") if t != unreal.AdditiveAnimationType.AAT_NONE else None
            zs.log("%-50s %s %s %s" % (o.get_name(), t, bp, ref.get_name() if ref else ""))
