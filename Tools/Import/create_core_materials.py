"""Create Zombieshooter's own (non-third-party) materials. These ARE versioned: /Game/ZS/Materials/.

M_ZS_BloodDecal - deferred decal: dark wet red, splotchy procedural edge (no texture needed).
"""
import os
import sys
import unreal

sys.path.append(os.path.dirname(__file__))
import zs_import_common as zs  # noqa: E402

DEST = "/Game/ZS/Materials"


def expr(mat, cls, x, y, **props):
    node = zs.mel.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        node.set_editor_property(k, v)
    return node


def create_blood_decal():
    path = DEST + "/M_ZS_BloodDecal"
    if zs.eal.does_asset_exist(path):
        zs.eal.delete_asset(path)
    mat = zs.asset_tools.create_asset("M_ZS_BloodDecal", DEST, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)

    # Radial falloff from the decal centre ...
    uv = expr(mat, unreal.MaterialExpressionTextureCoordinate, -1400, 0)
    centre = expr(mat, unreal.MaterialExpressionConstant2Vector, -1400, 150, r=0.5, g=0.5)
    dist = expr(mat, unreal.MaterialExpressionDistance, -1150, 50)
    zs.mel.connect_material_expressions(uv, "", dist, "A")
    zs.mel.connect_material_expressions(centre, "", dist, "B")
    edge = expr(mat, unreal.MaterialExpressionSubtract, -950, 50, const_a=0.42)
    zs.mel.connect_material_expressions(dist, "", edge, "B")
    sharpen = expr(mat, unreal.MaterialExpressionMultiply, -750, 50, const_b=7.0)
    zs.mel.connect_material_expressions(edge, "", sharpen, "A")

    # ... broken up by world-space noise so every splat has a different ragged outline.
    noise = expr(mat, unreal.MaterialExpressionNoise, -950, 300, scale=0.06, levels=4, output_min=-0.9, output_max=0.9)
    add = expr(mat, unreal.MaterialExpressionAdd, -550, 120)
    zs.mel.connect_material_expressions(sharpen, "", add, "A")
    zs.mel.connect_material_expressions(noise, "", add, "B")
    opacity = expr(mat, unreal.MaterialExpressionSaturate, -350, 120)
    zs.mel.connect_material_expressions(add, "", opacity, "")
    zs.mel.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)

    # Darker where thicker.
    dark = expr(mat, unreal.MaterialExpressionConstant3Vector, -550, -250, constant=unreal.LinearColor(0.035, 0.0, 0.0, 1))
    bright = expr(mat, unreal.MaterialExpressionConstant3Vector, -550, -400, constant=unreal.LinearColor(0.12, 0.004, 0.002, 1))
    lerp = expr(mat, unreal.MaterialExpressionLinearInterpolate, -300, -300)
    zs.mel.connect_material_expressions(bright, "", lerp, "A")
    zs.mel.connect_material_expressions(dark, "", lerp, "B")
    zs.mel.connect_material_expressions(opacity, "", lerp, "Alpha")
    zs.mel.connect_material_property(lerp, "", unreal.MaterialProperty.MP_BASE_COLOR)

    rough = expr(mat, unreal.MaterialExpressionConstant, -300, -100, r=0.15)
    zs.mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

    zs.mel.recompile_material(mat)
    zs.eal.save_loaded_asset(mat)
    zs.log("Created " + path)


create_blood_decal()
