"""Import the owner's ammo-box FBXs (SourceArt/ThirdParty/AmmoBoxes) for the unlimited ammo crates.

Output: /Game/ZS_Imported/Props/AmmoBoxes/SM_Ammobox_<Type>(_Closed) + MI_Ammobox_<Type>.
Textures are Unity-packed: _A albedo, _N normal (OpenGL, green flipped here), _MSA = Metallic / Smoothness / AO.
"""
import os
import sys
import unreal

sys.path.append(os.path.dirname(__file__))
import zs_import_common as zs  # noqa: E402

SRC = os.path.join(zs.SOURCE_ART, "AmmoBoxes")
DEST = zs.IMPORT_ROOT + "/Props/AmmoBoxes"
MASTER = zs.IMPORT_ROOT + "/Materials/M_ZS_PBR_MSA"
TYPES = ["556_NATO", "762_NATO", "9mm", "12_Gauge", "50", "300_Magnum", "40mm_Grenade"]


def expr(mat, cls, x, y, **props):
    node = zs.mel.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        node.set_editor_property(k, v)
    return node


def ensure_msa_master():
    if zs.eal.does_asset_exist(MASTER):
        return zs.eal.load_asset(MASTER)
    folder, name = MASTER.rsplit("/", 1)
    mat = zs.asset_tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    white = unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture")
    flat = unreal.load_asset("/Engine/EngineMaterials/DefaultNormal.DefaultNormal")
    ST = unreal.MaterialSamplerType
    base = zs._tex_param(mat, "BaseColor", -800, -300, ST.SAMPLERTYPE_COLOR, white)
    zs.mel.connect_material_property(base, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    nrm = zs._tex_param(mat, "Normal", -800, 0, ST.SAMPLERTYPE_NORMAL, flat)
    zs.mel.connect_material_property(nrm, "RGB", unreal.MaterialProperty.MP_NORMAL)
    msa = zs._tex_param(mat, "MSA", -800, 300, ST.SAMPLERTYPE_LINEAR_COLOR, white)
    zs.mel.connect_material_property(msa, "R", unreal.MaterialProperty.MP_METALLIC)
    inv = expr(mat, unreal.MaterialExpressionOneMinus, -450, 350)
    zs.mel.connect_material_expressions(msa, "G", inv, "")
    zs.mel.connect_material_property(inv, "", unreal.MaterialProperty.MP_ROUGHNESS)
    zs.mel.connect_material_property(msa, "B", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    zs.mel.recompile_material(mat)
    zs.eal.save_loaded_asset(mat)
    zs.log("Created " + MASTER)
    return mat


def import_tex(path, kind):
    objs = zs.import_files([path], DEST + "/Textures")
    if not objs:
        return None
    tex = zs.eal.load_asset(objs[0])
    if kind == "normal":
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("flip_green_channel", True)
    elif kind == "mask":
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        tex.set_editor_property("srgb", False)
    zs.eal.save_loaded_asset(tex)
    return tex


def run():
    master = ensure_msa_master()
    for t in TYPES:
        tex_dir = os.path.join(SRC, "Textures")
        a = import_tex(os.path.join(tex_dir, "Ammobox_%s_A.tga" % t), "color")
        n = import_tex(os.path.join(tex_dir, "Ammobox_%s_N.tga" % t), "normal")
        m = import_tex(os.path.join(tex_dir, "Ammobox_%s_MSA.tga" % t), "mask")
        mi_path = DEST + "/MI_Ammobox_" + t
        if zs.eal.does_asset_exist(mi_path):
            zs.eal.delete_asset(mi_path)
        mi = zs.asset_tools.create_asset("MI_Ammobox_" + t, DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        zs.mel.set_material_instance_parent(mi, master)
        for p, tex in (("BaseColor", a), ("Normal", n), ("MSA", m)):
            if tex:
                zs.mel.set_material_instance_texture_parameter_value(mi, p, tex)
        zs.eal.save_loaded_asset(mi)

        for variant in ("", "_Closed"):
            fbx = os.path.join(SRC, "Ammobox_%s%s.FBX" % (t, variant))
            if not os.path.exists(fbx):
                continue
            objs = zs.import_files([fbx], DEST, zs.static_mesh_fbx_options(combine=True))
            meshes = [o for o in objs if isinstance(zs.eal.load_asset(o), unreal.StaticMesh)]
            if not meshes:
                zs.log("FAILED " + fbx)
                continue
            target = DEST + "/SM_Ammobox_" + t + variant
            src_path = meshes[0].split(".")[0]
            if src_path != target:
                if zs.eal.does_asset_exist(target):
                    zs.eal.delete_asset(target)
                zs.eal.rename_asset(src_path, target)
            mesh = zs.eal.load_asset(target)
            for i in range(len(mesh.static_materials)):
                mesh.set_material(i, mi)
            # Simple box collision so the crate blocks the player and the interact trace.
            unreal.EditorStaticMeshLibrary.add_simple_collisions(mesh, unreal.ScriptCollisionShapeType.BOX)
            zs.eal.save_loaded_asset(mesh)
            zs.log("BOX %-22s %s" % (t + variant, zs.describe_mesh(mesh)))
    zs.log("Ammo boxes done")


run()
