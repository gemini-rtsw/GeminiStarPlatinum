// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GeminiStarPlatinum : ModuleRules
{
	public GeminiStarPlatinum(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });

		// Live data feed (Python bridge over TCP/JSON-lines): sockets for transport, Json for payload parsing.
		PrivateDependencyModuleNames.AddRange(new string[] { "Sockets", "Networking", "Json", "CelestialVault"});


		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
