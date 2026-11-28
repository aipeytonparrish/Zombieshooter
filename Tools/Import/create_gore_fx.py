"""Import our generated FX textures and build the gore / impact materials (versioned, /Game/ZS).

  python Tools/Import/gen_fx_textures.py Saved/GeneratedFX          (plain Python, makes the PNGs)
  UnrealEditor-Cmd.exe Zombieshooter.uproject -run=pythonscript -script="Tools/Import/create_gore_fx.py"

Creates in /Game/ZS/Textures/FX: T_ZS_BloodSplat (2x2 atlas), T_ZS_BloodPool, T_ZS_BulletHole (2x2 atlas), T_ZS_Puff, T_ZS_Drop,
and the HUD's T_ZS_HudPlate (grungy panel) / T_ZS_HudSmear (blood brush stroke)
and in /Game/ZS/Materials:
  M_ZS_DecalAtlas   deferred decal. Params: Tex, Atlas (0.5 = 2x2 atlas, 1 = single), Frame (0-3), Tint, Opacity,
                    RoughThin / RoughThick (wet blood is glossy where thick), Metal.
    MI_ZS_BloodSplat, MI_ZS_BloodPool, MI_ZS_BulletHole
  M_ZS_FXSprite     lit translucent billboard for our instanced particles. Per-instance custom data:
                    0 = opacity, 1-3 = colour. Param Tex.  MI_ZS_FXPuff (mist / dust), MI_ZS_FXDrop (droplets)
  M_ZS_FXSpark      unlit additive streak (custom data 0 = opacity, 1-3 = colour) for metal sparks / muzzle embers
  M_ZS_Flesh        wet, mottled flesh for gibs
  M_ZS_Paint        plain lit paint for placeholders. Params: Color, Roughness, Metallic, Emissive (scalar, x Color).
                    (The engine's BasicShapeMaterial ignores a "Color" parameter in game, which left placeholders white.)
"""
import os
import sys

import unreal

sys.path.append(os.path.dirname(__file__))
import zs_import_common as zs  # noqa: E402

mel = unreal.MaterialEditingLibrary
TEX = "/Game/ZS/Textures/FX"
MAT = "/Game/ZS/Materials"
SRC = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", "..", "Saved", "GeneratedFX"))


def expr(mat, cls, x, y, **props):
    node = mel.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        node.set_editor_property(k, v)
    return node


def scalar(mat, name, default, x, y):
    return expr(mat, unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=default)


def vector(mat, name, default, x, y):
    return expr(mat, unreal.MaterialExpressionVectorParameter, x, y, parameter_name=name, default_value=default)


def link(a, b, pin="", out=""):
    mel.connect_material_expressions(a, out, b, pin)


def fresh_material(name):
    path = MAT + "/" + name
    if zs.eal.does_asset_exist(path):
        zs.eal.delete_asset(path)
    return zs.asset_tools.create_asset(name, MAT, unreal.Material, unreal.MaterialFactoryNew())


def fresh_instance(name, parent, scalars=None, vectors=None, textures=None):
    path = MAT + "/" + name
    if zs.eal.does_asset_exist(path):
        zs.eal.delete_asset(path)
    mi = zs.asset_tools.create_asset(name, MAT, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    for k, v in (scalars or {}).items():
        mel.set_material_instance_scalar_parameter_value(mi, k, v)
    for k, v in (vectors or {}).items():
        mel.set_material_instance_vector_parameter_value(mi, k, v)
    for k, v in (textures or {}).items():
        mel.set_material_instance_texture_parameter_value(mi, k, v)
    zs.eal.save_loaded_asset(mi)
    return mi


def import_textures():
    out = {}
    for name in ("T_ZS_BloodSplat", "T_ZS_BloodPool", "T_ZS_BulletHole", "T_ZS_Puff", "T_ZS_Drop", "T_ZS_HudPlate", "T_ZS_HudSmear"):
        png = os.path.join(SRC, name + ".png")
        if not os.path.exists(png):
            raise RuntimeError("missing %s - run gen_fx_textures.py first" % png)
        tex = zs.import_texture(png, TEX, "color")
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
        if name.startswith("T_ZS_Hud"):
            tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
            tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        else:
            tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_EFFECTS)
        zs.eal.save_loaded_asset(tex)
        out[name] = tex
    return out


