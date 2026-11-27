"""Re-import Data/ZSRounds.csv into /Game/ZS/Data/CT_ZSRounds after editing it (create_core_materials.py does the same
as part of the full setup).

  UnrealEditor-Cmd.exe Zombieshooter.uproject -run=pythonscript -script=Tools/Import/import_round_curves.py
"""
import os
import sys

import unreal

sys.path.append(os.path.dirname(__file__))
import zs_import_common as zs  # noqa: E402

task = unreal.AssetImportTask()
task.filename = os.path.join(zs.PROJECT_DIR, "Data", "ZSRounds.csv")
task.destination_path = "/Game/ZS/Data"
task.destination_name = "CT_ZSRounds"
task.automated = True
task.replace_existing = True
task.save = True
factory = unreal.CSVImportFactory()
settings = unreal.CSVImportSettings()
settings.set_editor_property("import_type", unreal.CSVImportType.ECSV_CURVE_TABLE)
settings.set_editor_property("import_curve_interp_mode", unreal.RichCurveInterpMode.RCIM_LINEAR)
factory.set_editor_property("automated_import_settings", settings)
task.factory = factory
zs.asset_tools.import_asset_tasks([task])
zs.log("Imported round curves: " + str(list(task.imported_object_paths)))
