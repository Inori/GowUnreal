// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class GowThirdParty : ModuleRules
{
	public GowThirdParty(ReadOnlyTargetRules Target) : base(Target)
	{
        Type = ModuleType.External;

        PrivateDefinitions.Add("_DEBUG");

        PublicIncludePaths.AddRange(
            new string[] {
                Path.Combine(ModuleDirectory, "src"),
                Path.Combine(ModuleDirectory, "glm")
            }
            );

        string LibPath = Path.Combine(ModuleDirectory, "lib");
        PublicAdditionalLibraries.Add(Path.Combine(LibPath, "renderdoc.lib"));
        PublicAdditionalLibraries.Add(Path.Combine(LibPath, "GowThirdParty.lib"));

        string BinPath = Path.Combine(ModuleDirectory, "bin", "renderdoc.dll");
        RuntimeDependencies.Add("$(BinaryOutputDir)/renderdoc.dll", BinPath);
    }
}
