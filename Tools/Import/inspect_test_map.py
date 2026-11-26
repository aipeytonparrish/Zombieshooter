import os, sys, unreal
sys.path.append(os.path.dirname(__file__))
import zs_import_common as zs
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
les.load_level("/Game/ZS/Maps/Test/L_ZSTest")
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for a in eas.get_all_level_actors():
    if isinstance(a, unreal.ZSAmmoCrate) or isinstance(a, unreal.PlayerStart):
        comps = a.get_components_by_class(unreal.StaticMeshComponent)
        zs.log("%s at %s comps=%s" % (a.get_actor_label(), a.get_actor_location(), [(c.get_name(), c.static_mesh.get_name() if c.static_mesh else None) for c in comps]))
