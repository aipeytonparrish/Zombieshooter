"""The New York level's compound: a round-based-survival building behind the start room, from the owner's industrial
modular kit (/Game/IndustrialKit, import_industrial_kit.py). Used by create_nyc_map.py.

Layout (2 m tiles; tile (0, 0) is world (-1000, 7700); +Y runs north, away from the street):

    ty 30 +---------------------------------------+
          |  COURTYARD (open sky)  [B]        [B] |   zone Courtyard   <- 2nd outdoor fight area
    ty 24 +-----------------[LOADING BAY]---------+
          |                                       |
          |  WAREHOUSE (6 m high, pillars)        |   zone Warehouse
          [B]                                    [B]
    ty 14 +---[D]--------------[D]--------[D]-----+
          | LOCKER ROOM   |H|   GENERATOR ROOM    |   zones Lockers / Generator
          [B]            [D]A[D]                [B]
    ty  7 +-----[D]-------+L+--------[D]---------+
          | OFFICES       | L |   STORAGE         |   zone Hall (with the hallway)
          [B]           (open)(open)            [B]
    ty  0 +---[B]------+--[D]--+------[B]---------+
                       | START |                       always open
    ty -8              +-[D]---+   <- STREET DOOR -> the street (zone Street, 1st outdoor fight area)

[D] buy doors (run cash), [B] boarded windows (AZSBarricade): zombies spawn OUTSIDE the walls, walk up, tear the
boards off and climb in. Every window has a spawn point outside it in that room's zone.
Everything static is drawn by one AZSKitInstances actor (one instanced mesh per kit piece).
"""
import math

import unreal

import zs_import_common as zs

T = 200
X0, Y0 = -1000, 7700
FLOOR = -100
KIT = "/Game/IndustrialKit/%s/StaticMeshes/%s_LOD0"
BOARD_MAT = "/Game/IndustrialKit/SM_Ceiling_Beams_2x2/Materials/M_Rust_Heavy"
KB_PROP = "/Game/MS_NewYorkCityKitbash_Assets/Meshes-Assets/%s"

START = (5, -8, 13, 0)          # tx0, ty0, tx1, ty1
PLAYER_START = (800, 6900, 0, 90)

# Rooms: name, (tx0, ty0, tx1, ty1), height, ceiling, floor pick
ROOMS = [
    ("Start", START, 300, True, "start"),
    ("Offices", (0, 0, 9, 7), 300, True, "tile"),
    ("Hall", (9, 0, 11, 14), 300, True, "hall"),
    ("Storage", (11, 0, 20, 7), 300, True, "concrete"),
    ("Lockers", (0, 7, 9, 14), 300, True, "tile"),
    ("Generator", (11, 7, 20, 14), 300, True, "tread"),
    ("Warehouse", (0, 14, 20, 24), 600, True, "warehouse"),
    ("Courtyard", (0, 24, 20, 30), 300, False, "yard"),
]

