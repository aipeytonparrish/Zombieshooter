# Regenerates Content/ZS_Imported/ (git-ignored) from the third-party source files in SourceArt/ThirdParty/.
# Close the Unreal Editor first, then run from PowerShell:
#   powershell -ExecutionPolicy Bypass -File Tools\Import\run_imports.ps1
$ErrorActionPreference = "Stop"
$Project = Resolve-Path (Join-Path $PSScriptRoot "..\..\Zombieshooter.uproject")
$Editor = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$Scripts = @("create_core_materials.py", "import_rd704.py", "import_ammoboxes.py")

foreach ($s in $Scripts) {
    $path = Join-Path $PSScriptRoot $s
    Write-Host "== $s"
    & $Editor $Project -run=pythonscript "-script=$path" -unattended -nullrhi -nosplash -stdout |
        Select-String -Pattern "\[ZSImport\]|Traceback|Exception" |
        ForEach-Object { $_.Line -replace '^\[[^\]]*\]\[[^\]]*\]', '' }
}
Write-Host "Done. Imported assets are in Content/ZS_Imported/."

# Test map (ours, versioned): delete the old file first so it can be regenerated.
$map = Join-Path $PSScriptRoot "..\..\Content\ZS\Maps\Test\L_ZSTest.umap"
if (Test-Path $map) { Remove-Item $map -Force }
Write-Host "== create_test_map.py"
& $Editor $Project -run=pythonscript "-script=$(Join-Path $PSScriptRoot 'create_test_map.py')" -unattended -nullrhi -nosplash -stdout |
    Select-String -Pattern "\[ZSImport\]|Traceback|Exception" |
    ForEach-Object { $_.Line -replace '^\[[^\]]*\]\[[^\]]*\]', '' }

# Bunker hub map (ours, versioned).
$bunker = Join-Path $PSScriptRoot "..\..\Content\ZS\Maps\L_Bunker.umap"
if (Test-Path $bunker) { Remove-Item $bunker -Force }
Write-Host "== create_bunker_map.py"
& $Editor $Project -run=pythonscript "-script=$(Join-Path $PSScriptRoot 'create_bunker_map.py')" -unattended -nullrhi -nosplash -stdout |
    Select-String -Pattern "\[ZSImport\]|Traceback|Exception" |
    ForEach-Object { $_.Line -replace '^\[[^\]]*\]\[[^\]]*\]', '' }

# FX materials (glow, rain) and the New York level (ours, versioned; its art is imported separately, see
# Docs/ThirdPartyAssets.md "New York level": blender_export_postapoc_ny.py, import_postapoc_ny.py and
# import_industrial_kit.py for the compound, then kit_instancing_flags.py; prepare_kitbash_buildings.py readies the kitbash backdrop buildings).
foreach ($s in @("create_fx_materials.py", "create_gore_fx.py", "kit_instancing_flags.py", "prepare_kitbash_buildings.py", "create_nyc_map.py")) {
    if ($s -eq "create_nyc_map.py") {
        $nyc = Join-Path $PSScriptRoot "..\..\Content\ZS\Maps\L_NYC.umap"
        if (Test-Path $nyc) { Remove-Item $nyc -Force }
    }
    Write-Host "== $s"
    & $Editor $Project -run=pythonscript "-script=$(Join-Path $PSScriptRoot $s)" -unattended -nullrhi -nosplash -stdout |
        Select-String -Pattern "\[ZSImport\]|Traceback|Exception" |
        ForEach-Object { $_.Line -replace '^\[[^\]]*\]\[[^\]]*\]', '' }
}
