// Copyright (C) 2024 Van de Walle Bastien
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0


#include "RecallAIDAsset.h"


TArray<FString> URecallAIDAsset::GetSpawnGroupNames() const
{
	TArray<FString> Names;
	Names.Reserve(SpawnGroups.Num());
	
	for (const FRecallAIDSpawnGroup& SpawnGroup : SpawnGroups)
	{
		Names.Add(SpawnGroup.GroupName.ToString());
	}
	
	return Names;
}

TArray<FString> URecallAIDAsset::GetAIDStateNames() const
{
	TArray<FString> Names;
	Names.Reserve(AIDStates.Num());
	
	for (const FRecallAIDState& AIDState : AIDStates)
	{
		Names.Add(AIDState.StateName.ToString());
	}
	
	return Names;
}
