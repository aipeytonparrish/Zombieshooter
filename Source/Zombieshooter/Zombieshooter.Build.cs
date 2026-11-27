using UnrealBuildTool;

public class Zombieshooter : ModuleRules
{
	public Zombieshooter(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"GameplayTags",
			"UMG",
			"Slate",
			"SlateCore",
			"CommonUI",
			"CommonInput",
			"AIModule",
			"NavigationSystem",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"Niagara",
			"PhysicsCore", "AnimationCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"RenderCore"
		});

		// OnlineSubsystemSteam is enabled as a plugin in Zombieshooter.uproject and configured in
		// Config/DefaultEngine.ini (dev App ID 480). We talk to it through the generic OnlineSubsystem
		// interfaces, so no direct module dependency is needed.
	}
}