# Wall lines. axis "X": a line along X at y = ty; "Y": along Y at x = tx. ext: which side is outside ("+"/"-", None =
# interior, both faces finished). openings: tile -> "win:<zone>" | "open" | "door:<label>:<zone>:<cost>:<side>" |
# "door2..." (a 2-tile door starting at this tile); side = which side the door opens INTO ("+"/"-").
WALLS = [
    # Start room
    dict(axis="X", line=-8, t=(5, 13), ext="-", h=300, open={6: "glass", 7: "glass", 11: "door:STREET DOOR:Street:350:-"}),
    dict(axis="Y", line=5, t=(-8, 0), ext="-", h=300, open={-5: "win:"}),
    dict(axis="Y", line=13, t=(-8, 0), ext="+", h=300, open={-6: "win:", -3: "win:"}),
    dict(axis="X", line=0, t=(5, 13), ext=None, h=300, open={9: "door:HALLWAY:Hall:750:+"}),
    # West wing / east wing outer walls
    dict(axis="X", line=0, t=(0, 5), ext="-", h=300, open={2: "win:Hall"}),
    dict(axis="X", line=0, t=(13, 20), ext="-", h=300, open={16: "win:Hall"}),
    dict(axis="Y", line=0, t=(0, 14), ext="-", h=300, open={3: "win:Hall", 9: "win:Lockers", 12: "win:Lockers"}),
    dict(axis="Y", line=20, t=(0, 14), ext="+", h=300, open={3: "win:Hall", 9: "win:Generator", 12: "win:Generator"}),
    # Interior
    dict(axis="X", line=7, t=(0, 9), ext=None, h=300, open={4: "door:LOCKER ROOM:Lockers:1000:+"}),
    dict(axis="X", line=7, t=(11, 20), ext=None, h=300, open={15: "door:GENERATOR ROOM:Generator:1000:+"}),
    dict(axis="Y", line=9, t=(0, 14), ext=None, h=300, open={4: "open", 11: "door:LOCKER ROOM:Lockers:1000:-"}),
    dict(axis="Y", line=11, t=(0, 14), ext=None, h=300, open={3: "open", 10: "door:GENERATOR ROOM:Generator:1000:+"}),
    dict(axis="X", line=14, t=(0, 20), ext=None, h=600, open={4: "door:WAREHOUSE:Warehouse:1250:+", 9: "door:WAREHOUSE:Warehouse:1250:+",
                                                              16: "door:WAREHOUSE:Warehouse:1250:+"}),
    # Warehouse
    dict(axis="Y", line=0, t=(14, 24), ext="-", h=600, open={17: "win:Warehouse", 21: "win:Warehouse"}),
    dict(axis="Y", line=20, t=(14, 24), ext="+", h=600, open={17: "win:Warehouse", 21: "win:Warehouse"}),
    dict(axis="X", line=24, t=(0, 20), ext=None, h=600, open={9: "door2:LOADING BAY:Courtyard:1500:+", 10: "skip",
                                                              3: "glass", 6: "glass", 14: "glass", 17: "glass"}),
    # Courtyard perimeter (open sky)
    dict(axis="Y", line=0, t=(24, 30), ext="-", h=300, open={27: "win:Courtyard"}, roofless=True),
    dict(axis="Y", line=20, t=(24, 30), ext="+", h=300, open={27: "win:Courtyard"}, roofless=True),
    dict(axis="X", line=30, t=(0, 20), ext="+", h=300, open={5: "win:Courtyard", 15: "win:Courtyard"}, roofless=True),
]

INNER_LOOKS = ["Concrete", "Pipes", "Concrete", "Tile", "Concrete", "Corrugated", "Concrete", "Pipes", "Tile"]
ROOM_LOOKS = {"Offices": ["Tile", "Concrete", "Tile"], "Lockers": ["Tile", "Tile", "Concrete"], "Hall": ["Concrete", "Pipes"],
              "Storage": ["Corrugated", "Concrete"], "Generator": ["Pipes", "Corrugated", "Pipes"],
              "Warehouse": ["Corrugated", "Concrete", "Corrugated"], "Courtyard": ["Concrete", "Concrete", "Corrugated"]}


def wx(tx):
    return X0 + T * tx


def wy(ty):
    return Y0 + T * ty


def room_at(px, py):
    for name, (a, b, c, d), *_ in ROOMS:
        if wx(a) <= px <= wx(c) and wy(b) <= py <= wy(d):
            return name
    return None


