using UnrealBuildTool;

public class NexoraOpenWorld : ModuleRules
{
	public NexoraOpenWorld(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"UMG",
			"AIModule"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });
	}
}
