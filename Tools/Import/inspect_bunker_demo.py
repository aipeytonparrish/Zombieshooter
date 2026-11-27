"""Log the Bunker pack demo map's player start, bounds and floor/wall distances around it (for placing stations)."""
import math
import unreal

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les.load_level("/Game/Bunker/Maps/Demo")
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
actors = eas.get_all_level_actors()
unreal.log_warning("[ZSInspect] actors=%d" % len(actors))
classes = {}
for a in actors:
    classes[a.get_class().get_name()] = classes.get(a.get_class().get_name(), 0) + 1
unreal.log_warning("[ZSInspect] classes=%s" % sorted(classes.items(), key=lambda x: -x[1])[:15])

starts = [a for a in actors if isinstance(a, unreal.PlayerStart)]
for s in starts:
    unreal.log_warning("[ZSInspect] start %s %s" % (s.get_actor_location(), s.get_actor_rotation()))
lo = unreal.Vector(1e9, 1e9, 1e9); hi = unreal.Vector(-1e9, -1e9, -1e9)
for a in actors:
    if isinstance(a, unreal.StaticMeshActor):
        p = a.get_actor_location()
        lo = unreal.Vector(min(lo.x, p.x), min(lo.y, p.y), min(lo.z, p.z)); hi = unreal.Vector(max(hi.x, p.x), max(hi.y, p.y), max(hi.z, p.z))
unreal.log_warning("[ZSInspect] mesh bounds %s .. %s" % (lo, hi))
ws = world.get_world_settings()
unreal.log_warning("[ZSInspect] gm=%s" % ws.get_editor_property("default_game_mode"))


def trace(a, b):
    hit = unreal.SystemLibrary.line_trace_single(world, a, b, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return None
    t = hit.to_tuple()
    return t[4] if t[0] else None  # location


if starts:
    s = starts[0].get_actor_location()
    yaw = starts[0].get_actor_rotation().yaw
    fwd = unreal.Vector(math.cos(math.radians(yaw)), math.sin(math.radians(yaw)), 0)
    right = unreal.Vector(-fwd.y, fwd.x, 0)
    for d in range(0, 3001, 250):
        c = unreal.Vector(s.x + fwd.x * d, s.y + fwd.y * d, s.z)
        floor = trace(c, unreal.Vector(c.x, c.y, c.z - 600))
        fz = floor.z if floor else None
        eye = unreal.Vector(c.x, c.y, (fz if fz is not None else c.z) + 120)
        l = trace(eye, unreal.Vector(eye.x - right.x * 2000, eye.y - right.y * 2000, eye.z))
        r = trace(eye, unreal.Vector(eye.x + right.x * 2000, eye.y + right.y * 2000, eye.z))
        f = trace(eye, unreal.Vector(eye.x + fwd.x * 3000, eye.y + fwd.y * 3000, eye.z))
        dist = lambda h: round(((h.x - eye.x) ** 2 + (h.y - eye.y) ** 2) ** 0.5) if h else None
        unreal.log_warning("[ZSInspect] d=%d floor=%s left=%s right=%s ahead=%s" % (d, fz, dist(l), dist(r), dist(f)))