def atlas_uv(mat, x, y):
    """UV = (TexCoord + (fmod(Frame, 2), floor(Frame / 2))) * Atlas"""
    uv = expr(mat, unreal.MaterialExpressionTextureCoordinate, x - 900, y)
    frame = scalar(mat, "Frame", 0.0, x - 1100, y + 150)
    atlas = scalar(mat, "Atlas", 0.5, x - 400, y + 250)
    fx = expr(mat, unreal.MaterialExpressionFmod, x - 900, y + 150)
    link(frame, fx, "A")
    two = expr(mat, unreal.MaterialExpressionConstant, x - 1100, y + 300, r=2.0)
    link(two, fx, "B")
    half = expr(mat, unreal.MaterialExpressionMultiply, x - 900, y + 300, const_b=0.5)
    link(frame, half, "A")
    fy = expr(mat, unreal.MaterialExpressionFloor, x - 750, y + 300)
    link(half, fy)
    off = expr(mat, unreal.MaterialExpressionAppendVector, x - 600, y + 200)
    link(fx, off, "A")
    link(fy, off, "B")
    add = expr(mat, unreal.MaterialExpressionAdd, x - 450, y + 50)
    link(uv, add, "A")
    link(off, add, "B")
    mul = expr(mat, unreal.MaterialExpressionMultiply, x - 250, y + 100)
    link(add, mul, "A")
    link(atlas, mul, "B")
    return mul


def build_decal_master(default_tex):
    mat = fresh_material("M_ZS_DecalAtlas")
    mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    uv = atlas_uv(mat, -600, 0)
    tex = expr(mat, unreal.MaterialExpressionTextureSampleParameter2D, -300, 0, parameter_name="Tex", texture=default_tex)
    link(uv, tex, "UVs")
    tint = vector(mat, "Tint", unreal.LinearColor(1, 1, 1, 1), -300, -250)
    col = expr(mat, unreal.MaterialExpressionMultiply, -50, -150)
    link(tex, col, "A", "RGB")
    link(tint, col, "B")
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)

    opacity = scalar(mat, "Opacity", 1.0, -300, 300)
    op = expr(mat, unreal.MaterialExpressionMultiply, -50, 250)
    link(tex, op, "A", "A")
    link(opacity, op, "B")
    mel.connect_material_property(op, "", unreal.MaterialProperty.MP_OPACITY)

    rthin = scalar(mat, "RoughThin", 0.35, -300, 450)
    rthick = scalar(mat, "RoughThick", 0.08, -300, 550)
    rl = expr(mat, unreal.MaterialExpressionLinearInterpolate, -50, 450)
    link(rthin, rl, "A")
    link(rthick, rl, "B")
    link(tex, rl, "Alpha", "A")
    mel.connect_material_property(rl, "", unreal.MaterialProperty.MP_ROUGHNESS)
    metal = scalar(mat, "Metal", 0.0, -50, 650)
    mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
    mel.recompile_material(mat)
    zs.eal.save_loaded_asset(mat)
    return mat


