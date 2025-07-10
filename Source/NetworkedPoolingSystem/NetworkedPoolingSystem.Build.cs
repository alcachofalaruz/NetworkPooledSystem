// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO; // Include System.IO to handle path combinations

public class NetworkedPoolingSystem : ModuleRules
{
	public NetworkedPoolingSystem(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		// Add public include paths
		PublicIncludePaths.AddRange(
			new string[] {
				// Add the Public folder of the module to the include paths
				Path.Combine(ModuleDirectory, "Public")
				// Add other folders as needed
			}
		);

		// Add private include paths
		PrivateIncludePaths.AddRange(
			new string[] {
				// Add the Private folder of the module to the include paths
				Path.Combine(ModuleDirectory, "Private")
				// Add other private folders as needed
			}
		);
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core", "NetCore", "CoreUObject", "Engine", "DeveloperSettings", "GameplayAbilities" , "GameplayTasks"
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
