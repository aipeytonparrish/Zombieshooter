"""Build /Game/ZS/Maps/L_Rooftop: the safe house between runs - a survivors' camp on top of a New York tower at golden
hour, high above the dead city. Replaces the old underground bunker (L_Bunker) as the hub.

Ours (versioned). Uses the owner's packs (git-ignored): Megascans NYC kitbash buildings and rooftop props, the
industrial kit (roof floor, parapets, the stair house), RealisticFireFX (camp fire, fire barrels), Survivors Mega Pack
+ MC Idles pack (the people living here, AZSSurvivorNPC). Re-run to rebuild.

  UnrealEditor-Cmd.exe Zombieshooter.uproject -run=pythonscript -script=Tools/Import/create_rooftop_map.py
"""
import math
import os
import sys

import unreal

sys.path.append(os.path.dirname(__file__))
import zs_import_common as zs  # noqa: E402

MAP = "/Game/ZS/Maps/L_Rooftop"
KB = "/Game/MS_NewYorkCityKitbash_Assets/Meshes-Buildings/%s"
KBP = "/Game/MS_NewYorkCityKitbash_Assets/Meshes-Assets/%s"
KIT = "/Game/IndustrialKit/%s/StaticMeshes/%s_LOD0"
FIRE = "/Game/RealisticFireFX/FX/Campfire/NS_CampFire_01"
FIRE_SMALL = "/Game/RealisticFireFX/FX/Candle/NS_CandleFire_1_Wild"
MC = "/Game/MC_Idles_Pack/Animations/%s"
PAINT = "/Game/ZS/Materials/M_ZS_Paint"

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
mel = unreal.MaterialEditingLibrary

if zs.eal.does_asset_exist(MAP):
    zs.eal.delete_asset(MAP)
umap = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()), "ZS", "Maps", "L_Rooftop.umap")
if os.path.exists(umap):
    os.remove(umap)
unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(["/Game/ZS/Maps"], True)
if not les.new_level_from_template(MAP, "/Engine/Maps/Templates/Template_Default"):
    raise RuntimeError("new_level_from_template failed")
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def mesh(path):
    m = unreal.load_asset(path)
    if m is None:
        zs.log("missing " + path)
    return m


def place(m, x, y, z, yaw=0.0, scale=1.0, shadow=True, label=None, folder="Rooftop", ground=False):
    """Static mesh actor; ground=True drops it so its bounds' bottom sits at z."""
    if isinstance(m, str):
        m = mesh(m)
    if m is None:
        return None
    if ground:
        z -= m.get_bounding_box().min.z * scale
    a = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, z), unreal.Rotator(0, 0, yaw))
    a.static_mesh_component.set_static_mesh(m)
    a.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    a.static_mesh_component.set_editor_property("cast_shadow", shadow)
    a.set_folder_path(folder)
    if label:
        a.set_actor_label(label)
    return a


def size_of(path):
    m = mesh(path)
    if not m:
        return None
    b = m.get_bounding_box()
    return (b.max.x - b.min.x, b.max.y - b.min.y, b.max.z - b.min.z)


# ---- The tower we stand on: SM_Building12 (31 x 18 m roof, a cosy camp) standing on SM_Building11 (a 29 m city-block
# podium), so the camp is ~55 m up with the podium roof as a setback below the parapet.
podium = mesh(KB % "SM_Building11").get_bounding_box()
place(KB % "SM_Building11", -(podium.min.x + podium.max.x) * 0.5, -(podium.min.y + podium.max.y) * 0.5, -podium.min.z, shadow=False, label="Podium_SM_Building11", folder="City")
base_name = "SM_Building12"
TS = 1.5   # scaled up: a 47 x 27 m roof has room for the camp, the stations and the people
bb = mesh(KB % base_name).get_bounding_box()
top_of_podium = podium.max.z - podium.min.z
place(KB % base_name, -(bb.min.x + bb.max.x) * 0.5 * TS, -(bb.min.y + bb.max.y) * 0.5 * TS, top_of_podium - bb.min.z * TS, scale=TS, shadow=False, label="Tower_" + base_name, folder="City")
ROOF = top_of_podium + (bb.max.z - bb.min.z) * TS + 6
HX = (bb.max.x - bb.min.x) * 0.5 * TS - 60       # usable half extents of the roof
HY = (bb.max.y - bb.min.y) * 0.5 * TS - 60
zs.log("Tower %s, roof at %.0f, roof %.0f x %.0f" % (base_name, ROOF, HX * 2, HY * 2))

