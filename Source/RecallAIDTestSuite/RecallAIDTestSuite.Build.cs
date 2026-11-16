// Copyright (C) 2024 Van de Walle Bastien
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0

using System.IO;
using UnrealBuildTool;

public class RecallAIDTestSuite : ModuleRules
{
	public RecallAIDTestSuite(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"RecallAID",
			"RecallAIDCore",
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"GameplayTags",
			"MassEntity",
			"MassCore",
			"RecallCore",
			"RecallSimulation",
			"RecallAttributeModule",
			"VariableCollectionModule",
		});

		// The tests exercise RecallAID's internal spawn rules (Utility/AID/RecallAIDSpawnInternal.h).
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "..", "RecallAID", "Private"));
	}
}
