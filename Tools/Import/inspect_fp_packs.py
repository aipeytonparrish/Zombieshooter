"""Print what the C++ needs from the InfimaGames TFA rifle and the ProFPS pistol: anims, lengths, skeletons, sockets, bones."""
import os
import sys
import unreal

sys.path.append(os.path.dirname(__file__))
import zs_import_common as zs  # noqa: E402

TFA = "/Game/InfimaGames/TacticalFPSAnimations"
PFP = "/Game/ProFPSAnimationBundleV2/PistolAnimationV2"


def mesh_info(path):
    m = unreal.load_asset(path)
    if not m:
        zs.log("MISSING " + path)
        return
    skel = m.get_editor_property("skeleton")
    sockets = []
    try:
        for i in range(m.num_sockets()):
            s = m.get_socket_by_index(i)
            sockets.append(str(s.socket_name) + "@" + str(s.bone_name))
    except Exception as e:  # noqa: BLE001
        sockets = ["err " + str(e)]
    sk_sockets = []
    bones = []
    try:
        ref = unreal.SkeletalMeshEditorSubsystem if False else None
    except Exception:  # noqa: BLE001
        pass
    zs.log("MESH %s skel=%s sockets=%s skelSockets=%s" % (path, skel.get_path_name() if skel else None, sockets, sk_sockets))
    b = m.get_bounds()
    zs.log("   bounds origin=(%.1f,%.1f,%.1f) extent=(%.1f,%.1f,%.1f)" % (b.origin.x, b.origin.y, b.origin.z, b.box_extent.x, b.box_extent.y, b.box_extent.z))


def list_anims(folder, recursive=True, limit=400):
    for a in sorted(zs.eal.list_assets(folder, recursive=recursive))[:limit]:
        data = unreal.EditorAssetLibrary.find_asset_data(a)
        cls = str(data.asset_class_path.asset_name)
        if cls not in ("AnimSequence", "AnimMontage", "SoundCue", "SoundWave", "MetaSoundSource", "NiagaraSystem", "SkeletalMesh", "BlendSpace", "BlendSpace1D"):
            continue
        extra = ""
        if cls == "AnimSequence":
            anim = zs.eal.load_asset(a)
            skel = anim.get_editor_property("skeleton")
            extra = "len=%.2f skel=%s" % (anim.get_play_length(), skel.get_name() if skel else None)
        zs.log("%-15s %s %s" % (cls, a.split(".")[0], extra))


mesh_info(TFA + "/Common/Characters/Mannequins/Meshes/SKM_FP_Manny_Simple")
mesh_info(TFA + "/Weapons/AssaultRifle/Meshes/SK_TFA_AR_Default_Combined")
mesh_info(TFA + "/Weapons/AssaultRifle/Meshes/SK_TFA_AR")
mesh_info(PFP + "/Meshes/Pistol/SKM_PistolV2")
mesh_info("/Game/AAA_FPS_Gloves/Modules/SK_Arms_01")

for f in [TFA + "/Weapons/AssaultRifle/Animations", TFA + "/Weapons/AssaultRifle/Audio", TFA + "/Common/VFX", PFP + "/Animations", PFP + "/Meshes"]:
    zs.log("==== " + f)
    list_anims(f)

# Bone names of the FP skeletons (first 200)
for p in [TFA + "/Common/Characters/Mannequins/Meshes/SKEL_TFA_Mannequin", TFA + "/Weapons/AssaultRifle/Meshes/SKEL_TFA_AR"]:
    s = unreal.load_asset(p)
    if s:
        try:
            names = [str(n) for n in unreal.AnimationLibrary.get_bone_names_from_skeleton(s)] if hasattr(unreal.AnimationLibrary, "get_bone_names_from_skeleton") else []
        except Exception as e:  # noqa: BLE001
            names = ["err " + str(e)]
        zs.log("BONES %s: %s" % (p.split("/")[-1], names[:200]))
