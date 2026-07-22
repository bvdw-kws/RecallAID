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
#include "Desync/RecallDesyncLog.h"
#include "Entity/RecallAIDSpawnCommand.h"
#include "NavigationSystem.h"
#include "Simulation/AID/RecallAIDFragments.h"
#include "System/Entity/RecallEntityAsyncSpawnSubsystem.h"
#include "System/Entity/RecallEntityAsyncSpawnTypes.h"
#include "System/Random/RecallRandomNumberSubsystem.h"
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
	float& Timer,
	float CurrentTime)
{
	// TODO: Filter spawn points based on gameplay tags to support different spawn zones
	
	if (SpawnPoints.Num() == 0 || EntityCount == 0)
	{
		return 0;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(&SpawnContext.World);
	const FRandomStream& RandomStream = SpawnContext.RandomNumberSystem.GetRandomStream();
	
	// Set timer for next spawn
	Timer = RandomStream.FRandRange(
		SpawnEnemyGroup.SpawnInterval.GetLowerBoundValue(), SpawnEnemyGroup.SpawnInterval.GetUpperBoundValue());

	// Select spawn point
	const int32 SpawnPointIndex = RandomStream.RandRange(0, SpawnPoints.Num() - 1);
	const FRecallAIDSpawnPointCache& SpawnPoint = SpawnPoints[SpawnPointIndex];

#if RECALL_DESYNC_LOG
	RECALL_DESYNC_LOG_STR(&SpawnContext.World, AIDSpawnEnemyGroup,
		FString::Printf(TEXT("%s EntityCount: %d, Timer: %f, SpawnPointIndex: %d, SpawnPointCount: %d, SpawnPointLocation: %s"),
		*AIDEntity.DebugGetDescription(), EntityCount, Timer, SpawnPointIndex, SpawnPoints.Num(), *SpawnPoint.Location.ToString()));
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
			[&](const FRecallAIDStateGroupEntry& Entry) {
				return Entry.SpawnGroupName == SpawnGroup.GroupName;
			});
		
		if (GroupEntry)
		{
			Config.bGroupEnabled = GroupEntry->bEnabled;
		}
		else
		{
			// Group not mentioned in state = disabled
			Config.bGroupEnabled = false;
		}
		
		Config.SpawnSpeedModifier = CurrentAIDState->SpawnSpeedModifier;
		Config.SpawnRateModifier = CurrentAIDState->SpawnRateModifier;
		Config.bBurstMode = CurrentAIDState->bBurstMode;
		Config.BurstDuration = CurrentAIDState->BurstDuration;
	}
	// else: No state = default behavior (all groups enabled)
	
	return Config;
}

/**
 * Handle burst mode logic and determine if spawning should continue.
 * Returns true if spawning should proceed, false if it should be skipped.
 */
static bool ProcessBurstMode(
	FRecallAIDSpawnGroupInstance& Instance,
	const FSpawnGroupConfiguration& Config,
	float DeltaTime)
{
	if (!Config.bBurstMode)
	{
		return true; // Not in burst mode, continue normally
	}
	
	// Update burst timer
	Instance.BurstTimer += DeltaTime;
	
	// Check if burst duration has elapsed
	if (Config.BurstDuration > 0.0f && Instance.BurstTimer >= Config.BurstDuration)
	{
		// Burst duration elapsed, mark as completed
		Instance.bBurstCompleted = true;
	}
	else if (FMath::IsNearlyZero(Config.BurstDuration) && Instance.bBurstCompleted)
	{
		// Instant burst (0.0 duration) already completed
		return false;
	}
	
	// Skip if burst completed
	return !Instance.bBurstCompleted;
}

/**
 * Process spawn timing and determine if it's time to spawn entities.
 * Returns true if spawning should occur this frame.
 */
static bool ProcessSpawnTiming(
	FRecallAIDSpawnGroupInstance& Instance,
	const FSpawnGroupConfiguration& Config,
	float DeltaTime)
{
	if (Instance.SpawnTimer > 0.0f)
	{
		// Apply speed modifier to timer countdown
		const float TimerDelta = DeltaTime * Config.SpawnSpeedModifier;
		Instance.SpawnTimer = FMath::Max(0.0f, Instance.SpawnTimer - TimerDelta);
	}
	
	return FMath::IsNearlyZero(Instance.SpawnTimer);
}