# ---- Roof: industrial kit floor tiles + a waist-high parapet, drawn as instanced meshes ----------------------------
inst = {}


def kit(name, x, y, z, yaw, sz=1.0, sx=1.0, sy=1.0):
    inst.setdefault(name, []).append(unreal.Transform(unreal.Vector(x, y, z), unreal.Rotator(0, 0, yaw), unreal.Vector(sx, sy, sz)))


nx, ny = int(HX * 2 // 200), int(HY * 2 // 200)
x0, y0 = -nx * 100, -ny * 100
for ix in range(nx):
    for iy in range(ny):
        h = (ix * 7 + iy * 13) % 9
        kit("SM_Floor_Concrete_2x2" if h else "SM_Floor_Drain_2x2", x0 + 100 + ix * 200, y0 + 100 + iy * 200, ROOF - 3, 0)
for ix in range(nx):
    x = x0 + 100 + ix * 200
    for y, yaw in ((y0, 0), (y0 + ny * 200, 180)):
        kit("SM_Wall_Concrete_2x3", x, y + (12 if yaw == 0 else -12), ROOF, yaw, 0.36)
        kit("SM_Wall_Concrete_2x3", x, y - (12 if yaw == 0 else -12), ROOF, yaw + 180, 0.36)
for iy in range(ny):
    y = y0 + 100 + iy * 200
    for x, yaw in ((x0, -90), (x0 + nx * 200, 90)):
        kit("SM_Wall_Concrete_2x3", x + (12 if yaw == -90 else -12), y, ROOF, yaw, 0.36)
        kit("SM_Wall_Concrete_2x3", x - (12 if yaw == -90 else -12), y, ROOF, yaw + 180, 0.36)

# Stair house (the way down - closed off): a 4 x 4 m kit room in one corner with a boarded door, light over it.
sx, sy = x0 + 300, y0 + 300
for k in range(2):
    kit("SM_Wall_Concrete_2x3", sx - 100 + k * 200, sy - 200 + 12, ROOF, 180)
    kit("SM_Wall_Corrugated_2x3", sx - 100 + k * 200, sy + 200 - 12, ROOF, 0)
    kit("SM_Wall_Concrete_2x3", sx - 200 + 12, sy - 100 + k * 200, ROOF, 90)
    kit("SM_Wall_Pipes_2x3" if k else "SM_Wall_Door_2x3", sx + 200 - 12, sy - 100 + k * 200, ROOF, -90)
    for j in range(2):
        kit("SM_Floor_Concrete_2x2", sx - 100 + k * 200, sy - 100 + j * 200, ROOF + 300, 0)
for pz in (0, 300):
    for cx, cy in ((sx - 200, sy - 200), (sx + 200, sy - 200), (sx - 200, sy + 200), (sx + 200, sy + 200)):
        if pz == 0:
            kit("SM_Pillar_Concrete_3m", cx, cy, ROOF, 0)

kit_actor = eas.spawn_actor_from_class(unreal.ZSKitInstances, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
kit_actor.set_actor_label("Rooftop_Kit")
batches = []
for name, ts in inst.items():
    b = unreal.ZSKitBatch()
    b.set_editor_property("mesh", mesh(KIT % (name, name)))
    b.set_editor_property("instances", ts)
    b.set_editor_property("cast_shadow", not name.startswith("SM_Floor"))
    batches.append(b)
kit_actor.set_editor_property("batches", batches)


# ---- Props ---------------------------------------------------------------------------------------------------------
def prop(name, x, y, yaw=0.0, scale=1.0, maxsize=None, shadow=True):
    s = size_of(KBP % name)
    if s is None:
        return None
    if maxsize and max(s[0], s[1]) * scale > maxsize:
        scale = maxsize / max(s[0], s[1])
    return place(KBP % name, x, y, ROOF, yaw, scale, shadow, label="Prop_" + name, ground=True)


cx, cy = 0.0, HY * 0.15     # camp centre
# Water tank on stilts and a cluster of AC units on the far side; antenna; flag.
prop("SM_WaterTank01", HX - 420, HY - 420, 20, maxsize=650)
prop("SM_WaterTank02", HX - 420, -HY + 500, -30, maxsize=550)
for i, n in enumerate(["SM_AirConditioner02", "SM_AirConditioner04", "SM_AirConditioner06", "SM_AirConditioner08"]):
    prop(n, -HX + 250, -HY * 0.1 + i * 220, 90, maxsize=200)
prop("SM_CommunicationAntenna02", -HX + 300, HY - 300, 0, maxsize=500)
prop("SM_Flag01", HX - 200, 0, 90, maxsize=400)
prop("SM_Bilboard02", 0, HY - 40, 180, maxsize=1400, shadow=False)
# Rooftop garden: planters along the south parapet (what keeps this place fed).
for i in range(6):
    prop("SM_Plant01" if i % 2 else "SM_Plant02", -HX * 0.6 + i * HX * 0.24, -HY + 160, i * 37, maxsize=180)
# Benches around the camp fire, trash-can fire barrels.
for k in range(4):
    a = k * math.pi / 2 + 0.4
    prop("SM_Bench01" if k % 2 else "SM_Bench02", cx + math.cos(a) * 330, cy + math.sin(a) * 330, math.degrees(a) + 90, maxsize=260)
barrels = [(cx - HX * 0.55, cy + HY * 0.4), (cx + HX * 0.75, cy - HY * 0.1), (cx + HX * 0.3, cy + HY * 0.55)]
for i, (bx, by) in enumerate(barrels):
    prop("SM_TrashCan02", bx, by, i * 50, maxsize=90)
# Supplies: ammo crates by the armory.
for i, n in enumerate(["SM_Ammobox_50", "SM_Ammobox_40mm_Grenade_Closed", "SM_Ammobox_50"]):
    place("/Game/ZS_Imported/Props/AmmoBoxes/%s" % n, -HX * 0.45 + i * 70, -HY * 0.55, ROOF, i * 25, ground=True, label="Supplies_%d" % i)


# ---- Fire and light ------------------------------------------------------------------------------------------------
def light(x, y, z, color, cd, radius, shadows=False, label=None):
    pl = eas.spawn_actor_from_class(unreal.PointLight, unreal.Vector(x, y, z), unreal.Rotator(0, 0, 0))
    c = pl.point_light_component
    c.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
    c.set_editor_property("intensity", cd)
    c.set_editor_property("attenuation_radius", radius)
    c.set_editor_property("light_color", unreal.Color(r=color[0], g=color[1], b=color[2], a=255))
    c.set_editor_property("cast_shadows", shadows)   # shadowed local lights + Nanite VSM hang this GPU
    pl.set_folder_path("Rooftop/Lights")
    if label:
        pl.set_actor_label(label)
    return pl


def fire(x, y, z, system, scale, label):
    ns = unreal.load_asset(system)
    if not ns:
        zs.log("missing " + system)
        return
    a = eas.spawn_actor_from_class(unreal.NiagaraActor, unreal.Vector(x, y, z), unreal.Rotator(0, 0, 0))
    a.niagara_component.set_asset(ns)
    a.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    a.set_actor_label(label)
    a.set_folder_path("Rooftop/Fire")


fire(cx, cy, ROOF, FIRE, 1.0, "CampFire")
light(cx, cy, ROOF + 90, (255, 150, 70), 60, 1400, label="CampFireLight")
for i, (bx, by) in enumerate(barrels):
    fire(bx, by, ROOF + 95, FIRE, 0.35, "BarrelFire_%d" % i)
    light(bx, by, ROOF + 150, (255, 140, 60), 25, 700)

# String lights: warm bulbs on wires zig-zagging over the camp between the corners.
paint = unreal.load_asset(PAINT)
bulb_mat = None
if paint:
    path = "/Game/ZS/Materials/MI_ZS_StringBulb"
    if zs.eal.does_asset_exist(path):
        zs.eal.delete_asset(path)
    bulb_mat = zs.asset_tools.create_asset("MI_ZS_StringBulb", "/Game/ZS/Materials", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(bulb_mat, paint)
    mel.set_material_instance_vector_parameter_value(bulb_mat, "Color", unreal.LinearColor(1.0, 0.62, 0.3, 1))
    mel.set_material_instance_scalar_parameter_value(bulb_mat, "Emissive", 40.0)
    zs.eal.save_loaded_asset(bulb_mat)
sphere = mesh("/Engine/BasicShapes/Sphere")
cube = mesh("/Engine/BasicShapes/Cube")
poles = [(-HX + 80, -HY * 0.5), (HX - 80, -HY * 0.4), (-HX + 80, HY * 0.6), (HX - 80, HY * 0.55), (-HX + 80, -HY * 0.5)]
for (ax, ay) in poles[:4]:
    p = place(cube, ax, ay, ROOF + 150, 0, 1.0, label="LightPole")
    if p:
        p.set_actor_scale3d(unreal.Vector(0.08, 0.08, 3.0))
for (ax, ay), (bx, by) in zip(poles[:-1], poles[1:]):
    n = 9
    for k in range(n + 1):
        t = k / float(n)
        x, y = ax + (bx - ax) * t, ay + (by - ay) * t
        z = ROOF + 300 - 60 * math.sin(t * math.pi)      # sagging wire
        b = place(sphere, x, y, z, 0, 0.06, False, label="Bulb")
        if b and bulb_mat:
            b.static_mesh_component.set_material(0, bulb_mat)
        if k % 3 == 1:
            light(x, y, z - 10, (255, 170, 90), 6, 500)

# ---- Home: tarp canopies over the trader's stall, the medic's corner and the camp fire, bedrolls under them -------
def paint_mi(name, color, rough=0.9):
    path = "/Game/ZS/Materials/" + name
    if zs.eal.does_asset_exist(path):
        zs.eal.delete_asset(path)
    mi = zs.asset_tools.create_asset(name, "/Game/ZS/Materials", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, paint)
    mel.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(*color))
    mel.set_material_instance_scalar_parameter_value(mi, "Roughness", rough)
    zs.eal.save_loaded_asset(mi)
    return mi


if paint:
    tarps = [paint_mi("MI_ZS_TarpOlive", (0.09, 0.1, 0.05, 1)), paint_mi("MI_ZS_TarpBlue", (0.03, 0.08, 0.18, 1)),
             paint_mi("MI_ZS_TarpRust", (0.2, 0.06, 0.03, 1))]
    bedroll = paint_mi("MI_ZS_Bedroll", (0.12, 0.09, 0.06, 1))
    canopies = [(HX * 0.45, -HY * 0.42, 0, 1), (-HX * 0.6, HY * 0.15, 2, 0), (cx - HX * 0.25, cy + HY * 0.45, 1, 2)]
    for i, (tx, ty, mi_i, beds) in enumerate(canopies):
        for px, py in ((-200, -160), (200, -160), (-200, 160), (200, 160)):
            pole = place(cube, tx + px, ty + py, ROOF + 125, 0, 1.0, label="TarpPole", folder="Rooftop/Camp")
            if pole:
                pole.set_actor_scale3d(unreal.Vector(0.06, 0.06, 2.5 + (0.25 if py < 0 else 0)))
        tarp = place(cube, tx, ty, ROOF + 262, 0, 1.0, label="Tarp_%d" % i, folder="Rooftop/Camp")
        if tarp:
            tarp.set_actor_scale3d(unreal.Vector(4.6, 3.8, 0.03))
            tarp.set_actor_rotation(unreal.Rotator(0, 7, 0), False)   # (roll, pitch, yaw): sloped for the rain
            tarp.static_mesh_component.set_material(0, tarps[mi_i])
        for k in range(beds):
            bed = place(cube, tx - 120 + k * 150, ty + 60, ROOF + 6, 10 * k, 1.0, label="Bedroll", folder="Rooftop/Camp")
            if bed:
                bed.set_actor_scale3d(unreal.Vector(0.75, 1.9, 0.1))
                bed.static_mesh_component.set_material(0, bedroll)

# ---- The rest of the city around and below ---------------------------------------------------------------------------
ring = [("SM_Building04", 9000, 25, 0), ("SM_Building06", 8000, 80, 0), ("SM_Building07", 11000, 140, 0),
        ("SM_Building10", 7000, 200, 0), ("SM_Building12", 5500, 245, -2500), ("SM_Building13", 6000, 300, -2000),
        ("SM_Building14", 6500, 335, -1500), ("SM_Building15", 4800, 170, -3000), ("SM_Building03", 10000, 270, 0),
        ("SM_Building05", 12000, 110, 0), ("SM_Building08", 9500, 315, 0), ("SM_Building11", 7500, 50, -1000)]
for name, dist, ang, dz in ring:
    if name in (base_name, "SM_Building11"):
        continue
    m = mesh(KB % name)
    if not m:
        continue
    b = m.get_bounding_box()
    radius = 0.5 * math.hypot(b.max.x - b.min.x, b.max.y - b.min.y)
    dist = max(dist, 7500 + radius)          # clear of the podium block
    a = math.radians(ang)
    place(KB % name, math.cos(a) * dist, math.sin(a) * dist, -b.min.z, ang + 90, 1.0, False, label="City_" + name, folder="City")

# A denser city beyond: two more rings of towers (every kitbash building type, repeated), several burning with smoke
# columns climbing into the evening sky. Shadowless (huge Nanite shadow casters hang this GPU).
burning = []
types = ["SM_Building%02d" % i for i in range(1, 16) if i not in (11, 12)]
k = 0
for ring_r, n in ((22000, 12), (36000, 18), (52000, 22)):
    for j in range(n):
        name = types[k % len(types)]
        k += 1
        m = mesh(KB % name)
        if not m:
            continue
        b = m.get_bounding_box()
        a = math.radians(j * 360.0 / n + (k * 37) % 23)
        r = ring_r + ((k * 7919) % 6000) - 3000
        x, y = math.cos(a) * r, math.sin(a) * r
        place(KB % name, x, y, -b.min.z, (k * 47) % 360, 1.0, False, label="City_" + name, folder="City")
        if k % 5 == 0:
            burning.append((x, y, b.max.z - b.min.z))

fire_sys = unreal.load_asset("/Game/RealisticFireFX/FX/Floor/NS_FireFloor_03")
for i, (x, y, h) in enumerate(burning):
    if fire_sys:
        fa = eas.spawn_actor_from_class(unreal.NiagaraActor, unreal.Vector(x, y, h), unreal.Rotator(0, 0, 0))
        fa.niagara_component.set_asset(fire_sys)
        fa.set_actor_scale3d(unreal.Vector(9, 9, 9))
        fa.set_actor_label("SkylineFire_%d" % i)
        fa.set_folder_path("City/Fires")
    light(x, y, h + 600, (255, 120, 40), 2500, 9000, label="SkylineFireLight_%d" % i)
    sm = eas.spawn_actor_from_class(unreal.ZSSmokeColumn, unreal.Vector(x, y, h + 300), unreal.Rotator(0, 0, 0))
    sm.set_actor_label("SkylineSmoke_%d" % i)
    sm.set_folder_path("City/Fires")
    sm.set_editor_property("size", unreal.Vector2D(900, 6000))
    sm.set_editor_property("rise_speed", 380.0)
    sm.set_editor_property("life", 26.0)
    sm.set_editor_property("rate", 1.2)
    sm.set_editor_property("ember_rate", 0.0)
    sm.set_editor_property("wind", unreal.Vector(260, 120, 0))
# The camp fire's own thin smoke.
cs = eas.spawn_actor_from_class(unreal.ZSSmokeColumn, unreal.Vector(cx, cy, ROOF + 60), unreal.Rotator(0, 0, 0))
cs.set_actor_label("CampFireSmoke")
cs.set_folder_path("Rooftop/Fire")
cs.set_editor_property("size", unreal.Vector2D(40, 380))
cs.set_editor_property("rise_speed", 120.0)
cs.set_editor_property("life", 7.0)
cs.set_editor_property("rate", 2.5)
cs.set_editor_property("opacity", 0.3)
cs.set_editor_property("color", unreal.LinearColor(0.12, 0.11, 0.1, 1))
cs.set_editor_property("ember_rate", 5.0)

# Ash drifting down over the camp (our rain actor, set up as slow grey flakes, no lightning).
ash = eas.spawn_actor_from_class(unreal.ZSRain, unreal.Vector(0, 0, ROOF + 300), unreal.Rotator(0, 0, 0))
ash.set_actor_label("Ash")
ash.set_editor_property("drop_count", 1800)
ash.set_editor_property("box_extent", unreal.Vector(1600, 1600, 900))
ash.set_editor_property("fall_speed", 85.0)
ash.set_editor_property("streak_size", unreal.Vector2D(2.6, 2.6))
ash.set_editor_property("opacity", 0.75)
ash.set_editor_property("color", unreal.LinearColor(0.33, 0.31, 0.29, 1))
ash.set_editor_property("wind", 0.9)
ash.set_editor_property("lightning", False)

# ---- Sky: golden hour -----------------------------------------------------------------------------------------------
for a in eas.get_all_level_actors():
    label = a.get_actor_label()
    if label == "Floor" or label.startswith("SM_Template_Map_Floor"):
        eas.destroy_actor(a)
    elif isinstance(a, unreal.PlayerStart):
        a.set_actor_location(unreal.Vector(sx + 450, sy + 150, ROOF + 100), False, False)
        a.set_actor_rotation(unreal.Rotator(0, 0, math.degrees(math.atan2(cy - sy, cx - sx))), False)
    elif isinstance(a, unreal.DirectionalLight):
        a.set_actor_rotation(unreal.Rotator(-9, 205, 0), False)
        lc = a.light_component
        lc.set_intensity(7.0)
        lc.set_light_color(unreal.LinearColor(1.0, 0.68, 0.42, 1))
        lc.set_editor_property("light_source_angle", 2.0)
    elif isinstance(a, unreal.SkyLight):
        sc = a.light_component
        sc.set_editor_property("real_time_capture", True)
        sc.set_intensity(1.2)
    elif isinstance(a, unreal.ExponentialHeightFog):
        a.set_actor_location(unreal.Vector(0, 0, ROOF - 2500), False, False)
        fc = a.component
        fc.set_editor_property("fog_density", 0.02)
        fc.set_editor_property("fog_height_falloff", 0.25)
        fc.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(0.55, 0.38, 0.25, 1))
        fc.set_editor_property("start_distance", 1500.0)

ppv = eas.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, ROOF + 200), unreal.Rotator(0, 0, 0))
ppv.set_actor_label("PPV_GoldenHour")
ppv.set_editor_property("unbound", True)
s = ppv.get_editor_property("settings")
for prop_name, val in [("color_saturation", unreal.Vector4(1.05, 1.0, 0.95, 1.0)), ("color_gain", unreal.Vector4(1.04, 0.99, 0.92, 1.0)),
                       ("color_contrast", unreal.Vector4(1.08, 1.08, 1.08, 1.0)), ("vignette_intensity", 0.45),
                       ("bloom_intensity", 0.7), ("film_grain_intensity", 0.12), ("auto_exposure_bias", -0.3)]:
    s.set_editor_property("override_" + prop_name, True)
    s.set_editor_property(prop_name, val)