def build_sprite():
    mat = fresh_material("M_ZS_FXSprite")
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("two_sided", True)
    mat.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_VOLUMETRIC_PER_VERTEX_NON_DIRECTIONAL)
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    tex = expr(mat, unreal.MaterialExpressionTextureSampleParameter2D, -500, 0, parameter_name="Tex",
               texture=unreal.load_asset(TEX + "/T_ZS_Puff"))
    op = expr(mat, unreal.MaterialExpressionPerInstanceCustomData, -500, 250, data_index=0, const_default_value=1.0)
    col = expr(mat, unreal.MaterialExpressionPerInstanceCustomData3Vector, -500, -250, data_index=1,
               const_default_value=unreal.LinearColor(0.2, 0.0, 0.0, 1))
    c = expr(mat, unreal.MaterialExpressionMultiply, -200, -150)
    link(tex, c, "A", "RGB")
    link(col, c, "B")
    mel.connect_material_property(c, "", unreal.MaterialProperty.MP_BASE_COLOR)
    o = expr(mat, unreal.MaterialExpressionMultiply, -200, 150)
    link(tex, o, "A", "A")
    link(op, o, "B")
    mel.connect_material_property(o, "", unreal.MaterialProperty.MP_OPACITY)
    rough = expr(mat, unreal.MaterialExpressionConstant, -200, 300, r=0.3)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(mat)
    zs.eal.save_loaded_asset(mat)
    return mat


