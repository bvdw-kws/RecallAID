// Copyright (C) 2024 Van de Walle Bastien
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0

namespace UnrealBuildTool.Rules
{
	public class RecallAID : ModuleRules
	{
		public RecallAID(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

			PublicDependencyModuleNames.AddRange(new string[] { 
					"Engine", 
					"RecallAIDCore",
					"GameplayTags",
					"VariableCollectionModule",
				}
			);
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"MassEntity",
					"MassCore",
					"MassSpawner",
					"RecallCore",
					"RecallGameplayCore",
					"RecallSimulation",
					"RecallPhysicsModule",
					"RecallGameplay",
					"RecallAttributeModule",
					"RecallSignals",
					"NavigationSystem",
					"StateTreeModule",
				}
			);

			if (RecallCore.IsPluginEnabled(Target, "DebugMenu"))
			{
				PrivateDependencyModuleNames.Add("DebugMenu");
			}
		}
	}
}
