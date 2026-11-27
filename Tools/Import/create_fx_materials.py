"""Create our own FX materials (versioned, /Game/ZS/Materials):
 - M_ZS_Glow: unlit translucent fresnel glow. Params: Color (vector), Opacity (scalar), Fresnel (scalar).
   Used as the overlay on special zombies, and for spit projectiles, acid pools and power-ups.
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
