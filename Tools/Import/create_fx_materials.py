"""Create our own FX materials (versioned, /Game/ZS/Materials):
 - M_ZS_Glow: unlit translucent fresnel glow. Params: Color (vector), Opacity (scalar), Fresnel (scalar).
   Used as the overlay on special zombies, and for spit projectiles, acid pools and power-ups.
 - M_ZS_Rain: lit translucent rain streak (lit, so it reads at any exposure) for AZSRain's instanced planes. The fall is animated on the GPU with
   World Position Offset (time + per-instance random, wrapped over Height), with a little wind slant.
   Params: Color, Opacity, Speed (cm/s), Height (cm, = the rain box height), Wind (x drift per cm fallen).
"""
import os
import sys
import unreal

sys.path.append(os.path.dirname(__file__))
import zs_import_common as zs  # noqa: E402

mel = unreal.MaterialEditingLibrary
PATH = "/Game/ZS/Materials"
NAME = "M_ZS_Glow"
full = PATH + "/" + NAME
if zs.eal.does_asset_exist(full):
    zs.eal.delete_asset(full)

mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, PATH, unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property("two_sided", True)

color = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -600, 0)
color.set_editor_property("parameter_name", "Color")
color.set_editor_property("default_value", unreal.LinearColor(1.0, 0.2, 0.05, 1.0))
fres = mel.create_material_expression(mat, unreal.MaterialExpressionFresnel, -600, 250)
fpow = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -800, 250)
fpow.set_editor_property("parameter_name", "Fresnel")
fpow.set_editor_property("default_value", 2.0)
mel.connect_material_expressions(fpow, "", fres, "ExponentIn")
opac = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -600, 400)
opac.set_editor_property("parameter_name", "Opacity")
opac.set_editor_property("default_value", 0.6)
boost = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -600, 120)
boost.set_editor_property("r", 4.0)

# Emissive = Color * 4 * (0.35 + Fresnel); Opacity = Opacity * (0.25 + Fresnel)
add1 = mel.create_material_expression(mat, unreal.MaterialExpressionAdd, -350, 250)
add1.set_editor_property("const_b", 0.35)
mel.connect_material_expressions(fres, "", add1, "A")
mul1 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -350, 50)
mel.connect_material_expressions(color, "", mul1, "A")
mel.connect_material_expressions(boost, "", mul1, "B")
mul2 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -150, 100)
mel.connect_material_expressions(mul1, "", mul2, "A")
mel.connect_material_expressions(add1, "", mul2, "B")
mel.connect_material_property(mul2, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

add2 = mel.create_material_expression(mat, unreal.MaterialExpressionAdd, -350, 400)
add2.set_editor_property("const_b", 0.25)
mel.connect_material_expressions(fres, "", add2, "A")
mul3 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -150, 400)
mel.connect_material_expressions(opac, "", mul3, "A")
mel.connect_material_expressions(add2, "", mul3, "B")
sat = mel.create_material_expression(mat, unreal.MaterialExpressionSaturate, 0, 400)
mel.connect_material_expressions(mul3, "", sat, "")
mel.connect_material_property(sat, "", unreal.MaterialProperty.MP_OPACITY)

mel.recompile_material(mat)
zs.eal.save_asset(full)
zs.log("Saved " + full)