def build_spark():
    mat = fresh_material("M_ZS_FXSpark")
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("two_sided", True)
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    tex = expr(mat, unreal.MaterialExpressionTextureSample, -500, 0, texture=unreal.load_asset(TEX + "/T_ZS_Drop"))
    op = expr(mat, unreal.MaterialExpressionPerInstanceCustomData, -500, 250, data_index=0, const_default_value=1.0)
    col = expr(mat, unreal.MaterialExpressionPerInstanceCustomData3Vector, -500, -250, data_index=1,
               const_default_value=unreal.LinearColor(1.0, 0.5, 0.15, 1))
    m1 = expr(mat, unreal.MaterialExpressionMultiply, -250, 0)
    link(tex, m1, "A", "A")
    link(op, m1, "B")
    m2 = expr(mat, unreal.MaterialExpressionMultiply, -100, -100)
    link(m1, m2, "A")
    link(col, m2, "B")
    m3 = expr(mat, unreal.MaterialExpressionMultiply, 50, -100, const_b=30.0)
    link(m2, m3, "A")
    mel.connect_material_property(m3, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    zs.eal.save_loaded_asset(mat)
    return mat


def build_flesh():
    mat = fresh_material("M_ZS_Flesh")
    noise = expr(mat, unreal.MaterialExpressionNoise, -600, 0, scale=0.35, levels=3, output_min=0.0, output_max=1.0)
    dark = expr(mat, unreal.MaterialExpressionConstant3Vector, -600, -250, constant=unreal.LinearColor(0.045, 0.003, 0.003, 1))
    raw = expr(mat, unreal.MaterialExpressionConstant3Vector, -600, -400, constant=unreal.LinearColor(0.32, 0.06, 0.05, 1))
    l = expr(mat, unreal.MaterialExpressionLinearInterpolate, -300, -250)
    link(dark, l, "A")
    link(raw, l, "B")
    link(noise, l, "Alpha")
    mel.connect_material_property(l, "", unreal.MaterialProperty.MP_BASE_COLOR)
    r = expr(mat, unreal.MaterialExpressionLinearInterpolate, -300, 100, const_a=0.12, const_b=0.45)
    link(noise, r, "Alpha")
    mel.connect_material_property(r, "", unreal.MaterialProperty.MP_ROUGHNESS)
    spec = expr(mat, unreal.MaterialExpressionConstant, -300, 250, r=0.6)
    mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(mat)
    zs.eal.save_loaded_asset(mat)
    return mat


def build_camo():
    """Gun camo overlay (UMeshComponent overlay material): a 3-colour blotch pattern in the gun's own space, so it stays
    put as the gun moves. Params: ColorA/B/C, Scale, Metal (gold), Emissive (glow)."""
    mat = fresh_material("M_ZS_Camo")
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE)
    mat.set_editor_property("used_with_skeletal_mesh", True)
    pos = expr(mat, unreal.MaterialExpressionLocalPosition, -1300, 0)
    sc = scalar(mat, "Scale", 1.0, -1300, 150)
    p = expr(mat, unreal.MaterialExpressionMultiply, -1100, 50)
    link(pos, p, "A")
    link(sc, p, "B")
    n1 = expr(mat, unreal.MaterialExpressionNoise, -900, 0, scale=0.06, levels=3, output_min=0.0, output_max=1.0)
    link(p, n1, "Position")
    off = expr(mat, unreal.MaterialExpressionAdd, -1100, 250)
    link(p, off, "A")
    k = expr(mat, unreal.MaterialExpressionConstant3Vector, -1300, 300, constant=unreal.LinearColor(37.0, 11.0, 23.0, 1))
    link(k, off, "B")
    n2 = expr(mat, unreal.MaterialExpressionNoise, -900, 250, scale=0.09, levels=2, output_min=0.0, output_max=1.0)
    link(off, n2, "Position")

    def mask(n, th, x, y):
        s = expr(mat, unreal.MaterialExpressionSubtract, x, y, const_b=th)
        link(n, s, "A")
        m = expr(mat, unreal.MaterialExpressionMultiply, x + 150, y, const_b=14.0)
        link(s, m, "A")
        sat = expr(mat, unreal.MaterialExpressionSaturate, x + 300, y)
        link(m, sat)
        return sat
    m1 = mask(n1, 0.46, -700, 0)
    m2 = mask(n2, 0.56, -700, 250)
    ca = vector(mat, "ColorA", unreal.LinearColor(0.02, 0.025, 0.012, 1), -500, -400)
    cb = vector(mat, "ColorB", unreal.LinearColor(0.07, 0.09, 0.03, 1), -500, -250)
    cc = vector(mat, "ColorC", unreal.LinearColor(0.16, 0.13, 0.07, 1), -500, -100)
    l1 = expr(mat, unreal.MaterialExpressionLinearInterpolate, -250, -300)
    link(ca, l1, "A")
    link(cb, l1, "B")
    link(m1, l1, "Alpha")
    l2 = expr(mat, unreal.MaterialExpressionLinearInterpolate, -100, -200)
    link(l1, l2, "A")
    link(cc, l2, "B")
    link(m2, l2, "Alpha")
    mel.connect_material_property(l2, "", unreal.MaterialProperty.MP_BASE_COLOR)
    metal = scalar(mat, "Metal", 0.0, -250, 100)
    mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
    rough = expr(mat, unreal.MaterialExpressionLinearInterpolate, -100, 150, const_a=0.6, const_b=0.15)
    link(metal, rough, "Alpha")
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    em = scalar(mat, "Emissive", 0.0, -250, 300)
    emc = expr(mat, unreal.MaterialExpressionMultiply, -100, 300)
    link(l2, emc, "A")
    link(em, emc, "B")
    mel.connect_material_property(emc, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    op = expr(mat, unreal.MaterialExpressionConstant, -100, 450, r=0.96)
    mel.connect_material_property(op, "", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(mat)
    zs.eal.save_loaded_asset(mat)
    return mat


def build_blood_overlay():
    """Zombies soak with blood as they're shot (mesh overlay). Blotches pinned to the body (pre-skinned position), so
    they move with it; Amount 0..1 grows them, Seed varies the pattern per zombie. Dark, wet, glossy."""
    mat = fresh_material("M_ZS_BloodOverlay")
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    # Volumetric lighting: no sky-reflection sheen (surface lighting made the blotches read pink-white in the sun).
    mat.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_VOLUMETRIC_NON_DIRECTIONAL)
    mat.set_editor_property("used_with_skeletal_mesh", True)
    pos = expr(mat, unreal.MaterialExpressionPreSkinnedPosition, -1300, 0)
    seed = scalar(mat, "Seed", 0.0, -1300, 150)
    sp = expr(mat, unreal.MaterialExpressionAdd, -1100, 50)
    link(pos, sp, "A")
    link(seed, sp, "B")
    n = expr(mat, unreal.MaterialExpressionNoise, -900, 0, scale=0.05, levels=4, output_min=0.0, output_max=1.0)
    link(sp, n, "Position")
    amount = scalar(mat, "Amount", 0.0, -900, 250)
    inv = expr(mat, unreal.MaterialExpressionOneMinus, -700, 250)
    link(amount, inv)
    sub_ = expr(mat, unreal.MaterialExpressionSubtract, -550, 100)
    link(n, sub_, "A")
    link(inv, sub_, "B")
    mul = expr(mat, unreal.MaterialExpressionMultiply, -400, 100, const_b=10.0)
    link(sub_, mul, "A")
    sat = expr(mat, unreal.MaterialExpressionSaturate, -250, 100)
    link(mul, sat)
    op = expr(mat, unreal.MaterialExpressionMultiply, -100, 100, const_b=0.97)
    link(sat, op, "A")
    mel.connect_material_property(op, "", unreal.MaterialProperty.MP_OPACITY)
    # Thicker (deeper into the blotch) = darker, edges a little brighter red.
    dark = expr(mat, unreal.MaterialExpressionConstant3Vector, -400, -250, constant=unreal.LinearColor(0.018, 0.0, 0.0, 1))
    red = expr(mat, unreal.MaterialExpressionConstant3Vector, -400, -400, constant=unreal.LinearColor(0.07, 0.003, 0.002, 1))
    col = expr(mat, unreal.MaterialExpressionLinearInterpolate, -200, -300)
    link(red, col, "A")
    link(dark, col, "B")
    link(sat, col, "Alpha")
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.recompile_material(mat)
    zs.eal.save_loaded_asset(mat)
    return mat


def build_paint():
    mat = fresh_material("M_ZS_Paint")
    color = vector(mat, "Color", unreal.LinearColor(0.5, 0.5, 0.5, 1), -500, -100)
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = scalar(mat, "Roughness", 0.65, -500, 100)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    metal = scalar(mat, "Metallic", 0.0, -500, 200)
    mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
    emis = scalar(mat, "Emissive", 0.0, -500, 300)
    em = expr(mat, unreal.MaterialExpressionMultiply, -250, 300)
    link(color, em, "A")
    link(emis, em, "B")
    mel.connect_material_property(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    zs.eal.save_loaded_asset(mat)
    return mat


tex = import_textures()
decal = build_decal_master(tex["T_ZS_BloodSplat"])
fresh_instance("MI_ZS_BloodSplat", decal, {"Atlas": 0.5, "RoughThin": 0.3, "RoughThick": 0.06},
               {"Tint": unreal.LinearColor(0.75, 0.75, 0.75, 1)}, {"Tex": tex["T_ZS_BloodSplat"]})
fresh_instance("MI_ZS_BloodPool", decal, {"Atlas": 1.0, "RoughThin": 0.2, "RoughThick": 0.03},
               {"Tint": unreal.LinearColor(0.6, 0.6, 0.6, 1)}, {"Tex": tex["T_ZS_BloodPool"]})
fresh_instance("MI_ZS_BulletHole", decal, {"Atlas": 0.5, "RoughThin": 0.95, "RoughThick": 0.9},
               {"Tint": unreal.LinearColor(1, 1, 1, 1)}, {"Tex": tex["T_ZS_BulletHole"]})
sprite = build_sprite()
fresh_instance("MI_ZS_FXPuff", sprite, textures={"Tex": tex["T_ZS_Puff"]})
fresh_instance("MI_ZS_FXDrop", sprite, textures={"Tex": tex["T_ZS_Drop"]})
build_spark()
build_flesh()
build_paint()
build_camo()
build_blood_overlay()
zs.log("gore fx: textures + materials created")
