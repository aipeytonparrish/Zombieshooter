# Regenerates Content/ZS_Imported/ (git-ignored) from the third-party source files in SourceArt/ThirdParty/.
# Close the Unreal Editor first, then run from PowerShell:
#   powershell -ExecutionPolicy Bypass -File Tools\Import\run_imports.ps1
$ErrorActionPreference = "Stop"
$Project = Resolve-Path (Join-Path $PSScriptRoot "..\..\Zombieshooter.uproject")
$Editor = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$Scripts = @("import_rd704.py")

foreach ($s in $Scripts) {
    $path = Join-Path $PSScriptRoot $s
    Write-Host "== $s"
    & $Editor $Project -run=pythonscript "-script=$path" -unattended -nullrhi -nosplash -stdout |
        Select-String -Pattern "\[ZSImport\]|Traceback|Exception" |
        ForEach-Object { $_.Line -replace '^\[[^\]]*\]\[[^\]]*\]', '' }
}
Write-Host "Done. Imported assets are in Content/ZS_Imported/."
