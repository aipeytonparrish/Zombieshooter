"""Outdoor dressing for the New York level (used by create_nyc_map.py): street furniture from the Megascans NYC kitbash
(street lights, traffic lights, signs, hydrants, trash, benches, trees, guardrails), burning barrels and fires in the
rubble (RealisticFireFX + warm shadowless lights), and old blood on the street and through the compound.

The street runs along Y between the river (x < -1200) and the east facades (x ~3250); sidewalk lines at x ~ -1000
and x ~ 2900. Lights are shadowless on purpose (shadowed local lights + Nanite virtual shadow maps hang this GPU).
"""
import math
import random

import unreal

import zs_import_common as zs

KBP = "/Game/MS_NewYorkCityKitbash_Assets/Meshes-Assets/%s"
FIRE = "/Game/RealisticFireFX/FX/Campfire/NS_CampFire_01"
FIRE_FLOOR = "/Game/RealisticFireFX/FX/Campfire/NS_CampFire_02"   # the floor fires read as lava pools
SPLAT = "/Game/ZS/Materials/MI_ZS_BloodSplat"
POOL = "/Game/ZS/Materials/MI_ZS_BloodPool"
STREET_Z = -110


class Dresser:
    def __init__(self, eas):
        self.eas = eas
        self.n = 0
        self.rng = random.Random(77)

    def ground_z(self, x, y, fallback):
        """Floor height under (x, y) from a downward trace (art + compound), so props sit on whatever is there."""
        try:
            hit = unreal.SystemLibrary.line_trace_single(unreal.EditorLevelLibrary.get_editor_world(), unreal.Vector(x, y, 1500),
                                                         unreal.Vector(x, y, -1500), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [],
                                                         unreal.DrawDebugTrace.NONE, True)
            if hit:
                t = hit.to_tuple()
                if t[0] and -600 < t[4].z < 600:   # blocking hit, sane height (not a roof)
                    return t[4].z
        except Exception as e:  # noqa: BLE001
            zs.log("trace failed: %s" % e)
        return fallback

    def prop(self, name, x, y, yaw=0.0, maxsize=None, z=None, shadow=True, folder="Dressing/Street"):
        m = unreal.load_asset(KBP % name)
        if not m:
            return None
        b = m.get_bounding_box()
        scale = 1.0
        if maxsize:
            big = max(b.max.x - b.min.x, b.max.y - b.min.y, b.max.z - b.min.z)
            if big > maxsize:
                scale = maxsize / big
        gz = self.ground_z(x, y, STREET_Z) if z is None else z
        a = self.eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, gz - b.min.z * scale), unreal.Rotator(0, 0, yaw))
        a.static_mesh_component.set_static_mesh(m)
        a.set_actor_scale3d(unreal.Vector(scale, scale, scale))
        a.static_mesh_component.set_editor_property("cast_shadow", shadow)
        a.set_actor_label("Dress_%s_%d" % (name, self.n))
        a.set_folder_path(folder)
        self.n += 1
        return a

    def light(self, x, y, z, color, cd, radius, folder="Dressing/Lights"):
        pl = self.eas.spawn_actor_from_class(unreal.PointLight, unreal.Vector(x, y, z), unreal.Rotator(0, 0, 0))
        c = pl.point_light_component
        c.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
        c.set_editor_property("intensity", cd)
        c.set_editor_property("attenuation_radius", radius)
        c.set_editor_property("light_color", unreal.Color(r=color[0], g=color[1], b=color[2], a=255))
        c.set_editor_property("cast_shadows", False)
        pl.set_folder_path(folder)

    def fire(self, x, y, z, system, scale, folder="Dressing/Fires"):
        ns = unreal.load_asset(system)
        if not ns:
            return
        a = self.eas.spawn_actor_from_class(unreal.NiagaraActor, unreal.Vector(x, y, z), unreal.Rotator(0, 0, 0))
        a.niagara_component.set_asset(ns)
        a.set_actor_scale3d(unreal.Vector(scale, scale, scale))
        a.set_folder_path(folder)

    def smoke(self, x, y, z, size, rise, life, rate, opacity, embers, folder="Dressing/Fires"):
        s = self.eas.spawn_actor_from_class(unreal.ZSSmokeColumn, unreal.Vector(x, y, z), unreal.Rotator(0, 0, 0))
        s.set_folder_path(folder)
        s.set_editor_property("size", unreal.Vector2D(*size))
        s.set_editor_property("rise_speed", rise)
        s.set_editor_property("life", life)
        s.set_editor_property("rate", rate)
        s.set_editor_property("opacity", opacity)
        s.set_editor_property("ember_rate", embers)
        return s

    def barrel_fire(self, x, y, folder="Dressing/Fires"):
        can = self.prop("SM_TrashCan02", x, y, self.rng.uniform(0, 360), maxsize=95, folder=folder)
        if can:
            top = can.get_actor_location().z + 85
            self.fire(x, y, top, FIRE, 0.32, folder)
            self.light(x, y, top + 60, (255, 140, 60), 30, 900)
            self.smoke(x, y, top + 40, (30, 420), 140, 8.0, 2.0, 0.35, 6.0)

    def decal(self, x, y, z, path, size, folder="Dressing/Blood"):
        mat = unreal.load_asset(path)
        if not mat:
            return
        d = self.eas.spawn_actor_from_class(unreal.DecalActor, unreal.Vector(x, y, z), unreal.Rotator(0, -90, self.rng.uniform(0, 360)))
        d.decal.set_decal_material(mat)
        d.decal.set_editor_property("decal_size", unreal.Vector(40, size, size))
        d.set_folder_path(folder)