void Recall::AID::Utils::ProcessAIDSpawnGroups(
	const FRecallAIDSpawnContext& SpawnContext,
	const URecallAIDAsset& AIDAsset,
	const FMassEntityHandle& AIDEntity,
	FRecallAIDFragment& AIDFragment,
	const FRecallAIDState* CurrentAIDState,
	const TArray<FRecallAIDSpawnPointCache>& SpawnPoints,
	const FRandomStream& RandomStream,
	float DeltaTime)
{
	// Get current time once for all spawn groups
	const float CurrentTime = static_cast<float>(Recall::Simulation::Utils::GetTimeSeconds(&SpawnContext.World));
	
	// Filter available spawn points that are not on cooldown
	TArray<FRecallAIDSpawnPointCache> AvailableSpawnPoints;
	FilterAvailableSpawnPoints(SpawnPoints, AIDAsset, CurrentTime, AvailableSpawnPoints);

#if RECALL_DESYNC_LOG
	RECALL_DESYNC_LOG_STR(&SpawnContext.World, AIDProcessSpawnGroups,
		FString::Printf(TEXT("%s CurrentTime: %f, SpawnPointCount: %d, AvailableSpawnPointCount: %d, CurrentAIDStateName: %s"),
		*AIDEntity.DebugGetDescription(), CurrentTime, SpawnPoints.Num(), AvailableSpawnPoints.Num(),
		CurrentAIDState ? *CurrentAIDState->StateName.ToString() : TEXT("None")));
#endif // RECALL_DESYNC_LOG

	// Skip spawning if no spawn points are available due to cooldown
	if (AvailableSpawnPoints.Num() == 0)
	{
		return;
	}

	for (int32 GroupIndex = 0; GroupIndex < AIDFragment.SpawnGroupInstances.Num(); GroupIndex++)
	{
		if (!ensure(AIDAsset.SpawnGroups.IsValidIndex(GroupIndex)))
		{
			continue;
		}
		
		const FRecallAIDSpawnGroup& SpawnGroup = AIDAsset.SpawnGroups[GroupIndex];
		FRecallAIDSpawnGroupInstance& Instance = AIDFragment.SpawnGroupInstances[GroupIndex];
		
		// Get spawn configuration for this group
		const FSpawnGroupConfiguration Config = GetSpawnGroupConfiguration(SpawnGroup, CurrentAIDState);

#if RECALL_DESYNC_LOG
		RECALL_DESYNC_LOG_STR(&SpawnContext.World, AIDSpawnGroupConfig,
			FString::Printf(TEXT("%s GroupIndex: %d, bGroupEnabled: %s, SpawnSpeedModifier: %f, SpawnRateModifier: %f, bBurstMode: %s, BurstDuration: %f"),
			*AIDEntity.DebugGetDescription(), GroupIndex, *Recall::Desync::Conv_BoolToString(Config.bGroupEnabled),
			Config.SpawnSpeedModifier, Config.SpawnRateModifier, *Recall::Desync::Conv_BoolToString(Config.bBurstMode), Config.BurstDuration));
#endif // RECALL_DESYNC_LOG
		
		if (!Config.bGroupEnabled)
		{
			continue;
		}
		
		// Process burst mode logic
		const bool bBurstModeAllowsSpawn = ProcessBurstMode(Instance, Config, DeltaTime);

#if RECALL_DESYNC_LOG
		RECALL_DESYNC_LOG_STR(&SpawnContext.World, AIDSpawnGroupBurst,
			FString::Printf(TEXT("%s GroupIndex: %d, bBurstModeAllowsSpawn: %s, BurstTimer: %f, bBurstCompleted: %s"),
			*AIDEntity.DebugGetDescription(), GroupIndex, *Recall::Desync::Conv_BoolToString(bBurstModeAllowsSpawn),
			Instance.BurstTimer, *Recall::Desync::Conv_BoolToString(Instance.bBurstCompleted)));
#endif // RECALL_DESYNC_LOG

		if (!bBurstModeAllowsSpawn)
		{
			continue;
		}
		
		// Process spawn timing
		const bool bTimingAllowsSpawn = ProcessSpawnTiming(Instance, Config, DeltaTime);

#if RECALL_DESYNC_LOG
		RECALL_DESYNC_LOG_STR(&SpawnContext.World, AIDSpawnGroupTiming,
			FString::Printf(TEXT("%s GroupIndex: %d, bTimingAllowsSpawn: %s, SpawnTimer: %f"),
			*AIDEntity.DebugGetDescription(), GroupIndex, *Recall::Desync::Conv_BoolToString(bTimingAllowsSpawn), Instance.SpawnTimer));
#endif // RECALL_DESYNC_LOG

		if (!bTimingAllowsSpawn)
		{
			continue;
		}
		
		// Time to spawn entities
		// Keep mobs below the spawn limit.
		// TODO: Implement entity despawning when mob limit is reached
		int32 BaseEntityCount = RandomStream.RandRange(SpawnGroup.EntityCount.Min, SpawnGroup.EntityCount.Max);
		
		// Apply rate modifier to entity count
		int32 EntityCount = FMath::RoundToInt(BaseEntityCount * Config.SpawnRateModifier);
		EntityCount = FMath::Max(1, EntityCount); // Ensure at least 1 entity

#if RECALL_DESYNC_LOG
		RECALL_DESYNC_LOG_STR(&SpawnContext.World, AIDSpawnGroupEntityCount,
			FString::Printf(TEXT("%s GroupIndex: %d, BaseEntityCount: %d, EntityCount: %d, MobCount: %d, MobLimit: %d"),
			*AIDEntity.DebugGetDescription(), GroupIndex, BaseEntityCount, EntityCount, AIDFragment.MobCount, AIDAsset.MobLimit));
#endif // RECALL_DESYNC_LOG

		if (AIDFragment.MobCount + EntityCount > AIDAsset.MobLimit)
		{
			continue;
		}
		
		const int32 SpawnedCount = SpawnEnemyGroup(SpawnContext,
			AIDEntity, SpawnGroup, AvailableSpawnPoints, EntityCount, Instance.SpawnTimer, CurrentTime);
		AIDFragment.MobCount += SpawnedCount;
		
		#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
		UE_LOG(LogTemp, VeryVerbose, TEXT("%hs: Spawned %d entities for AI Director. New mob count: %d"), 
			__FUNCTION__, SpawnedCount, AIDFragment.MobCount);
		#endif
		
		// Mark burst as completed if in instant burst mode (0.0 duration)
		if (Config.bBurstMode && FMath::IsNearlyZero(Config.BurstDuration))
		{
			Instance.bBurstCompleted = true;
		}
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
