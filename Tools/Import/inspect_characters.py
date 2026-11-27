"""Log skeletons, material slots and material parameters of the survivor bodies and FP glove arms (for M9)."""
import unreal

paths = ["/Game/SurvivorsMegaPack/Survivor%d/Mesh/SK_Survivor%d" % (i, i) for i in range(1, 6)]
paths += ["/Game/AAA_FPS_Gloves/Modules/SK_Arms_%02d" % i for i in range(1, 6)]
paths += ["/Game/AAA_FPS_Gloves_Vol2/Modules/SK_Arms_%02d" % i for i in range(6, 12)]

mel = unreal.MaterialEditingLibrary
for p in paths:
    m = unreal.load_asset(p)
    if not m:
        unreal.log_warning("[ZSInspect] missing %s" % p)
        continue
    skel = m.get_editor_property("skeleton")
    unreal.log_warning("[ZSInspect] %s skel=%s bones=%d" % (p, skel.get_path_name() if skel else None, m.get_editor_property("skeleton").get_editor_property("bone_tree").__len__() if False else -1))
    for sm in m.get_editor_property("materials"):
        mi = sm.get_editor_property("material_interface")
        names = []
        if mi:
            try:
                names = [str(n) for n in mel.get_vector_parameter_names(mi)]
            except Exception as e:  # noqa
                names = ["?" + str(e)]
        unreal.log_warning("[ZSInspect]    slot=%s mat=%s vec=%s" % (sm.get_editor_property("material_slot_name"), mi.get_path_name() if mi else None, names))

for i in range(1, 6):
    d = "/Game/SurvivorsMegaPack/Survivor%d/Animations/Demo" % i
    for a in unreal.EditorAssetLibrary.list_assets(d, False):
        unreal.log_warning("[ZSInspect] anim %s" % a)
