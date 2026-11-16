// Copyright (C) 2024 Van de Walle Bastien
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0

#include "RecallAIDSpawnInternal.h"

#include "AID/RecallAIDAsset.h"

void Recall::AID::Utils::Private::InitializeTimerSet(
	const URecallAIDAsset& AIDAsset,
	const FRandomStream& RandomStream,
	FRecallAIDSpawnTimerSet& OutTimers)
{
	const int32 SpawnGroupCount = AIDAsset.SpawnGroups.Num();
	OutTimers.GroupInstances.SetNum(SpawnGroupCount);
	OutTimers.PendingGroupIndex = INDEX_NONE;

	for (int32 GroupIndex = 0; GroupIndex < SpawnGroupCount; GroupIndex++)
	{
		const FVariableCollectionFloatRange& InitialTimer = AIDAsset.SpawnGroups[GroupIndex].InitialTimerRange;
		OutTimers.GroupInstances[GroupIndex].SpawnTimer = RandomStream.FRandRange(
			InitialTimer.GetMinValue(), InitialTimer.GetMaxValue());
	}
}

void Recall::AID::Utils::Private::UpdateBurstMode(const FSpawnRuleContext& Rule, FRecallAIDSpawnTimerSet& Timers)
{
	const FRecallAIDState* State = Rule.CurrentAIDState;
	if (State == nullptr
		|| State->SpawnRule != ERecallAIDSpawnRule::Parallel
		|| !State->bBurstMode
		|| State->BurstDuration <= 0.0f)
	{
		return;
	}

	for (int32 GroupIndex = 0; GroupIndex < Rule.AIDAsset.SpawnGroups.Num(); GroupIndex++)
	{
		if (!Timers.GroupInstances.IsValidIndex(GroupIndex))
		{
			continue;
		}

		const FSpawnGroupConfiguration Config = GetSpawnGroupConfiguration(
			Rule.AIDAsset.SpawnGroups[GroupIndex], State);
		FRecallAIDSpawnGroupInstance& Instance = Timers.GroupInstances[GroupIndex];
		if (Config.bGroupEnabled && !Instance.bBurstCompleted)
		{
			Instance.BurstTimer += Rule.DeltaTime;
			if (Instance.BurstTimer >= State->BurstDuration)
			{
				Instance.bBurstCompleted = true;
			}
		}
	}
}

bool Recall::AID::Utils::Private::ProcessSpawnTiming(
	float& SpawnTimer,
	const FSpawnGroupConfiguration& Config,
	float DeltaTime)
{
	if (SpawnTimer > 0.0f)
	{
		// Apply speed modifier to timer countdown
		const float TimerDelta = DeltaTime * Config.SpawnSpeedModifier;
		SpawnTimer = FMath::Max(0.0f, SpawnTimer - TimerDelta);
	}

	return FMath::IsNearlyZero(SpawnTimer);
}

float Recall::AID::Utils::Private::RollSpawnInterval(const FSpawnRuleContext& Rule, int32 GroupIndex)
{
	const FFloatInterval Bounds = GetSpawnIntervalBounds(Rule.AIDAsset.SpawnGroups[GroupIndex], Rule.AttributeSet);
	return Rule.RandomStream.FRandRange(Bounds.Min, Bounds.Max);
}

bool Recall::AID::Utils::Private::ScheduleNextSpawnGroup(const FSpawnRuleContext& Rule, FRecallAIDSpawnTimerSet& Timers)
{
	const int32 GroupIndex = SelectWeightedSpawnGroup(Rule);
	if (!Rule.AIDAsset.SpawnGroups.IsValidIndex(GroupIndex)
		|| !Timers.GroupInstances.IsValidIndex(GroupIndex))
	{
		Timers.PendingGroupIndex = INDEX_NONE;
		return false;
	}

	FRecallAIDSpawnGroupInstance& Instance = Timers.GroupInstances[GroupIndex];
	Timers.PendingGroupIndex = GroupIndex;
	if (Instance.bHasSpawned)
	{
		Instance.SpawnTimer = RollSpawnInterval(Rule, GroupIndex);
	}
	else
	{
		const FVariableCollectionFloatRange& InitialTimer = Rule.AIDAsset.SpawnGroups[GroupIndex].InitialTimerRange;
		Instance.SpawnTimer = Rule.RandomStream.FRandRange(InitialTimer.GetMinValue(), InitialTimer.GetMaxValue());
	}
	return true;
}