def make_rain():
    name = "M_ZS_Rain"
    path = PATH + "/" + name
    if zs.eal.does_asset_exist(path):
        zs.eal.delete_asset(path)
    m = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, PATH, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    m.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_VOLUMETRIC_NON_DIRECTIONAL)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("used_with_instanced_static_meshes", True)

    def param(cls, n, v, x, y):
        e = mel.create_material_expression(m, cls, x, y)
        e.set_editor_property("parameter_name", n)
        e.set_editor_property("default_value", v)
        return e

    col = param(unreal.MaterialExpressionVectorParameter, "Color", unreal.LinearColor(0.62, 0.66, 0.72, 1.0), -600, -200)
    opa = param(unreal.MaterialExpressionScalarParameter, "Opacity", 0.22, -800, 100)
    spd = param(unreal.MaterialExpressionScalarParameter, "Speed", 1100.0, -1400, 400)
    hgt = param(unreal.MaterialExpressionScalarParameter, "Height", 2000.0, -1400, 550)
    wnd = param(unreal.MaterialExpressionScalarParameter, "Wind", 0.12, -600, 700)

    # Lit: base colour (plus a touch of emissive so streaks never go fully dark against the sky)
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -400, -100)
    rough.set_editor_property("r", 0.15)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    # Opacity * streak fade along U
    uv = mel.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -1200, 150)
    msk = mel.create_material_expression(m, unreal.MaterialExpressionComponentMask, -1050, 150)
    msk.set_editor_property("r", True); msk.set_editor_property("g", False)
    mel.connect_material_expressions(uv, "", msk, "")
    m2 = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -900, 150)
    m2.set_editor_property("const_b", 2.0)
    mel.connect_material_expressions(msk, "", m2, "A")
    s1 = mel.create_material_expression(m, unreal.MaterialExpressionSubtract, -800, 150)
    s1.set_editor_property("const_b", 1.0)
    mel.connect_material_expressions(m2, "", s1, "A")
    ab = mel.create_material_expression(m, unreal.MaterialExpressionAbs, -700, 150)
    mel.connect_material_expressions(s1, "", ab, "")
    om = mel.create_material_expression(m, unreal.MaterialExpressionOneMinus, -600, 150)
    mel.connect_material_expressions(ab, "", om, "")
    op = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -400, 120)
    mel.connect_material_expressions(opa, "", op, "A")
    mel.connect_material_expressions(om, "", op, "B")
    mel.connect_material_property(op, "", unreal.MaterialProperty.MP_OPACITY)

    # WPO: fall = -frac(Time * Speed / Height + PerInstanceRandom) * Height ; x drift = fall * Wind
    t = mel.create_material_expression(m, unreal.MaterialExpressionTime, -1400, 300)
    ts = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -1200, 350)
    mel.connect_material_expressions(t, "", ts, "A")
    mel.connect_material_expressions(spd, "", ts, "B")
    dv = mel.create_material_expression(m, unreal.MaterialExpressionDivide, -1050, 400)
    mel.connect_material_expressions(ts, "", dv, "A")
    mel.connect_material_expressions(hgt, "", dv, "B")
    rnd = mel.create_material_expression(m, unreal.MaterialExpressionPerInstanceRandom, -1050, 520)
    ad = mel.create_material_expression(m, unreal.MaterialExpressionAdd, -900, 450)
    mel.connect_material_expressions(dv, "", ad, "A")
    mel.connect_material_expressions(rnd, "", ad, "B")
    fr = mel.create_material_expression(m, unreal.MaterialExpressionFrac, -800, 450)
    mel.connect_material_expressions(ad, "", fr, "")
    neg = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -700, 500)
    mel.connect_material_expressions(fr, "", neg, "A")
    mel.connect_material_expressions(hgt, "", neg, "B")
    fall = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -600, 500)
    fall.set_editor_property("const_b", -1.0)
    mel.connect_material_expressions(neg, "", fall, "A")
    dx = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -450, 650)
    mel.connect_material_expressions(fall, "", dx, "A")
    mel.connect_material_expressions(wnd, "", dx, "B")
    zero = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -450, 750)
    a1 = mel.create_material_expression(m, unreal.MaterialExpressionAppendVector, -300, 650)
    mel.connect_material_expressions(dx, "", a1, "A")
    mel.connect_material_expressions(zero, "", a1, "B")
    a2 = mel.create_material_expression(m, unreal.MaterialExpressionAppendVector, -150, 600)
    mel.connect_material_expressions(a1, "", a2, "A")
    mel.connect_material_expressions(fall, "", a2, "B")
    mel.connect_material_property(a2, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)

    mel.recompile_material(m)
    zs.eal.save_asset(path)
    zs.log("Saved " + path)


make_rain()
