using UnrealBuildTool;

public class StrategicSimulationPlugin : ModuleRules
{
    public StrategicSimulationPlugin(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "UMG",
                "CommonUI",
                "CommonInput"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "InputCore",
                "Slate",
                "SlateCore"
            }
        );
    }
}