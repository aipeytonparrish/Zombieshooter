import os, sys, unreal
sys.path.append(os.path.dirname(__file__))
import zs_import_common as zs
A = "/Game/InfimaGames/TacticalFPSAnimations/Weapons/AssaultRifle/Animations/"
P = "/Game/ProFPSAnimationBundleV2/PistolAnimationV2/Animations/"
for p in [A+"Character/FP/Combat/A_TFA_FP_AR_Reload", A+"Character/FP/Combat/A_TFA_FP_AR_Reload_Empty", A+"Character/FP/Combat/A_TFA_FP_AR_Fire_Semi", A+"Character/FP/Transitions/A_TFA_FP_AR_Equip_Quick",
          A+"Weapon/FP/A_TFA_FP_WEP_AR_Reload", A+"Weapon/FP/A_TFA_FP_WEP_AR_Fire", P+"AS_Player_PistolV2_Reload", P+"AS_Player_PistolV2_Reload_Empty", P+"AS_Player_PistolV2_Fire", P+"PistolV2/AS_SK_PistolV2_Reload", P+"PistolV2/AS_SK_PistolV2_Fire"]:
    a = unreal.load_asset(p)
    if not a:
        zs.log("MISSING " + p); continue
    evs = unreal.AnimationLibrary.get_animation_notify_events(a)
    out = []
    for e in evs:
        n = e.notify
        extra = ""
        if n and isinstance(n, unreal.AnimNotify_PlaySound):
            s = n.get_editor_property("sound")
            extra = ":" + (s.get_name() if s else "None")
        out.append("%.2f %s%s" % (e.get_editor_property("trigger_time_offset") if False else e.get_time(), (n.get_class().get_name() if n else str(e.notify_name)), extra))
    zs.log("%s len=%.2f notifies=%s" % (p.split("/")[-1], a.get_play_length(), out))
