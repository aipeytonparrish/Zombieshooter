"""Log the city packs' demo maps: actor classes, player starts, mesh bounds, lighting actors, and a floor grid
(for picking a playable street area and placing spawns/crates)."""
import unreal

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

MAPS = ["/Game/MS_NewYorkCityKitbash_Assets/Maps/MS_NewYorkCityKitbash_Assets", "/Game/ZombieCity01/Maps/ZombieCity01"]


def trace(world, a, b):
    hit = unreal.SystemLibrary.line_trace_single(world, a, b, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return None
    t = hit.to_tuple()
    return t[4] if t[0] else None


for m in MAPS:
    les.load_level(m)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    actors = eas.get_all_level_actors()
    tag = "[ZSInspect] " + m.split("/")[-1]
    classes = {}
    for a in actors:
        classes[a.get_class().get_name()] = classes.get(a.get_class().get_name(), 0) + 1
    unreal.log_warning("%s actors=%d classes=%s" % (tag, len(actors), sorted(classes.items(), key=lambda x: -x[1])[:25]))
    for a in actors:
        if isinstance(a, (unreal.PlayerStart, unreal.DirectionalLight, unreal.SkyLight, unreal.ExponentialHeightFog, unreal.SkyAtmosphere, unreal.PostProcessVolume, unreal.VolumetricCloud)):
            unreal.log_warning("%s %s %s at %s rot %s" % (tag, a.get_class().get_name(), a.get_actor_label(), a.get_actor_location(), a.get_actor_rotation()))
    xs, ys, zs = [], [], []
    for a in actors:
        if isinstance(a, unreal.StaticMeshActor):
            p = a.get_actor_location()
            xs.append(p.x); ys.append(p.y); zs.append(p.z)
    if not xs:
        continue
    xs.sort(); ys.sort(); zs.sort()
    n = len(xs)
    pct = lambda v, q: v[min(n - 1, int(n * q))]
    unreal.log_warning("%s meshes=%d x[%d..%d] p10-90 x[%d..%d] y[%d..%d] z[%d..%d]" % (tag, n, xs[0], xs[-1], pct(xs, .1), pct(xs, .9), pct(ys, .1), pct(ys, .9), pct(zs, .1), pct(zs, .9)))
    # Floor grid over the core area: floor height per cell ('.' = no floor hit within range).
    x0, x1, y0, y1 = pct(xs, .1), pct(xs, .9), pct(ys, .1), pct(ys, .9)
    step = max(200, int(max(x1 - x0, y1 - y0) / 30))
    zc = pct(zs, .9) + 3000
    for yy in range(int(y0), int(y1) + 1, step):
        row = []
        for xx in range(int(x0), int(x1) + 1, step):
            h = trace(world, unreal.Vector(xx, yy, zc), unreal.Vector(xx, yy, pct(zs, .1) - 3000))
            row.append("%5d" % h.z if h else "    .")
        unreal.log_warning("%s y=%6d %s" % (tag, yy, " ".join(row)))
    unreal.log_warning("%s grid x from %d step %d" % (tag, x0, step))
