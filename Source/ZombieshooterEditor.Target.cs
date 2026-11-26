using UnrealBuildTool;
using System.Collections.Generic;

public class ZombieshooterEditorTarget : TargetRules
{
	public ZombieshooterEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("Zombieshooter");
	}
}
