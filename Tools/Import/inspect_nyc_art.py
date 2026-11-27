"""Map the imported New York art level: actor bounds, and a top-down height grid (traced) around the street so the
play area, spawn points and crates can be placed. Writes Saved/nyc_grid.txt."""
import os
import unreal

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les.load_level("/Game/NYC_PostApoc/Maps/L_NYC_Art")
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
out = []
actors = eas.get_all_level_actors()
out.append("actors %d" % len(actors))
for a in actors:
    o, e = a.get_actor_bounds(False)
    out.append("A %-40s c(%d,%d,%d) e(%d,%d,%d) %s" % (a.get_actor_label()[:40], o.x, o.y, o.z, e.x, e.y, e.z, a.get_class().get_name()))


def trace(x, y):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 20000), unreal.Vector(x, y, -20000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return None
    t = hit.to_tuple()
    return t[4].z if t[0] else None


X0, X1, Y0, Y1, STEP = -12000, 12000, -16000, 16000, 500
out.append("grid x %d..%d y %d..%d step %d (value = floor z in m, '.' = nothing)" % (X0, X1, Y0, Y1, STEP))
for y in range(Y0, Y1 + 1, STEP):
    row = []
    for x in range(X0, X1 + 1, STEP):
        z = trace(x, y)
        row.append("   ." if z is None else "%4d" % round(z / 100.0))
    out.append("y=%6d %s" % (y, "".join(row)))
path = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "nyc_grid.txt")
open(path, "w").write("\n".join(out))
unreal.log_warning("[ZSInspect] wrote " + path)
