// Copyright (C) 2024 Van de Walle Bastien
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0

#include "RecallAIDSpawnInternal.h"

#include "AID/RecallAIDAsset.h"
#include "Desync/RecallDesyncLog.h"

using namespace Recall::AID::Utils::Private;

void Recall::AID::Utils::Private::ProcessParallelRule(
	const FSpawnRuleContext& Rule,
	FRecallAIDSpawnTimerSet& Timers,
	const UWorld* World,
	const FMassEntityHandle& AIDEntity,
	FTrySpawnGroupFunc TrySpawnGroup)
{
	for (int32 GroupIndex = 0; GroupIndex < Rule.AIDAsset.SpawnGroups.Num(); GroupIndex++)
	{
		if (!Timers.GroupInstances.IsValidIndex(GroupIndex))
		{
			continue;
		}

		FRecallAIDSpawnGroupInstance& Instance = Timers.GroupInstances[GroupIndex];
		const FSpawnGroupConfiguration Config = GetSpawnGroupConfiguration(
			Rule.AIDAsset.SpawnGroups[GroupIndex], Rule.CurrentAIDState);
		if (!Config.bGroupEnabled || (Config.bBurstMode && Instance.bBurstCompleted))
		{
			continue;
		}

#if RECALL_DESYNC_LOG
		RECALL_DESYNC_LOG_STR(World, AIDSpawnGroupTiming,
			FString::Printf(TEXT("%s Rule: Parallel, GroupIndex: %d, SpawnTimer: %f"),
			*AIDEntity.DebugGetDescription(), GroupIndex, Instance.SpawnTimer));
#endif // RECALL_DESYNC_LOG

		if (!ProcessSpawnTiming(Instance.SpawnTimer, Config, Rule.DeltaTime))
		{
			continue;
		}

		if (!TrySpawnGroup(GroupIndex, Config))
		{
			continue;
		}

		Instance.bHasSpawned = true;
		if (Config.bBurstMode && FMath::IsNearlyZero(Config.BurstDuration))
		{
			Instance.bBurstCompleted = true;
			continue;
		}

		Instance.SpawnTimer = RollSpawnInterval(Rule, GroupIndex);
	}
}

void Recall::AID::Utils::Private::ProcessRandomWeightRule(
	const FSpawnRuleContext& Rule,
	FRecallAIDSpawnTimerSet& Timers,
	const UWorld* World,
	const FMassEntityHandle& AIDEntity,
	FTrySpawnGroupFunc TrySpawnGroup)
{
	if (!Timers.IsSpawnTimerInitialized() && !ScheduleNextSpawnGroup(Rule, Timers))
	{
		return;
	}

	if (!Rule.AIDAsset.SpawnGroups.IsValidIndex(Timers.PendingGroupIndex)
		|| !Timers.GroupInstances.IsValidIndex(Timers.PendingGroupIndex))
	{
		Timers.PendingGroupIndex = INDEX_NONE;
		return;
	}

	int32 GroupIndex = Timers.PendingGroupIndex;
	FSpawnGroupConfiguration Config = GetSpawnGroupConfiguration(
		Rule.AIDAsset.SpawnGroups[GroupIndex], Rule.CurrentAIDState);

	// A state edit may have disabled the selected group while its timer was running.
	if (!Config.bGroupEnabled)
	{
		Timers.PendingGroupIndex = INDEX_NONE;
		if (!ScheduleNextSpawnGroup(Rule, Timers))
		{
			return;
		}

		GroupIndex = Timers.PendingGroupIndex;
		Config = GetSpawnGroupConfiguration(Rule.AIDAsset.SpawnGroups[GroupIndex], Rule.CurrentAIDState);
	}

	FRecallAIDSpawnGroupInstance& SelectedInstance = Timers.GroupInstances[GroupIndex];
	const bool bTimingAllowsSpawn = ProcessSpawnTiming(SelectedInstance.SpawnTimer, Config, Rule.DeltaTime);
#if RECALL_DESYNC_LOG
	RECALL_DESYNC_LOG_STR(World, AIDSpawnGroupTiming,
		FString::Printf(TEXT("%s Rule: RandomWeight, GroupIndex: %d, bTimingAllowsSpawn: %s, SpawnTimer: %f"),
		*AIDEntity.DebugGetDescription(), GroupIndex,
		*Recall::Desync::Conv_BoolToString(bTimingAllowsSpawn), SelectedInstance.SpawnTimer));
#endif // RECALL_DESYNC_LOG

	if (!bTimingAllowsSpawn || !TrySpawnGroup(GroupIndex, Config))
	{
		return;
	}

	SelectedInstance.bHasSpawned = true;
	Timers.PendingGroupIndex = INDEX_NONE;
	ScheduleNextSpawnGroup(Rule, Timers);
}
