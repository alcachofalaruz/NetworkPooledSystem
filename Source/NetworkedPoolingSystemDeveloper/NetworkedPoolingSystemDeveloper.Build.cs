// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class NetworkedPoolingSystemDeveloper : ModuleRules
{
	public NetworkedPoolingSystemDeveloper(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
 
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "NetworkedPoolingSystem" });

		PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "KismetCompiler", "BlueprintGraph" });

		// Only compile this module for editor builds
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "EditorStyle", "SlateCore", "Slate" });
		}
		
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));
		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));
	}
}
