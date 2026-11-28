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
  M_ZS_BloodOverlay per-zombie wound blood + special rim glow (mesh overlay). Pass 'overlay' to rebuild only it.
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


WOUND_SLOTS = 8

WOUND_HLSL = r"""
float3 Wp[8] = {W0, W1, W2, W3, W4, W5, W6, W7};
float3 Wd[8] = {D0, D1, D2, D3, D4, D5, D6, D7};
float  Wr[8] = {R0, R1, R2, R3, R4, R5, R6, R7};
float a = 0;
for (int i = 0; i < 8; i++)
{
    if (Wr[i] <= 0.01) continue;
    // Each wound is the bullet's path through the body: a short cylinder (axis Wd, |Wd| = half length) centred in the
    // body, so the stain shows on the clothes where it went in and where it came out. Wd = 0 -> a plain blob.
    float3 d = P - Wp[i];
    float3 ax = Wd[i];
    float t = dot(d, ax) / max(dot(ax, ax), 1.0);
    float3 perp = d - ax * t;
    // Blood runs down from the wound: below it the vertical distance counts for less, above it for more.
    perp.z *= (perp.z < 0) ? 0.32 : 1.4;
    float r = Wr[i] * (0.7 + 0.6 * N);
    a = max(a, saturate((r - length(perp)) / (0.45 * r)) * saturate((1.0 - abs(t)) * 3.0));
}
return a;
"""


def build_blood_overlay():
    """Per-zombie wound blood (mesh overlay). Up to 8 wounds, each pinned to the body in pre-skinned space where a bullet
    landed: W0..W7 = centre of the bullet's path through the body (mesh space, cm), D0..D7 = path direction x half
    length, R0..R7 = radius (0 = unused). Ragged edges (noise, Seed), runs
    downward. Also carries the special-zombie rim glow (GlowColor, GlowOpacity, Fresnel) so one overlay does both."""
    mat = fresh_material("M_ZS_BloodOverlay")
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    # Volumetric lighting: no sky-reflection sheen (surface lighting made the blotches read pink-white in the sun).
    mat.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_VOLUMETRIC_NON_DIRECTIONAL)
    mat.set_editor_property("used_with_skeletal_mesh", True)
    raw_pos = expr(mat, unreal.MaterialExpressionPreSkinnedPosition, -1800, 0)
    # Pre-skinned position only exists in the vertex shader: hand it to the pixel shader through an interpolator.
    pos = expr(mat, unreal.MaterialExpressionVertexInterpolator, -1600, 0)
    link(raw_pos, pos, "VS")
    seed = scalar(mat, "Seed", 0.0, -1500, 150)
    sp = expr(mat, unreal.MaterialExpressionAdd, -1300, 50)
    link(pos, sp, "A")
    link(seed, sp, "B")
    n = expr(mat, unreal.MaterialExpressionNoise, -1100, 50, scale=0.18, levels=4, output_min=0.0, output_max=1.0)
    link(sp, n, "Position")

    c = expr(mat, unreal.MaterialExpressionCustom, -700, 0)
    c.set_editor_property("code", WOUND_HLSL)
    c.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    c.set_editor_property("description", "Wounds")
    names = (["P", "N"] + ["W%d" % i for i in range(WOUND_SLOTS)] + ["D%d" % i for i in range(WOUND_SLOTS)]
             + ["R%d" % i for i in range(WOUND_SLOTS)])
    inputs = []
    for nm in names:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", nm)
        inputs.append(ci)
    c.set_editor_property("inputs", inputs)
    link(pos, c, "P")
    link(n, c, "N")
    for i in range(WOUND_SLOTS):
        w = vector(mat, "W%d" % i, unreal.LinearColor(0, 0, 0, 0), -1100, 250 + i * 160)
        link(w, c, "W%d" % i)
        dv = vector(mat, "D%d" % i, unreal.LinearColor(0, 0, 0, 0), -1400, 250 + i * 160)
        link(dv, c, "D%d" % i)
        r = scalar(mat, "R%d" % i, 0.0, -1100, 1600 + i * 100)
        link(r, c, "R%d" % i)

    # Rim glow for specials, same look as M_ZS_Glow: opacity = GlowOpacity * (0.25 + fresnel),
    # emissive = GlowColor * 4 * (0.35 + fresnel), faded out under blood.
    fres_exp = scalar(mat, "Fresnel", 2.5, -900, -500)
    fres = expr(mat, unreal.MaterialExpressionFresnel, -700, -500)
    link(fres_exp, fres, "ExponentIn")
    f_op = expr(mat, unreal.MaterialExpressionAdd, -550, -450, const_b=0.25)
    link(fres, f_op, "A")
    glow_op = scalar(mat, "GlowOpacity", 0.0, -700, -350)
    glow_a = expr(mat, unreal.MaterialExpressionMultiply, -400, -450)
    link(f_op, glow_a, "A")
    link(glow_op, glow_a, "B")

    wound_a = expr(mat, unreal.MaterialExpressionMultiply, -450, 0, const_b=0.95)
    link(c, wound_a, "A")
    op = expr(mat, unreal.MaterialExpressionMax, -250, -100)
    link(wound_a, op, "A")
    link(glow_a, op, "B")
    mel.connect_material_property(op, "", unreal.MaterialProperty.MP_OPACITY)

    # Colour: wet dark-red core, brighter red where it soaks out at the edges.
    dark = expr(mat, unreal.MaterialExpressionConstant3Vector, -500, -800, constant=unreal.LinearColor(0.045, 0.002, 0.001, 1))
    red = expr(mat, unreal.MaterialExpressionConstant3Vector, -500, -950, constant=unreal.LinearColor(0.13, 0.008, 0.004, 1))
    col = expr(mat, unreal.MaterialExpressionLinearInterpolate, -300, -850)
    link(red, col, "A")
    link(dark, col, "B")
    link(c, col, "Alpha")
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = expr(mat, unreal.MaterialExpressionConstant, -300, -700, r=0.25)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    glow_col = vector(mat, "GlowColor", unreal.LinearColor(0, 0, 0, 1), -700, -650)
    f_em = expr(mat, unreal.MaterialExpressionAdd, -550, -600, const_b=0.35)
    link(fres, f_em, "A")
    keep = expr(mat, unreal.MaterialExpressionOneMinus, -300, -600)
    link(c, keep)
    em = expr(mat, unreal.MaterialExpressionMultiply, -150, -650)
    link(glow_col, em, "A")
    link(f_em, em, "B")
    em2 = expr(mat, unreal.MaterialExpressionMultiply, -50, -650)
    link(em, em2, "A")
    link(keep, em2, "B")
    em3 = expr(mat, unreal.MaterialExpressionMultiply, 50, -650, const_b=4.0)
    link(em2, em3, "A")
    # Lit translucency doesn't fade emissive by opacity the way the unlit glow did, so pre-multiply it.
    em4 = expr(mat, unreal.MaterialExpressionMultiply, 150, -650)
    link(em3, em4, "A")
    link(glow_a, em4, "B")
    mel.connect_material_property(em4, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
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


if "overlay" in sys.argv[1:]:
    build_blood_overlay()   # just the wound overlay (fast iteration)
    zs.log("gore fx: blood overlay rebuilt")
    raise SystemExit(0)

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