def dress(eas, compound_rooms):
    """compound_rooms: [(xmin, ymin, xmax, ymax), ...] floor rectangles (z -100) for blood inside the compound."""
    d = Dresser(eas)
    rng = d.rng
    if not unreal.load_asset(KBP % "SM_StreetLight01"):
        zs.log("WARNING: kitbash props missing - no street dressing")
        return

    # Street lights down both sidewalks; most dead, a few still burning (cold sodium-ish, no shadows).
    for i, y in enumerate(range(-5400, 7000, 1500)):
        for x, yaw in ((2920, 180), (-1000, 0)):
            lamp = d.prop("SM_StreetLight0%d" % (1 + (i % 3)), x, y + (0 if x > 0 else 700), yaw, maxsize=900)
            if lamp and (i + (x > 0)) % 3 == 0:
                d.light(x + (-150 if x > 0 else 150), y + (0 if x > 0 else 700), lamp.get_actor_location().z + 650, (255, 200, 140), 35, 1600)
    # Traffic lights and signs at the "crossings".
    for y in (-3000, 1500, 5200):
        d.prop("SM_TrafficLight01", 2850, y + 250, 180, maxsize=650)
        d.prop("SM_TrafficLight02", -950, y - 250, 0, maxsize=650)
        d.prop("SM_StreetSign0%d" % rng.randint(1, 9), 2880, y - 300, 180, maxsize=320)
    # Sidewalk clutter: hydrants, trash cans, trash bags, benches, a bus stop, mailbox, electricity boxes.
    clutter = ["SM_Hydrant01", "SM_TrashCan01", "SM_TrashCan03", "SM_Trash03", "SM_Trash06", "SM_Trash09", "SM_Trash11",
               "SM_Trash14", "SM_Bench01", "SM_MailBox", "SM_ElectricityBox", "SM_Trash17", "SM_TrashCan05"]
    for i, y in enumerate(range(-5600, 7200, 520)):
        x = 2780 + rng.uniform(-60, 60) if i % 2 else -870 + rng.uniform(-60, 60)
        name = clutter[i % len(clutter)]
        d.prop(name, x, y + rng.uniform(-120, 120), rng.uniform(0, 360) if name.startswith("SM_Trash") else (180 if x > 0 else 0), maxsize=260)
    d.prop("SM_BusStop01", 2800, -1200, 180, maxsize=700)
    # Guardrail along the river edge, broken in places.
    for y in range(-5800, 6800, 900):
        if rng.random() < 0.75:
            d.prop("SM_StreetGuardrail01", -1150, y, 90 + rng.uniform(-6, 6), maxsize=880)
    # Dead street trees.
    for y in (-4600, -800, 2900, 6200):
        d.prop("SM_Tree0%d" % rng.randint(1, 8), -900, y, rng.uniform(0, 360), maxsize=900)

    # Burning barrels where survivors made a stand, and fires smouldering in the rubble.
    for x, y in ((2400, -4300), (-400, -2200), (1900, 600), (-500, 3300), (2300, 4600), (600, 5800)):
        d.barrel_fire(x, y)
    for x, y in ((1200, -5200), (-700, 1000), (2600, 2400), (400, 4000)):
        z = d.ground_z(x, y, STREET_Z)
        d.fire(x, y, z, FIRE_FLOOR, 0.7)
        d.light(x, y, z + 120, (255, 120, 50), 40, 1100)
        d.smoke(x, y, z + 80, (60, 900), 200, 12.0, 2.0, 0.45, 8.0)
    # The city burning beyond the street: big smoke columns over the rooftops.
    for x, y in ((7000, -3000), (9000, 4000), (-6000, 2000), (5000, 12000), (-4000, -7000)):
        d.smoke(x, y, 3500, (800, 5000), 320, 24.0, 1.0, 0.5, 0.0)

    # Old blood: splats and pools on the street and through the compound.
    for i in range(40):
        x, y = rng.uniform(-1000, 2800), rng.uniform(-5800, 7500)
        z = d.ground_z(x, y, STREET_Z)
        d.decal(x, y, z, POOL if i % 3 == 0 else SPLAT, rng.uniform(60, 180))
    for (xa, ya, xb, yb) in compound_rooms:
        for i in range(max(2, int((xb - xa) * (yb - ya) / 900000))):
            d.decal(rng.uniform(xa + 100, xb - 100), rng.uniform(ya + 100, yb - 100), -100, POOL if i % 2 else SPLAT, rng.uniform(50, 150), "Dressing/Blood")
    zs.log("Street dressing: %d props" % d.n)
