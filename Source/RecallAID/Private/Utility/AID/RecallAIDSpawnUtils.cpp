// Copyright (C) 2024 Van de Walle Bastien
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0


#include "Utility/AID/RecallAIDSpawnUtils.h"

#include "MassEntityView.h"
#include "MassExecutionContext.h"
#include "AID/RecallAIDAsset.h"
#include "AID/RecallAIDSpawnPointSettings.h"
#include "AID/RecallAIDSpawnTypes.h"
#include "Attribute/RecallAttributeSetTypes.h"
#include "Desync/RecallDesyncLog.h"
#include "Entity/RecallAIDSpawnCommand.h"
#include "NavigationSystem.h"
#include "Simulation/AID/RecallAIDFragments.h"
#include "System/Entity/RecallEntityAsyncSpawnSubsystem.h"
#include "System/Entity/RecallEntityAsyncSpawnTypes.h"
#include "System/Random/RecallRandomNumberSubsystem.h"
#include "Simulation/AID/RecallAIDSpawnGroupModifierTypes.h"
#include "Utility/Simulation/RecallSimulationUtils.h"

/**
 * Creates spawn parameters for entity spawning.
 */
static FRecallEntityAsyncSpawnParameters CreateSpawnParameters(
	const FMassEntityHandle& AIDEntity,
	int32 EntityCount)
{
	FRecallEntityAsyncSpawnParameters SpawnParams;
	SpawnParams.EntityCount = EntityCount;
	SpawnParams.SpawnCommand.InitializeAs<FRecallAIDSpawnCommand>();

	FRecallAIDSpawnCommand& SpawnCommand = SpawnParams.SpawnCommand.GetMutable<FRecallAIDSpawnCommand>();
	SpawnCommand.SpawnInfos.SetNum(SpawnParams.EntityCount);
	SpawnCommand.AIDEntity = AIDEntity;

	return SpawnParams;
}

/**
 * Calculates spawn positions for entities within a spawn area.
 */
static void CalculateSpawnPositions(
	const FRecallAIDSpawnGroup& SpawnGroup,
	const FRecallAIDSpawnPointCache& SpawnPoint,
	const FRandomStream& RandomStream,
	UNavigationSystemV1* NavSys,
	TArray<FRecallAIDSpawnInfo>& OutSpawnInfos)
{
	const float SpawnArea = SpawnPoint.SpawnPointSettings->SpawnArea;
	const FBox SpawnBox(SpawnPoint.Location - FVector(SpawnArea, SpawnArea, 0.0f), 
		SpawnPoint.Location + FVector(SpawnArea, SpawnArea, 1.0f));
	
	for (int32 EntityIndex = 0; EntityIndex < OutSpawnInfos.Num(); EntityIndex++)
	{
		FRecallAIDSpawnInfo& SpawnInfo = OutSpawnInfos[EntityIndex];

		FNavLocation NavLocation;
		const FVector RandomPosition = RandomStream.RandPointInBox(SpawnBox);
		if (!SpawnGroup.bProjectOntoNavigation)
		{
			SpawnInfo.Position = RandomPosition;
		}
		else if (NavSys && NavSys->ProjectPointToNavigation(RandomPosition, NavLocation))
		{
			SpawnInfo.Position = NavLocation.Location;
		}
		else
		{
			SpawnInfo.Position = SpawnPoint.Location;
		}
	}
}