ppv.set_editor_property("settings", s)

# ---- Stations (the hub's menus) next to the people who run them --------------------------------------------------
T = unreal.ZSStationType
stations = [
    (T.ARMORY, -HX * 0.45, -HY * 0.42, 90),
    (T.MARKET, HX * 0.45, -HY * 0.42, 90),
    (T.CHARACTER, HX * 0.55, HY * 0.05, 180),
    (T.MISSIONS, -HX * 0.6, HY * 0.15, 0),
    (T.MAP_TABLE, cx, cy - HY * 0.45, 90),
]
for kind, x, y, yaw in stations:
    st = eas.spawn_actor_from_class(unreal.ZSStation, unreal.Vector(x, y, ROOF + 50), unreal.Rotator(0, 0, yaw))
    st.set_editor_property("type", kind)
    st.set_actor_label("ZSStation_" + str(kind).split(".")[-1])
    st.set_folder_path("Rooftop/Stations")

# ---- The people ------------------------------------------------------------------------------------------------------
L = unreal.Text
people = [
    # body, idle, name, role, lines, x, y, face-towards
    (3, "ArmsCrossed/am_ArmsCrossedB_Idle_02_LookAround", "Marcus", "Quartermaster",
     ["Keep your rifle clean and it'll keep you breathing.", "Bring me back scrap and I'll find you something with more kick.",
      "Checked your mags? Check them again."], -HX * 0.45 - 130, -HY * 0.42 + 170, None),
    (2, "HandHip/am_HandHipL_Idle_02_LookAround", "Rosa", "Trader",
     ["Cash talks up here. Zombies don't haggle, I do.", "Got a crate of rounds in this morning. Not cheap.",
      "You look like you need armor. Everybody does."], HX * 0.45 + 130, -HY * 0.42 + 170, None),
    (5, "HandsClaspFront/am_HandsClaspFront_Idle_04_LookAround", "Sarge", "Ops",
     ["The map's marked. Pick your fight and come back alive.", "Riverside's crawling again. Clear it, salvage what you can.",
      "Nobody goes out alone. Except you, apparently."], cx + 170, cy - HY * 0.45 - 140, None),
    (4, "Tired/am_Stand_Idle_Tired_02", "Doc Avery", "Medic",
     ["Try not to bleed on my clean floor.", "Every bite gets looked at. No exceptions.", "Sleep when you can. I can't."],
     -HX * 0.6 + 160, HY * 0.15 + 160, None),
    (1, "WallLean/am_WallLean_Idle_Loop_02_LookAround", "Jonah", "Lookout",
     ["Quiet tonight. I don't trust quiet.", "Saw a horde moving down Fifth. Big one.", "From up here they look like ants. Hungry ants."],
     HX - 160, -HY * 0.05, (HX + 500, -HY * 0.05)),
    (2, "Cold/am_Stand_Idle_Cold_Loop_01", "Lena", "",
     ["Come warm up. You earned it.", "Glad you made it back."], cx - 200, cy + 180, (cx, cy)),
    (3, "Cold/am_Stand_Idle_Cold_Loop_02", "Teddy", "",
     ["Fire's the best part of the day.", "Tell me it's getting better out there. Lie if you have to."], cx + 220, cy + 140, (cx, cy)),
    (1, "ListenMusic/am_Stand_Idle_ListenMusic_Loop_02_TapLeg", "Milo", "",
     ["Found batteries! Radio still only plays static though.", "You ever hear a song you forgot existed?"], cx + 60, cy - 250, (cx, cy)),
    (4, "Smoke/am_Stand_Idle_Smoke_Hold_Loop", "Vic", "Mechanic",
     ["Generator's holding. Barely.", "If it's got an engine I can fix it. Probably."], sx + 330, sy - 40, None),
]
for i, (body, idle, name, role, lines, x, y, face) in enumerate(people):
    if face is None:
        face = (cx, cy)
    yaw = math.degrees(math.atan2(face[1] - y, face[0] - x))
    npc = eas.spawn_actor_from_class(unreal.ZSSurvivorNPC, unreal.Vector(x, y, ROOF + 92), unreal.Rotator(0, 0, yaw))
    npc.set_actor_label("Survivor_" + name.replace(" ", ""))
    npc.set_folder_path("Rooftop/People")
    npc.set_editor_property("body", body)
    npc.set_editor_property("idle_anim", MC % idle)
    npc.set_editor_property("survivor_name", L(name))
    npc.set_editor_property("job", L(role))
    npc.set_editor_property("lines", [L(t) for t in lines])
    npc.set_editor_property("play_rate", 0.9 + (i % 4) * 0.06)

# Invisible fence just inside the parapet (no walking off the roof).
for x, y, w, d in ((0, y0 - 40, nx * 200 + 200, 60), (0, y0 + ny * 200 + 40, nx * 200 + 200, 60),
                   (x0 - 40, 0, 60, ny * 200 + 200), (x0 + nx * 200 + 40, 0, 60, ny * 200 + 200)):
    wa = place(cube, x, y, ROOF + 300, 0, 1.0, False, label="RoofFence", folder="Rooftop/Bounds")
    if wa:
        wa.set_actor_scale3d(unreal.Vector(w / 100.0, d / 100.0, 6))
        wa.set_actor_hidden_in_game(True)

world.get_world_settings().set_editor_property("default_game_mode", unreal.ZSBunkerGameMode)
les.save_current_level()
zs.log("Saved " + MAP)
