"""Log skeletons of the zombie meshes and the extra zombie animation packs (for M11 specials)."""
import unreal

eal = unreal.EditorAssetLibrary
for p in ["/Game/ZombieCity01/Mesh/SK_ZombieCity01", "/Game/ZombieDoctor/Mesh/SK_ZombieDoctor"]:
    m = unreal.load_asset(p)
    skel = m.get_editor_property("skeleton")
    unreal.log_warning("[ZSInspect] mesh %s skel %s" % (p, skel.get_path_name()))
    names = [str(skel.get_editor_property("bone_tree")[i]) for i in range(0)] if False else []

for folder in ["/Game/Zombie_deaths_crawls", "/Game/Mocap_Zombie", "/Game/Zombie_Anim", "/Game/Zombie_Animation_Pack_Vol01", "/Game/Zombie_attack_hit_reaction_anim"]:
    assets = eal.list_assets(folder, True)
    skels = {}
    meshes = []
    for a in assets:
        d = eal.find_asset_data(a)
        cls = str(d.asset_class_path.asset_name)
        if cls == "AnimSequence":
            s = unreal.load_asset(a).get_editor_property("skeleton")
            k = s.get_path_name() if s else "None"
            skels.setdefault(k, []).append(a.split("/")[-1].split(".")[0])
        elif cls == "SkeletalMesh":
            meshes.append(a)
    for k, v in skels.items():
        unreal.log_warning("[ZSInspect] %s skel %s: %d anims e.g. %s" % (folder, k, len(v), ", ".join(sorted(v)[:40])))
    for m in meshes:
        unreal.log_warning("[ZSInspect] %s mesh %s" % (folder, m))