class Builder:
    def __init__(self, eas):
        self.eas = eas
        self.inst = {}            # mesh name -> [Transform]
        self.meshes = {}
        self.barricades = []      # (actor, zone, outward vector)
        self.lights = 0

    # ---- instancing
    def put(self, name, x, y, z, yaw, sz=1.0, sx=1.0):
        self.inst.setdefault(name, []).append(
            unreal.Transform(unreal.Vector(x, y, z), unreal.Rotator(0, 0, yaw), unreal.Vector(sx, 1.0, sz)))

    def mesh(self, name):
        if name not in self.meshes:
            self.meshes[name] = unreal.load_asset(KIT % (name, name))
            if self.meshes[name] is None:
                raise RuntimeError("kit mesh missing: " + KIT % (name, name))
        return self.meshes[name]

    def finish(self):
        actor = self.eas.spawn_actor_from_class(unreal.ZSKitInstances, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
        actor.set_actor_label("ZSCompound_Kit")
        actor.set_folder_path("Compound")
        batches = []
        total = 0
        for name, transforms in self.inst.items():
            b = unreal.ZSKitBatch()
            b.set_editor_property("mesh", self.mesh(name))
            b.set_editor_property("instances", transforms)
            b.set_editor_property("collide", not name.startswith("SM_Light"))
            b.set_editor_property("cast_shadow", not name.startswith(("SM_Floor", "SM_Light")))   # fewer Nanite shadow casters
            batches.append(b)
            total += len(transforms)
        actor.set_editor_property("batches", batches)
        zs.log("Compound kit: %d pieces in %d instanced meshes" % (total, len(batches)))

    # ---- walls
    def faces(self, axis, line_w, along, z, look_pos, look_neg, sz=1.0):
        """Both faces of one 2 m wall tile: look_pos faces the + side, look_neg the - side (None = skip)."""
        if axis == "X":
            if look_pos: self.put("SM_Wall_%s_2x3" % look_pos, along, line_w + 12, z, 0, sz)
            if look_neg: self.put("SM_Wall_%s_2x3" % look_neg, along, line_w - 12, z, 180, sz)
        else:
            if look_pos: self.put("SM_Wall_%s_2x3" % look_pos, line_w + 12, along, z, -90, sz)
            if look_neg: self.put("SM_Wall_%s_2x3" % look_neg, line_w - 12, along, z, 90, sz)

    def build_wall(self, w):
        axis, line, (t0, t1), ext, h = w["axis"], w["line"], w["t"], w["ext"], w["h"]
        line_w = wy(line) if axis == "X" else wx(line)
        opens = w.get("open", {})
        roofless = w.get("roofless", False)
        for t in range(t0, t1):
            along = (wx(t) + 100) if axis == "X" else (wy(t) + 100)
            # Which room is on each side (for its look).
            if axis == "X":
                pos_room, neg_room = room_at(along, line_w + 50), room_at(along, line_w - 50)
            else:
                pos_room, neg_room = room_at(line_w + 50, along), room_at(line_w - 50, along)

            def look(room, k):
                if room is None:
                    return "Concrete"
                looks = ROOM_LOOKS.get(room, INNER_LOOKS)
                return looks[(k + t * 3) % len(looks)]

            lp, ln = look(pos_room, 0), look(neg_room, 1)
            if ext == "+":
                lp = "Concrete"
            elif ext == "-":
                ln = "Concrete"
            kind = opens.get(t, "")
            z = FLOOR
            if kind == "skip":
                continue
            if kind.startswith("win"):
                self.faces(axis, line_w, along, z, "Concrete", "Concrete", 0.3)            # sill 0.9 m
                self.faces(axis, line_w, along, z + 240, "Concrete", "Concrete", 0.2)      # lintel
                self.add_barricade(axis, line_w, along, ext, kind.split(":", 1)[1] or None)
            elif kind in ("open",) or kind.startswith("door"):
                width = 2 if kind.startswith("door2") else 1
                top = 260 if width == 2 else 220
                for k in range(width):
                    a = along + k * 200
                    self.faces(axis, line_w, a, z + top, lp if width == 1 else "Concrete", ln if width == 1 else "Concrete", (300 - top) / 300.0)
                    if k > 0 and h > 300:
                        self.faces(axis, line_w, a, z + 300, lp, ln)   # the next tile is "skip": fill its upper wall here
                if kind.startswith("door"):
                    _, label, zone, cost, side = kind.split(":")
                    centre = along + (width - 1) * 100
                    self.add_door(axis, line_w, centre, label, zone, int(cost), side, width * 200, top)
            elif kind == "glass":
                self.faces(axis, line_w, along, z, "Window", "Window")
            else:
                self.faces(axis, line_w, along, z, lp, ln)
            if h > 300:
                self.faces(axis, line_w, along, z + 300, lp, ln)
            if ext and not roofless:
                # Parapet over the roof line on the outside.
                pz = z + h
                if axis == "X":
                    self.put("SM_Wall_Concrete_2x3", along, line_w + (12 if ext == "+" else -12), pz, 0 if ext == "+" else 180, 0.3)
                else:
                    self.put("SM_Wall_Concrete_2x3", line_w + (12 if ext == "+" else -12), along, pz, -90 if ext == "+" else 90, 0.3)

    # ---- actors
    def add_barricade(self, axis, line_w, along, ext, zone):
        if axis == "X":
            loc = unreal.Vector(along, line_w, FLOOR)
            yaw = 90 if ext == "+" else -90
        else:
            loc = unreal.Vector(line_w, along, FLOOR)
            yaw = 0 if ext == "+" else 180
        b = self.eas.spawn_actor_from_class(unreal.ZSBarricade, loc, unreal.Rotator(0, 0, yaw))
        b.set_actor_label("ZSBarricade_%s_%d" % (zone or "Start", len(self.barricades)))
        b.set_folder_path("Compound/Barricades")
        b.set_editor_property("opening_size", unreal.Vector2D(180, 150))
        out = unreal.Vector(math.cos(math.radians(yaw)), math.sin(math.radians(yaw)), 0)
        # Spawn point outside (narrower gap on the long west / east sides, where the backdrop buildings stand).
        dist = 230 if axis == "Y" else 330
        sp = self.eas.spawn_actor_from_class(unreal.ZSSpawnPoint, unreal.Vector(loc.x + out.x * dist, loc.y + out.y * dist, FLOOR + 95), unreal.Rotator(0, 0, yaw + 180))
        sp.set_actor_label("ZSSpawnPoint_Window_%d" % len(self.barricades))
        sp.set_folder_path("Compound/Spawns")
        if zone:
            sp.set_editor_property("zone", zone)
        sp.set_editor_property("min_player_distance", 500.0)
        sp.set_editor_property("barricade", b)
        self.barricades.append((b, zone, out))

    def add_door(self, axis, line_w, centre, label, zone, cost, side, width, height):
        if axis == "X":
            loc = unreal.Vector(centre, line_w, FLOOR)
            yaw = 90 if side == "+" else -90
        else:
            loc = unreal.Vector(line_w, centre, FLOOR)
            yaw = 0 if side == "+" else 180
        d = self.eas.spawn_actor_from_class(unreal.ZSBuyDoor, loc, unreal.Rotator(0, 0, yaw))
        d.set_actor_label("ZSBuyDoor_%s_%d" % (label.replace(" ", ""), int(centre)))
        d.set_folder_path("Compound/Doors")
        d.set_editor_property("cost", cost)
        d.set_editor_property("zone", zone)
        d.set_editor_property("label", label)
        d.set_editor_property("opening_size", unreal.Vector2D(width, height))
        mat = unreal.load_asset(BOARD_MAT)
        if mat:
            d.set_editor_property("board_material", mat)

    def light(self, x, y, z, color, cd, radius, fixture=None, shadows=False):
        if fixture:
            self.put(fixture, x, y, z, 0)
        pl = self.eas.spawn_actor_from_class(unreal.PointLight, unreal.Vector(x, y, z - 30), unreal.Rotator(0, 0, 0))
        c = pl.point_light_component
        c.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
        c.set_editor_property("intensity", cd)
        c.set_editor_property("attenuation_radius", radius)
        c.set_editor_property("light_color", unreal.Color(r=color[0], g=color[1], b=color[2], a=255))
        c.set_editor_property("source_radius", 6.0)
        c.set_editor_property("cast_shadows", False)   # shadowed local lights + Nanite VSM hung the GPU on the first frame
        pl.set_folder_path("Compound/Lights")
        self.lights += 1


WARM = (255, 196, 140)
COLD = (190, 215, 255)
RED = (255, 40, 30)


def floor_pick(kind, ix, iy):
    h = (ix * 7 + iy * 13) % 11
    if kind == "tile":
        return "SM_Floor_Tile_2x2" if h else "SM_Floor_Drain_2x2"
    if kind == "hall":
        return "SM_Floor_Concrete_2x2" if h % 4 else "SM_Floor_TreadPlate_2x2"
    if kind == "tread":
        return "SM_Floor_TreadPlate_2x2" if h % 3 else "SM_Floor_Grate_2x2"
    if kind == "warehouse":
        return "SM_Floor_Concrete_2x2" if h % 5 else ("SM_Floor_Drain_2x2" if h else "SM_Floor_TreadPlate_2x2")
    if kind == "yard":
        return "SM_Floor_Concrete_2x2" if h % 6 else "SM_Floor_Drain_2x2"
    if kind == "start":
        return "SM_Floor_Concrete_2x2" if (ix + iy * 3) % 7 else "SM_Floor_Drain_2x2"
    return "SM_Floor_Concrete_2x2" if h % 7 else "SM_Floor_Drain_2x2"


def build(eas):
    """Builds the compound; returns shelter boxes (no rain inside) for AZSRain."""
    if not zs.eal.does_asset_exist(KIT % ("SM_Wall_Concrete_2x3", "SM_Wall_Concrete_2x3")):
        zs.log("WARNING: /Game/IndustrialKit missing - run import_industrial_kit.py (no compound)")
        return []
    b = Builder(eas)
    ceilings = ["SM_Ceiling_Tiles_2x2", "SM_Ceiling_Beams_2x2", "SM_Ceiling_Pipes_2x2", "SM_Ceiling_Vent_2x2"]
    for name, (a, c0, c, d), h, has_ceiling, fkind in ROOMS:
        for ix in range(a, c):
            for iy in range(c0, d):
                x, y = wx(ix) + 100, wy(iy) + 100
                b.put(floor_pick(fkind, ix, iy), x, y, FLOOR - 3, 0)
                if has_ceiling:
                    b.put(ceilings[(ix * 7 + iy * 3) % 4], x, y, FLOOR + h, 0)
    # Strips outside the west / east / north walls where window zombies spawn (north of the street art, y > 7950).
    for iy in range(2, 32):
        for ix in (-2, -1, 20, 21):
            b.put("SM_Floor_Concrete_2x2", wx(ix) + 100, wy(iy) + 100, FLOOR - 3, 0)
    for ix in range(0, 20):
        for iy in (30, 31):
            b.put("SM_Floor_Concrete_2x2", wx(ix) + 100, wy(iy) + 100, FLOOR - 3, 0)

    for w in WALLS:
        b.build_wall(w)

    # Corner pillars (cover the gaps where two double walls meet) and structural pillars.
    for (tx, ty) in [(0, 0), (20, 0), (0, 14), (20, 14), (0, 24), (20, 24), (0, 30), (20, 30), (5, -8), (13, -8), (5, 0), (13, 0)]:
        b.put("SM_Pillar_Concrete_3m", wx(tx), wy(ty), FLOOR, 0)
        if 14 <= ty <= 24:
            b.put("SM_Pillar_Concrete_3m", wx(tx), wy(ty), FLOOR + 300, 0)
    for (tx, ty) in [(7.5, -5.5), (10.5, -5.5)]:
        b.put("SM_Pillar_Concrete_3m", wx(tx), wy(ty), FLOOR, 0)
    for tx in (5, 10, 15):
        for ty in (17.5, 21):
            b.put("SM_Pillar_Concrete_3m", wx(tx), wy(ty), FLOOR, 0)
            b.put("SM_Pillar_Concrete_3m", wx(tx), wy(ty), FLOOR + 300, 0)

    # Lights.
    z3, z6 = FLOOR + 300, FLOOR + 600
    for (tx, ty) in [(7, -6), (11, -6), (7, -2), (11, -2)]:
        b.light(wx(tx), wy(ty), z3, WARM, 40, 900, "SM_Light_Caged_Bulb", shadows=True)
    b.light(wx(11) + 100, wy(-8) + 40, FLOOR + 280, RED, 15, 500, "SM_Light_Emergency_Red")
    for ty in (2, 5, 8, 11, 13):
        b.light(wx(10), wy(ty), z3, COLD, 22, 700, "SM_Light_Fluorescent_Broken")
    for (tx, ty) in [(2, 2), (6, 2), (2, 5), (6, 5)]:
        b.light(wx(tx), wy(ty), z3, WARM, 30, 800, "SM_Light_Caged_Bulb")
    b.light(wx(4.5), wy(10.5), z3, COLD, 25, 900, "SM_Light_Fluorescent_Broken")
    b.light(wx(2), wy(12.5), z3, WARM, 20, 700, "SM_Light_Caged_Bulb")
    for (tx, ty) in [(13.5, 2), (17.5, 2), (13.5, 5.5), (17.5, 5.5)]:
        b.light(wx(tx), wy(ty), z3, WARM, 30, 800, "SM_Light_Caged_Bulb")
    b.light(wx(13), wy(9), z3, RED, 30, 900, "SM_Light_Emergency_Red", shadows=True)
    b.light(wx(18), wy(12.5), z3, RED, 30, 900, "SM_Light_Emergency_Red")
    b.light(wx(15.5), wy(10.5), z3, COLD, 20, 800, "SM_Light_Fluorescent_Broken")
    for tx in (3, 7.5, 12.5, 17):
        for ty in (16, 19.5, 22.5):
            b.light(wx(tx), wy(ty), z6, COLD if (tx + ty) % 2 else WARM, 55, 1300, "SM_Light_Fluorescent_Broken", shadows=(tx == 7.5 and ty == 19.5))
    for (tx, ty) in [(0.3, 27), (19.7, 27), (5, 29.7), (15, 29.7), (10, 24.3)]:
        b.light(wx(tx), wy(ty), FLOOR + 270, WARM, 25, 900, "SM_Light_Caged_Bulb")
    b.light(wx(10), wy(24) - 60, FLOOR + 280, RED, 18, 600, "SM_Light_Emergency_Red")

    b.finish()

    # Courtyard: ground spawns in the far corners (zombies climbing out of the rubble) and some street furniture.
    for i, (tx, ty) in enumerate([(1.2, 29), (18.8, 29), (10, 29.2)]):
        sp = eas.spawn_actor_from_class(unreal.ZSSpawnPoint, unreal.Vector(wx(tx), wy(ty), FLOOR + 95), unreal.Rotator(0, 0, -90))
        sp.set_actor_label("ZSSpawnPoint_Courtyard_%d" % i)
        sp.set_folder_path("Compound/Spawns")
        sp.set_editor_property("zone", "Courtyard")
        sp.set_editor_property("min_player_distance", 900.0)
    props = [("SM_TrashCan01", 2, 25.5, 0), ("SM_TrashCan03", 2.6, 25.4, 30), ("SM_ElectricityBox", 19.6, 25.5, -90),
             ("SM_AirConditioner03", 7, 29.5, 180), ("SM_AirConditioner05", 13, 29.5, 180), ("SM_Bench01", 5, 25.4, 0),
             ("SM_Trash05", 16, 26, 0), ("SM_Trash09", 4, 28, 40), ("SM_Trash12", 12, 27.5, 0), ("SM_Plant01", 18.5, 28.5, 0)]
    for name, tx, ty, yaw in props:
        m = unreal.load_asset(KB_PROP % name)
        if not m:
            continue
        a = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(wx(tx), wy(ty), FLOOR), unreal.Rotator(0, 0, yaw))
        a.static_mesh_component.set_static_mesh(m)
        a.set_actor_label("Prop_" + name)
        a.set_folder_path("Compound/Props")

    # Ammo crates.
    for i, (tx, ty, yaw) in enumerate([(1, 6.3, 90), (10, 22.8, 0), (18.5, 26.5, -90), (12.5, -7.3, 90)]):
        c = eas.spawn_actor_from_class(unreal.ZSAmmoCrate, unreal.Vector(wx(tx), wy(ty), FLOOR), unreal.Rotator(0, 0, yaw))
        c.set_actor_label("ZSAmmoCrate_Compound_%d" % i)
        c.set_folder_path("Compound")

    # Guns on the walls (run cash; buying one you carry buys its ammo) and the mystery box in the warehouse.
    wall_guns = [("PST_P9", 250, wx(6.5), wy(0) - 15, -90), ("AR_M4", 1000, wx(9) + 15, wy(8), 0),
                 ("AR_RD704", 1500, wx(20) - 15, wy(5.5), 180), ("AR_M4", 1000, wx(0) + 15, wy(19), 0),
                 ("AR_RD704", 1500, wx(10), wy(30) - 15, -90)]
    for i, (item, cost, x, y, yaw) in enumerate(wall_guns):
        wb = eas.spawn_actor_from_class(unreal.ZSWallBuy, unreal.Vector(x, y, FLOOR + 150), unreal.Rotator(0, 0, yaw))
        wb.set_actor_label("ZSWallBuy_%s_%d" % (item, i))
        wb.set_folder_path("Compound/WallBuys")
        wb.set_editor_property("item_id", item)
        wb.set_editor_property("cost", cost)
    box = eas.spawn_actor_from_class(unreal.ZSMysteryBox, unreal.Vector(wx(12.5), wy(19.2), FLOOR + 28), unreal.Rotator(0, 0, 90))
    box.set_actor_label("ZSMysteryBox")
    box.set_folder_path("Compound")

    zs.log("Compound built: %d barricades, %d lights" % (len(b.barricades), b.lights))
    sx0, sy0, sx1, sy1 = wx(START[0]), wy(START[1]), wx(START[2]), wy(START[3])
    return [unreal.Box(unreal.Vector(sx0 - 60, sy0 - 60, FLOOR - 60), unreal.Vector(sx1 + 60, sy1 + 60, FLOOR + 420)),
            unreal.Box(unreal.Vector(wx(0) - 60, wy(0) - 60, FLOOR - 60), unreal.Vector(wx(20) + 60, wy(24) + 60, FLOOR + 720))]
