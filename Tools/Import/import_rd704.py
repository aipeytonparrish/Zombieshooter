"""Import the RD-704 modular AK (SourceArt/ThirdParty/RD704_ModularAK) as static-mesh parts + PBR materials.

Output: /Game/ZS_Imported/Weapons/RD704/  (SM_RD704_<Part>, MI_RD704_<Part>, textures in /Textures)
The parts were modelled in place, so attaching them all at the same origin assembles the gun.
"""
import os
import sys
import unreal

sys.path.append(os.path.dirname(__file__))
import zs_import_common as zs  # noqa: E402

SRC = os.path.join(zs.SOURCE_ART, "RD704_ModularAK")
FBX = os.path.join(SRC, "separated_FBX", "separated_FBX")
TEX = os.path.join(SRC, "textures", "textures", "PBR")
DEST = zs.IMPORT_ROOT + "/Weapons/RD704"

# Part name -> (fbx relative to FBX dir, texture folder relative to TEX, texture file prefix)
PARTS = {
    "Receiver":   ("rd_704_receiver/rd_704_receiver.fbx", "rd_704_receiver", "rd_704_receiver"),
    "Handguard":  ("rd_704_handguard/rd_704_handguard.fbx", "rd_704_handguard", "rd_704_handguard"),
    "Cover":      ("AK_cover_PDC/AK_cover_PDC.fbx", "AK_cover_PDC", "AK_cover_PDC"),
    "Grip":       ("AK_grip_scorpius/AK_grip_scorpius.fbx", "AK_grip_scorpius", "AK_grip_scorpius"),
    "Stock":      ("stock_magpul_zhukov/stock_magpul_zhukov.fbx", "stock_magpul_zhukov", "stock_magpul_zhukov"),
    "Magazine":   ("magazine_FABdefense_ultimag30/magazine_FABdefense_ultimag30.fbx", "magazine_FABdefense_ultimag30", "magazine_FABdefense_ultimag30"),
    "MagAmmo":    ("ammo_7.62_BP/AMMO_in_magazine.fbx", "ammo_7.62_BP", "ammo_7.62_BP"),
    "Round":      ("ammo_7.62_BP/ammo_7.62_BP.fbx", "ammo_7.62_BP", "ammo_7.62_BP"),
    "Scope":      ("scope_BRAVO4/scope_BRAVO4.fbx", "scope_BRAVO4", "scope_BRAVO4"),
    "Reticle":    ("scope_BRAVO4/scope_BRAVO4_reticle.fbx", None, None),
    "Suppressor": ("Suppressor_WAWE_QD/Suppressors_WAWE_QD.fbx", "Suppressors_WAWE_QD", "Suppressors_WAWE_QD"),
    "MuzzleBrake": ("Suppressor_WAWE_QD/MuzzleBrake_WAWE.fbx", "Suppressors_WAWE_QD", "Suppressors_WAWE_QD"),
    "ForeGrip":   ("tactical_grip_BCM_GUNFIGHTER/tactical_grip_BCM_GUNFIGHTER.fbx", "tactical_grip_BCM_GUNFIGHTER", "tactical_grip_BCM_GUNFIGHTER"),
}


def import_part_textures(folder, prefix):
    d = os.path.join(TEX, folder)
    dest = DEST + "/Textures"
    maps = {
        "BaseColor": (prefix + "_BaseColor.png", "color"),
        "Normal": (prefix + "_Normal_directX.png", "normal"),
        "Metallic": (prefix + "_Metallic.png", "mask"),
        "Roughness": (prefix + "_Roughness.png", "mask"),
        "AO": (prefix + "_Mixed_AO.png", "mask"),
    }
    out = {}
    for param, (fname, kind) in maps.items():
        p = os.path.join(d, fname)
        out[param] = zs.import_texture(p, dest, kind) if os.path.exists(p) else None
    return out


def make_reticle_instance():
    """Bravo4 reticle: its opacity texture on the masked, unlit glow material."""
    parent = zs.ensure_reticle_material()
    tex_path = os.path.join(TEX, "scope_BRAVO4", "reticle", "scope_BRAVO4_reticle_Opacity.png")
    tex = zs.import_texture(tex_path, DEST + "/Textures", "mask")
    path = DEST + "/MI_RD704_Reticle"
    if zs.eal.does_asset_exist(path):
        zs.eal.delete_asset(path)
    mi = zs.asset_tools.create_asset("MI_RD704_Reticle", DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    zs.mel.set_material_instance_parent(mi, parent)
    if tex is not None:
        zs.mel.set_material_instance_texture_parameter_value(mi, "Opacity", tex)
    zs.eal.save_loaded_asset(mi)
    return mi


def run():
    zs.ensure_master_material()
    tex_cache = {}
    for part, (fbx_rel, tex_folder, prefix) in PARTS.items():
        fbx = os.path.join(FBX, fbx_rel)
        if not os.path.exists(fbx):
            zs.log("MISSING " + fbx)
            continue
        objs = zs.import_files([fbx], DEST, zs.static_mesh_fbx_options(combine=True))
        meshes = [o for o in objs if isinstance(zs.eal.load_asset(o), unreal.StaticMesh)]
        if not meshes:
            zs.log("FAILED mesh " + fbx + " -> " + str(objs))
            continue
        mesh = zs.eal.load_asset(meshes[0])
        target = DEST + "/SM_RD704_" + part
        if mesh.get_path_name().split(".")[0] != target:
            if zs.eal.does_asset_exist(target):
                zs.eal.delete_asset(target)
            zs.eal.rename_asset(mesh.get_path_name().split(".")[0], target)
            mesh = zs.eal.load_asset(target)

        if tex_folder:
            if tex_folder not in tex_cache:
                tex_cache[tex_folder] = import_part_textures(tex_folder, prefix)
            mi = zs.make_pbr_instance("MI_RD704_" + tex_folder, DEST, tex_cache[tex_folder])
        else:
            mi = make_reticle_instance()
        for i in range(len(mesh.static_materials)):
            mesh.set_material(i, mi)
        zs.eal.save_loaded_asset(mesh)
        zs.log("PART %-12s %s" % (part, zs.describe_mesh(mesh)))
    zs.log("RD704 import done")


run()