int32 Recall::AID::Utils::SpawnEnemyGroup(
	const FRecallAIDSpawnContext& SpawnContext,
	const FMassEntityHandle& AIDEntity,
	const FRecallAIDSpawnGroup& SpawnEnemyGroup,
	const TArray<FRecallAIDSpawnPointCache>& SpawnPoints,
	int32 EntityCount,
	float CurrentTime)
{
	// TODO: Filter spawn points based on gameplay tags to support different spawn zones
	
	if (SpawnPoints.Num() == 0 || EntityCount == 0)
	{
		return 0;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(&SpawnContext.World);
	const FRandomStream& RandomStream = SpawnContext.RandomNumberSystem.GetRandomStream();
	
	// Select spawn point
	const int32 SpawnPointIndex = RandomStream.RandRange(0, SpawnPoints.Num() - 1);
	const FRecallAIDSpawnPointCache& SpawnPoint = SpawnPoints[SpawnPointIndex];

#if RECALL_DESYNC_LOG
	RECALL_DESYNC_LOG_STR(&SpawnContext.World, AIDSpawnEnemyGroup,
		FString::Printf(TEXT("%s EntityCount: %d, CurrentTime: %f, SpawnPointIndex: %d, SpawnPointCount: %d, SpawnPointLocation: %s"),
		*AIDEntity.DebugGetDescription(), EntityCount, CurrentTime, SpawnPointIndex, SpawnPoints.Num(), *SpawnPoint.Location.ToString()));
#endif // RECALL_DESYNC_LOG

	// Create spawn parameters
	FRecallEntityAsyncSpawnParameters SpawnParams = CreateSpawnParameters(AIDEntity, EntityCount);
	FRecallAIDSpawnCommand& SpawnCommand = SpawnParams.SpawnCommand.GetMutable<FRecallAIDSpawnCommand>();
	
	// Calculate spawn positions
	CalculateSpawnPositions(SpawnEnemyGroup, SpawnPoint, RandomStream, NavSys, SpawnCommand.SpawnInfos);

#if RECALL_DESYNC_LOG
	for (int32 SpawnInfoIndex = 0; SpawnInfoIndex < SpawnCommand.SpawnInfos.Num(); SpawnInfoIndex++)
	{
		RECALL_DESYNC_LOG_STR(&SpawnContext.World, AIDSpawnEnemyGroupPosition,
			FString::Printf(TEXT("%s SpawnInfoIndex: %d, Position: %s"),
			*AIDEntity.DebugGetDescription(), SpawnInfoIndex, *SpawnCommand.SpawnInfos[SpawnInfoIndex].Position.ToString()));
	}
#endif // RECALL_DESYNC_LOG
	
	// Spawn entities
	SpawnContext.EntityAsyncSpawnSystem.SpawnEntityAsync(SpawnEnemyGroup.EntityConfigAsset,
		SpawnPoint.Location, SpawnPoint.Rotation, SpawnParams);

	// Update the spawn point's last spawn time to start its cooldown
	if (SpawnPoint.SpawnPointFragment)
	{
		SpawnPoint.SpawnPointFragment->LastSpawnTime = CurrentTime;
	}

	return EntityCount;
}

float Recall::AID::Utils::GetSpawnGroupEntryWeight(
	const FRecallAIDStateGroupEntry& GroupEntry,
	const FRecallAttributeSet* AttributeSet)
{
	float Weight = GroupEntry.Weight;
	for (const FInstancedStruct& ModifierData : GroupEntry.WeightModifiers)
	{
		if (const FRecallAIDSpawnGroupWeightModifierBase* ModifierPtr = ModifierData.GetPtr<FRecallAIDSpawnGroupWeightModifierBase>())
		{
			Weight *= ModifierPtr->GetWeightMultiplier(AttributeSet);
		}
	}

	return FMath::IsFinite(Weight) ? FMath::Max(0.0f, Weight) : 0.0f;
}

/**
 * Filters spawn points that are not on cooldown.
 */
static void FilterAvailableSpawnPoints(
	const TArray<FRecallAIDSpawnPointCache>& AllSpawnPoints,
	const URecallAIDAsset& AIDAsset,
	float CurrentTime,
	TArray<FRecallAIDSpawnPointCache>& OutAvailableSpawnPoints)
{
	OutAvailableSpawnPoints.Reset();
	OutAvailableSpawnPoints.Reserve(AllSpawnPoints.Num());
	
	for (const FRecallAIDSpawnPointCache& SpawnPointCache : AllSpawnPoints)
	{
		if (SpawnPointCache.SpawnPointFragment)
		{
			const float TimeSinceLastSpawn = CurrentTime - SpawnPointCache.SpawnPointFragment->LastSpawnTime;
			if (SpawnPointCache.SpawnPointFragment->LastSpawnTime < 0.0f || TimeSinceLastSpawn >= AIDAsset.SpawnPointCooldown)
			{
				OutAvailableSpawnPoints.Add(SpawnPointCache);
			}
		}
		else
		{
			// Safety fallback: if fragment is null, consider it available
			OutAvailableSpawnPoints.Add(SpawnPointCache);
		}
	}
}

/**
 * Extract and configure spawn parameters from the current AID state.
 */
struct FSpawnGroupConfiguration
{
	bool bGroupEnabled = true;
	float BaseWeight = 1.0f;
	float SpawnSpeedModifier = 1.0f;
	float SpawnRateModifier = 1.0f;
	bool bBurstMode = false;
	float BurstDuration = 0.0f;
};

static FSpawnGroupConfiguration GetSpawnGroupConfiguration(
	const FRecallAIDSpawnGroup& SpawnGroup,
	const FRecallAIDState* CurrentAIDState)
{
	FSpawnGroupConfiguration Config;
	
	if (CurrentAIDState)
	{
		// Check if this group is configured in the current state
		const FRecallAIDStateGroupEntry* GroupEntry = CurrentAIDState->SpawnGroupEntries.FindByPredicate(
			[&](const FRecallAIDStateGroupEntry& Entry) { return Entry.SpawnGroupName == SpawnGroup.GroupName; });
		
		if (GroupEntry)
		{
			Config.bGroupEnabled = GroupEntry->bEnabled;
			Config.BaseWeight = GroupEntry->Weight;
		}
		else
		{
			// Group not mentioned in state = disabled
			Config.bGroupEnabled = false;
		}
		
		Config.SpawnSpeedModifier = CurrentAIDState->SpawnSpeedModifier;
		Config.SpawnRateModifier = CurrentAIDState->SpawnRateModifier;
		Config.bBurstMode = CurrentAIDState->SpawnRule == ERecallAIDSpawnRule::Parallel
			&& CurrentAIDState->bBurstMode;
		Config.BurstDuration = Config.bBurstMode ? CurrentAIDState->BurstDuration : 0.0f;
	}
	// else: No state = default behavior (all groups enabled)
	
	return Config;
}

/**
 * Selects one enabled group using its effective state-entry weight.
 */
static int32 SelectWeightedSpawnGroup(
	const URecallAIDAsset& AIDAsset,
	const FRecallAIDState* CurrentAIDState,
	const FRecallAttributeSet* AttributeSet,
	const FRandomStream& RandomStream)
{
	TArray<int32> CandidateIndices;
	TArray<float> CandidateWeights;
	float TotalWeight = 0.0f;

	for (int32 GroupIndex = 0; GroupIndex < AIDAsset.SpawnGroups.Num(); GroupIndex++)
	{
		const FRecallAIDSpawnGroup& SpawnGroup = AIDAsset.SpawnGroups[GroupIndex];
		const FSpawnGroupConfiguration Config = GetSpawnGroupConfiguration(SpawnGroup, CurrentAIDState);
		const FRecallAIDStateGroupEntry* GroupEntry = CurrentAIDState
			? CurrentAIDState->SpawnGroupEntries.FindByPredicate([&](const FRecallAIDStateGroupEntry& Entry)
				{
					return Entry.SpawnGroupName == SpawnGroup.GroupName;
				})
			: nullptr;
		const float EffectiveWeight = CurrentAIDState && GroupEntry
			? Recall::AID::Utils::GetSpawnGroupEntryWeight(*GroupEntry, AttributeSet)
			: 0.0f;
		if (!Config.bGroupEnabled || EffectiveWeight <= 0.0f || !FMath::IsFinite(EffectiveWeight))
		{
			continue;
		}

		CandidateIndices.Add(GroupIndex);
		CandidateWeights.Add(EffectiveWeight);
		TotalWeight += EffectiveWeight;
	}

	if (CandidateIndices.IsEmpty() || TotalWeight <= 0.0f || !FMath::IsFinite(TotalWeight))
	{
		return INDEX_NONE;
	}

	float WeightRoll = RandomStream.FRand() * TotalWeight;
	for (int32 CandidateIndex = 0; CandidateIndex < CandidateIndices.Num(); CandidateIndex++)
	{
		WeightRoll -= CandidateWeights[CandidateIndex];
		if (WeightRoll < 0.0f)
		{
			return CandidateIndices[CandidateIndex];
		}
	}

	// Floating-point rounding can leave the roll at the upper bound.
	return CandidateIndices.Last();
}

/**
 * Advances duration-based burst timers for enabled groups.
 */
static void UpdateBurstMode(
	const URecallAIDAsset& AIDAsset,
	FRecallAIDFragment& AIDFragment,
	const FRecallAIDState* CurrentAIDState,
	float DeltaTime)
{
	if (CurrentAIDState == nullptr
		|| CurrentAIDState->SpawnRule != ERecallAIDSpawnRule::Parallel
		|| !CurrentAIDState->bBurstMode
		|| CurrentAIDState->BurstDuration <= 0.0f)
	{
		return;
	}

	for (int32 GroupIndex = 0; GroupIndex < AIDAsset.SpawnGroups.Num(); GroupIndex++)
	{
		if (!AIDFragment.SpawnGroupInstances.IsValidIndex(GroupIndex))
		{
			continue;
		}

		const FSpawnGroupConfiguration Config = GetSpawnGroupConfiguration(
			AIDAsset.SpawnGroups[GroupIndex], CurrentAIDState);
		FRecallAIDSpawnGroupInstance& Instance = AIDFragment.SpawnGroupInstances[GroupIndex];
		if (Config.bGroupEnabled && !Instance.bBurstCompleted)
		{
			Instance.BurstTimer += DeltaTime;
			if (Instance.BurstTimer >= CurrentAIDState->BurstDuration)
			{
				Instance.bBurstCompleted = true;
			}
		}
	}
}

/**
 * Process spawn timing and determine if it's time to spawn entities.
 * Returns true if spawning should occur this frame.
 */
static bool ProcessSpawnTiming(
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

static bool ScheduleNextSpawnGroup(
	const URecallAIDAsset& AIDAsset,
	FRecallAIDFragment& AIDFragment,
	const FRecallAIDState* CurrentAIDState,
	const FRecallAttributeSet* AttributeSet,
	const FRandomStream& RandomStream)
{
	const int32 GroupIndex = SelectWeightedSpawnGroup(
		AIDAsset, CurrentAIDState, AttributeSet, RandomStream);
	if (!AIDAsset.SpawnGroups.IsValidIndex(GroupIndex)
		|| !AIDFragment.SpawnGroupInstances.IsValidIndex(GroupIndex))
	{
		AIDFragment.PendingSpawnGroupIndex = INDEX_NONE;
		return false;
	}

	const FRecallAIDSpawnGroup& SpawnGroup = AIDAsset.SpawnGroups[GroupIndex];
	FRecallAIDSpawnGroupInstance& Instance = AIDFragment.SpawnGroupInstances[GroupIndex];
	const FVariableCollectionFloatRange& TimerRange =Instance.bHasSpawned ? SpawnGroup.SpawnIntervalRange : SpawnGroup.InitialTimerRange;
	AIDFragment.PendingSpawnGroupIndex = GroupIndex;
	Instance.SpawnTimer = RandomStream.FRandRange(
		TimerRange.GetMinValue(), TimerRange.GetMaxValue());
	return true;
}

void Recall::AID::Utils::ProcessAIDSpawnGroups(
	const FRecallAIDSpawnContext& SpawnContext,
	const URecallAIDAsset& AIDAsset,
	const FMassEntityHandle& AIDEntity,
	FRecallAIDFragment& AIDFragment,
	const FRecallAIDState* CurrentAIDState,
	const TArray<FRecallAIDSpawnPointCache>& SpawnPoints,
	const FRandomStream& RandomStream,
	const FRecallAttributeSet* AttributeSet,
	float DeltaTime)
{
	const float CurrentTime = static_cast<float>(Recall::Simulation::Utils::GetTimeSeconds(&SpawnContext.World));

	TArray<FRecallAIDSpawnPointCache> AvailableSpawnPoints;
	FilterAvailableSpawnPoints(SpawnPoints, AIDAsset, CurrentTime, AvailableSpawnPoints);

#if RECALL_DESYNC_LOG
	RECALL_DESYNC_LOG_STR(&SpawnContext.World, AIDProcessSpawnGroups,
		FString::Printf(TEXT("%s CurrentTime: %f, SpawnPointCount: %d, AvailableSpawnPointCount: %d, CurrentAIDStateName: %s"),
		*AIDEntity.DebugGetDescription(), CurrentTime, SpawnPoints.Num(), AvailableSpawnPoints.Num(),
		CurrentAIDState ? *CurrentAIDState->StateName.ToString() : TEXT("None")));
#endif // RECALL_DESYNC_LOG

	if (AvailableSpawnPoints.Num() == 0)
	{
		return;
	}

	UpdateBurstMode(AIDAsset, AIDFragment, CurrentAIDState, DeltaTime);

	const auto TrySpawnGroup = [&](int32 GroupIndex, const FSpawnGroupConfiguration& Config) -> bool
	{
		const FRecallAIDSpawnGroup& SpawnGroup = AIDAsset.SpawnGroups[GroupIndex];
		const int32 BaseEntityCount = RandomStream.RandRange(SpawnGroup.EntityCount.Min, SpawnGroup.EntityCount.Max);
		const int32 EntityCount = FMath::Max(1, FMath::RoundToInt(BaseEntityCount * Config.SpawnRateModifier));

#if RECALL_DESYNC_LOG
		RECALL_DESYNC_LOG_STR(&SpawnContext.World, AIDSpawnGroupEntityCount,
			FString::Printf(TEXT("%s GroupIndex: %d, BaseEntityCount: %d, EntityCount: %d, MobCount: %d, MobLimit: %d"),
			*AIDEntity.DebugGetDescription(), GroupIndex, BaseEntityCount, EntityCount,
			AIDFragment.MobCount, AIDAsset.MobLimit));
#endif // RECALL_DESYNC_LOG

		if (AIDFragment.MobCount + EntityCount > AIDAsset.MobLimit)
		{
			return false;
		}

		const int32 SpawnedCount = SpawnEnemyGroup(
			SpawnContext, AIDEntity, SpawnGroup, AvailableSpawnPoints, EntityCount, CurrentTime);
		AIDFragment.MobCount += SpawnedCount;

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
		UE_LOG(LogTemp, VeryVerbose, TEXT("%hs: Spawned %d entities for AI Director. New mob count: %d"),
			__FUNCTION__, SpawnedCount, AIDFragment.MobCount);
#endif

		return SpawnedCount > 0;
	};

	const bool bRandomWeightRule = CurrentAIDState
		&& CurrentAIDState->SpawnRule == ERecallAIDSpawnRule::RandomWeight;
	if (bRandomWeightRule)
	{
		if (!AIDFragment.IsSpawnTimerInitialized()
			&& !ScheduleNextSpawnGroup(AIDAsset, AIDFragment, CurrentAIDState, AttributeSet, RandomStream))
		{
			return;
		}

		if (!AIDAsset.SpawnGroups.IsValidIndex(AIDFragment.PendingSpawnGroupIndex)
			|| !AIDFragment.SpawnGroupInstances.IsValidIndex(AIDFragment.PendingSpawnGroupIndex))
		{
			AIDFragment.PendingSpawnGroupIndex = INDEX_NONE;
			return;
		}

		int32 GroupIndex = AIDFragment.PendingSpawnGroupIndex;
		FSpawnGroupConfiguration Config = GetSpawnGroupConfiguration(
			AIDAsset.SpawnGroups[GroupIndex], CurrentAIDState);

		// A state edit may have disabled the selected group while its timer was running.
		if (!Config.bGroupEnabled)
		{
			AIDFragment.PendingSpawnGroupIndex = INDEX_NONE;
			if (!ScheduleNextSpawnGroup(AIDAsset, AIDFragment, CurrentAIDState, AttributeSet, RandomStream))
			{
				return;
			}

			GroupIndex = AIDFragment.PendingSpawnGroupIndex;
			Config = GetSpawnGroupConfiguration(AIDAsset.SpawnGroups[GroupIndex], CurrentAIDState);
		}

		FRecallAIDSpawnGroupInstance& SelectedInstance = AIDFragment.SpawnGroupInstances[GroupIndex];
		const bool bTimingAllowsSpawn = ProcessSpawnTiming(SelectedInstance.SpawnTimer, Config, DeltaTime);
#if RECALL_DESYNC_LOG
		RECALL_DESYNC_LOG_STR(&SpawnContext.World, AIDSpawnGroupTiming,
			FString::Printf(TEXT("%s Rule: RandomWeight, GroupIndex: %d, bTimingAllowsSpawn: %s, SpawnTimer: %f"),
			*AIDEntity.DebugGetDescription(), GroupIndex,
			*Recall::Desync::Conv_BoolToString(bTimingAllowsSpawn), SelectedInstance.SpawnTimer));
#endif // RECALL_DESYNC_LOG

		if (!bTimingAllowsSpawn || !TrySpawnGroup(GroupIndex, Config))
		{
			return;
		}

		SelectedInstance.bHasSpawned = true;
		AIDFragment.PendingSpawnGroupIndex = INDEX_NONE;
		ScheduleNextSpawnGroup(AIDAsset, AIDFragment, CurrentAIDState, AttributeSet, RandomStream);
		return;
	}

	// Parallel is the default rule: every enabled group owns an independent timer.
	for (int32 GroupIndex = 0; GroupIndex < AIDAsset.SpawnGroups.Num(); GroupIndex++)
	{
		if (!AIDFragment.SpawnGroupInstances.IsValidIndex(GroupIndex))
		{
			continue;
		}

		FRecallAIDSpawnGroupInstance& Instance = AIDFragment.SpawnGroupInstances[GroupIndex];
		const FSpawnGroupConfiguration Config = GetSpawnGroupConfiguration(
			AIDAsset.SpawnGroups[GroupIndex], CurrentAIDState);
		if (!Config.bGroupEnabled || (Config.bBurstMode && Instance.bBurstCompleted))
		{
			continue;
		}

#if RECALL_DESYNC_LOG
		RECALL_DESYNC_LOG_STR(&SpawnContext.World, AIDSpawnGroupTiming,
			FString::Printf(TEXT("%s Rule: Parallel, GroupIndex: %d, SpawnTimer: %f"),
			*AIDEntity.DebugGetDescription(), GroupIndex, Instance.SpawnTimer));
#endif // RECALL_DESYNC_LOG

		if (!ProcessSpawnTiming(Instance.SpawnTimer, Config, DeltaTime))
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

		const FVariableCollectionFloatRange& SpawnInterval =AIDAsset.SpawnGroups[GroupIndex].SpawnIntervalRange;
		Instance.SpawnTimer = RandomStream.FRandRange(
			SpawnInterval.GetMinValue(), SpawnInterval.GetMaxValue());
	}
}
void Recall::AID::Utils::DecrementAIDMobCount(
	FMassEntityManager& EntityManager,
	const FMassEntityHandle& AIDEntity)
{
	// Check if the AI Director entity is still valid
	if (!EntityManager.IsEntityValid(AIDEntity))
	{
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
		UE_LOG(LogTemp, Warning, TEXT("%hs: AI Director entity %s is no longer valid"), 
			__FUNCTION__, *AIDEntity.DebugGetDescription());
#endif // UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
		return;
	}

	const FMassEntityView DirectorView(EntityManager, AIDEntity);
	if (FRecallAIDFragment* AIDFragmentPtr = DirectorView.GetFragmentDataPtr<FRecallAIDFragment>())
	{
		const int32 PreviousCount = AIDFragmentPtr->MobCount;
		
		// Decrement mob count with safety check
		AIDFragmentPtr->MobCount = FMath::Max(0, AIDFragmentPtr->MobCount - 1);
		
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
		// Warn if we're trying to decrement below zero
		if (PreviousCount <= 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("%hs: Attempted to decrement mob count below zero for AI Director %s. Count was: %d"), 
				__FUNCTION__, *AIDEntity.DebugGetDescription(), PreviousCount);
		}
		
		UE_LOG(LogTemp, VeryVerbose, TEXT("%hs: Decremented mob count for AI Director %s. Previous: %d, New: %d"), 
			__FUNCTION__, *AIDEntity.DebugGetDescription(), PreviousCount, AIDFragmentPtr->MobCount);
		#endif
	}
	else
	{
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
		UE_LOG(LogTemp, Warning, TEXT("%hs: AI Director entity %s does not have FRecallAIDFragment"), 
			__FUNCTION__, *AIDEntity.DebugGetDescription());
#endif // UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	}
}
