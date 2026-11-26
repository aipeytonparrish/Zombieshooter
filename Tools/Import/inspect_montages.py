import os, sys, unreal
sys.path.append(os.path.dirname(__file__))
import zs_import_common as zs
A = "/Game/InfimaGames/TacticalFPSAnimations/Weapons/AssaultRifle/Animations/"
P = "/Game/ProFPSAnimationBundleV2/PistolAnimationV2/Animations/"
for p in [A+"Character/FP/Combat/AM_TFA_FP_AR_Reload", A+"Character/FP/Combat/AM_TFA_FP_AR_Reload_Empty", A+"Character/FP/Combat/AM_TFA_FP_AR_Fire_Semi", A+"Character/FP/Transitions/AM_TFA_FP_AR_Equip",
          A+"Weapon/FP/AM_TFA_FP_WEP_AR_Reload", A+"Weapon/FP/AM_TFA_FP_WEP_AR_Reload_Empty", A+"Weapon/FP/AM_TFA_FP_WEP_AR_Fire",
          P+"AM_Player_PistolV2_Reload_Montage", P+"AM_Player_PistolV2_Fire_Montage", P+"PistolV2/AM_SK_PistolV2_Reload_Montage", P+"PistolV2/AM_SK_PistolV2_Reload_Empty_Montage", P+"PistolV2/AM_SK_PistolV2_Fire_Montage", P+"PistolV2/AM_SK_PistolV2_Fire_Empty_Montage"]:
    m = unreal.load_asset(p)
    if not m:
        zs.log("MISSING " + p); continue
    segs = []
    for track in m.get_editor_property("slot_anim_tracks"):
        for s in track.get_editor_property("anim_track").get_editor_property("anim_segments"):
            ref = s.get_editor_property("anim_reference")
            segs.append("%s[%.2f-%.2f]@%.2f x%.2f" % (ref.get_name() if ref else None, s.get_editor_property("anim_start_time"), s.get_editor_property("anim_end_time"), s.get_editor_property("start_pos"), s.get_editor_property("anim_play_rate")))
    evs = unreal.AnimationLibrary.get_animation_notify_events(m)
    if evs and not globals().get("_dumped"):
        globals()["_dumped"] = True
        zs.log("EVENT ATTRS " + str([a for a in dir(evs[0]) if not a.startswith("_")]))
    notes = []
    for e in evs:
        n = e.notify
        extra = ""
        if n:
            for prop in ("sound", "sounds"):
                try:
                    v = n.get_editor_property(prop)
                    extra = ":" + (v.get_name() if hasattr(v, "get_name") else str(v))
                    break
                except Exception:
                    pass
        notes.append("%s %s%s" % (getattr(e, "trigger_time", "?") if hasattr(e, "trigger_time") else "", n.get_class().get_name() if n else str(e.notify_name), extra))
    zs.log("%s len=%.2f segs=%s notifies=%s" % (p.split("/")[-1], m.get_play_length(), segs, notes))
