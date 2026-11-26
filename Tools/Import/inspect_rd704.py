import os, sys, unreal
sys.path.append(os.path.dirname(__file__))
import zs_import_common as zs
for a in sorted(zs.eal.list_assets(zs.IMPORT_ROOT + "/Weapons/RD704", recursive=False)):
    o = zs.eal.load_asset(a)
    if isinstance(o, unreal.StaticMesh):
        zs.log("%-14s %s tris=%d" % (o.get_name(), zs.describe_mesh(o), o.get_num_triangles(0)))
m = zs.eal.load_asset(zs.MASTER_MATERIAL_PATH)
zs.log("master expressions=%d" % zs.mel.get_num_material_expressions(m))
