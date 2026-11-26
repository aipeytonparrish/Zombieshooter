"""Shared helpers for Zombieshooter's headless asset-import scripts.

Run any import script with:
  UnrealEditor-Cmd.exe <path>/Zombieshooter.uproject -run=pythonscript -script="<path>/Tools/Import/<script>.py" -unattended -nullrhi

Everything these scripts create goes under /Game/ZS_Imported/, which is git-ignored because it is derived from
licensed third-party source files (SourceArt/ThirdParty/). Re-run the scripts to regenerate it on another PC.
"""
import os
import unreal

PROJECT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SOURCE_ART = os.path.join(PROJECT_DIR, "SourceArt", "ThirdParty")
IMPORT_ROOT = "/Game/ZS_Imported"
MASTER_MATERIAL_PATH = IMPORT_ROOT + "/Materials/M_ZS_PBR_Master"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def log(msg):
    unreal.log_warning("[ZSImport] " + str(msg))


def import_files(files, dest_path, options=None):
    """Import a list of files into dest_path; returns imported object paths."""
    tasks = []
    for f in files:
        t = unreal.AssetImportTask()
        t.filename = f
        t.destination_path = dest_path
        t.automated = True
        t.replace_existing = True
        t.save = True
        if options is not None:
            t.options = options
        tasks.append(t)
    asset_tools.import_asset_tasks(tasks)
    out = []
    for t in tasks:
        out.extend(list(t.imported_object_paths))
    return out


def static_mesh_fbx_options(combine=True):
    ui = unreal.FbxImportUI()
    ui.import_mesh = True
    ui.import_as_skeletal = False
    ui.import_materials = False
    ui.import_textures = False
    ui.import_animations = False
    ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    sm = ui.static_mesh_import_data
    sm.combine_meshes = combine
    sm.generate_lightmap_u_vs = False
    sm.auto_generate_collision = False
    sm.remove_degenerates = True
    sm.convert_scene = True
    sm.force_front_x_axis = False
    sm.convert_scene_unit = True
    return ui


def import_texture(path, dest, kind):
    """kind: 'color', 'normal', 'mask'."""
    objs = import_files([path], dest)
    if not objs:
        log("FAILED texture " + path)
        return None
    tex = eal.load_asset(objs[0])
    if kind == "normal":
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("srgb", False)
    elif kind == "mask":
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        tex.set_editor_property("srgb", False)
    else:
        tex.set_editor_property("srgb", True)
    eal.save_loaded_asset(tex)
    return tex


def _tex_param(mat, name, x, y, sampler_type, default_tex=None):
    node = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("sampler_type", sampler_type)
    if default_tex is not None:
        node.set_editor_property("texture", default_tex)
    return node


def ensure_master_material():
    """Opaque PBR master: BaseColor, Normal, Metallic, Roughness, AO maps + tint / emissive scalars."""
    if eal.does_asset_exist(MASTER_MATERIAL_PATH):
        return eal.load_asset(MASTER_MATERIAL_PATH)
    folder, name = MASTER_MATERIAL_PATH.rsplit("/", 1)
    mat = asset_tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())

    white = unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture")
    flat_n = unreal.load_asset("/Engine/EngineMaterials/DefaultNormal.DefaultNormal")
    ST = unreal.MaterialSamplerType

    base = _tex_param(mat, "BaseColor", -800, -300, ST.SAMPLERTYPE_COLOR, white)
    tint = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -800, -500)
    tint.set_editor_property("parameter_name", "Tint")
    tint.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
    mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -400, -350)
    mel.connect_material_expressions(base, "RGB", mul, "A")
    mel.connect_material_expressions(tint, "", mul, "B")
    mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_BASE_COLOR)

    nrm = _tex_param(mat, "Normal", -800, 0, ST.SAMPLERTYPE_NORMAL, flat_n)
    mel.connect_material_property(nrm, "RGB", unreal.MaterialProperty.MP_NORMAL)

    met = _tex_param(mat, "Metallic", -800, 250, ST.SAMPLERTYPE_LINEAR_GRAYSCALE, white)
    mel.connect_material_property(met, "R", unreal.MaterialProperty.MP_METALLIC)

    rough = _tex_param(mat, "Roughness", -800, 500, ST.SAMPLERTYPE_LINEAR_GRAYSCALE, white)
    mel.connect_material_property(rough, "R", unreal.MaterialProperty.MP_ROUGHNESS)

    ao = _tex_param(mat, "AO", -800, 750, ST.SAMPLERTYPE_LINEAR_GRAYSCALE, white)
    mel.connect_material_property(ao, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)

    emis = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -800, 1000)
    emis.set_editor_property("parameter_name", "Emissive")
    emis.set_editor_property("default_value", unreal.LinearColor(0, 0, 0, 1))
    mel.connect_material_property(emis, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    log("Created master material " + MASTER_MATERIAL_PATH)
    return mat


def make_pbr_instance(name, dest, textures, tint=None, emissive=None, two_sided=False):
    """textures: dict param-name -> Texture2D (BaseColor/Normal/Metallic/Roughness/AO)."""
    master = ensure_master_material()
    path = dest + "/" + name
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    mi = asset_tools.create_asset(name, dest, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, master)
    for param, tex in textures.items():
        if tex is not None:
            mel.set_material_instance_texture_parameter_value(mi, param, tex)
    if tint is not None:
        mel.set_material_instance_vector_parameter_value(mi, "Tint", tint)
    if emissive is not None:
        mel.set_material_instance_vector_parameter_value(mi, "Emissive", emissive)
    if two_sided:
        overrides = mi.get_editor_property("base_property_overrides")
        overrides.set_editor_property("override_two_sided", True)
        overrides.set_editor_property("two_sided", True)
        mi.set_editor_property("base_property_overrides", overrides)
    eal.save_loaded_asset(mi)
    return mi


def describe_mesh(mesh):
    b = mesh.get_bounding_box()
    size = b.max - b.min
    return "min(%.1f, %.1f, %.1f) max(%.1f, %.1f, %.1f) size(%.1f, %.1f, %.1f) slots=%d" % (
        b.min.x, b.min.y, b.min.z, b.max.x, b.max.y, b.max.z, size.x, size.y, size.z,
        len(mesh.static_materials))


RETICLE_MATERIAL_PATH = IMPORT_ROOT + "/Materials/M_ZS_Reticle"


def ensure_reticle_material():
    """Masked, unlit, two-sided glowing reticle: OpacityMask = Opacity texture, Emissive = Color * Intensity."""
    if eal.does_asset_exist(RETICLE_MATERIAL_PATH):
        return eal.load_asset(RETICLE_MATERIAL_PATH)
    folder, name = RETICLE_MATERIAL_PATH.rsplit("/", 1)
    mat = asset_tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("two_sided", True)

    white = unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture")
    opacity = _tex_param(mat, "Opacity", -800, 0, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE, white)
    color = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -800, -250)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(1.0, 0.05, 0.02, 1))
    intensity = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -800, -400)
    intensity.set_editor_property("parameter_name", "Intensity")
    intensity.set_editor_property("default_value", 12.0)
    mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -400, -300)
    mel.connect_material_expressions(color, "", mul, "A")
    mel.connect_material_expressions(intensity, "", mul, "B")
    mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(opacity, "R", unreal.MaterialProperty.MP_OPACITY_MASK)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    log("Created reticle material " + RETICLE_MATERIAL_PATH)
    return mat
