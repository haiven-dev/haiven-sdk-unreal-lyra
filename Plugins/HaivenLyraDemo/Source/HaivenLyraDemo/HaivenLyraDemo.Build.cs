// The demo's build rules. It depends on the Haiven plugin and engine UI modules
// only, never on LyraGame, so the same plugin builds in the SDK's test host and
// drops into a fetched Lyra project unchanged.

using UnrealBuildTool;

public class HaivenLyraDemo : ModuleRules
{
	public HaivenLyraDemo(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings",
			// Public: HaivenLyraEntitlements.h takes the SDK's generated REST types.
			"Haiven",
			"UMG",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"InputCore",
			"Json",
			"Slate",
			"SlateCore",
		});

		// The smoke tests reach the editor world; game builds do not link the editor.
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
			// The sign-in test probes for a free loopback port.
			PrivateDependencyModuleNames.Add("Sockets");
		}
	}
}
