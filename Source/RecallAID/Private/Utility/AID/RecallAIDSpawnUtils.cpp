// Copyright (C) 2024 Van de Walle Bastien
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0


#include "Utility/AID/RecallAIDSpawnUtils.h"

#include "AID/RecallAIDAsset.h"
#include "AID/RecallAIDSpawnPointSettings.h"
#include "Desync/RecallDesyncLog.h"
#include "Engine/World.h"
#include "MassEntityManager.h"
#include "MassEntityView.h"
#include "RecallAIDSpawnInternal.h"
#include "Utility/Simulation/RecallSimulationUtils.h"

using namespace Recall::AID::Utils::Private;

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
		if (SpawnPointCache.SpawnPointFragment == nullptr)
		{
			continue;
		}
		
		const float TimeSinceLastSpawn = CurrentTime - SpawnPointCache.SpawnPointFragment->LastSpawnTime;
		if (SpawnPointCache.SpawnPointFragment->LastSpawnTime < 0.0f || TimeSinceLastSpawn >= AIDAsset.SpawnPointCooldown)
		{
			OutAvailableSpawnPoints.Add(SpawnPointCache);
		}
	}
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

	const FSpawnRuleContext Rule{ AIDAsset, CurrentAIDState, AttributeSet, RandomStream, DeltaTime };

	// Rolls the entity count, enforces the mob limit and spawns.
	// FixedSpawnPoint == nullptr: the spawner is picked at random among the available ones (Global scope).
	const auto TrySpawnGroup = [&](
		int32 GroupIndex,
		const FSpawnGroupConfiguration& Config,
		const FRecallAIDSpawnPointCache* FixedSpawnPoint) -> bool
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

		const int32 SpawnedCount = FixedSpawnPoint
			? SpawnEnemyGroupAt(SpawnContext, AIDEntity, SpawnGroup, *FixedSpawnPoint, EntityCount, CurrentTime)
			: SpawnEnemyGroup(SpawnContext, AIDEntity, SpawnGroup, AvailableSpawnPoints, EntityCount, CurrentTime);
		AIDFragment.MobCount += SpawnedCount;

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
		UE_LOG(LogTemp, VeryVerbose, TEXT("%hs: Spawned %d entities for AI Director. New mob count: %d"),
			__FUNCTION__, SpawnedCount, AIDFragment.MobCount);
#endif

		return SpawnedCount > 0;
	};

	// Runs the state's rule over one timer set. Parallel is the default rule.
	const bool bRandomWeightRule = CurrentAIDState
		&& CurrentAIDState->SpawnRule == ERecallAIDSpawnRule::RandomWeight;
	const auto RunRule = [&](FRecallAIDSpawnTimerSet& Timers, const FRecallAIDSpawnPointCache* FixedSpawnPoint)
	{
		UpdateBurstMode(Rule, Timers);

		const auto TrySpawn = [&](int32 GroupIndex, const FSpawnGroupConfiguration& Config)
		{
			return TrySpawnGroup(GroupIndex, Config, FixedSpawnPoint);
		};

		if (bRandomWeightRule)
		{
			ProcessRandomWeightRule(Rule, Timers, &SpawnContext.World, AIDEntity, TrySpawn);
		}
		else
		{
			ProcessParallelRule(Rule, Timers, &SpawnContext.World, AIDEntity, TrySpawn);
		}
	};

	if (!CurrentAIDState || CurrentAIDState->SpawnScope == ERecallAIDSpawnScope::Global)
	{
		RunRule(AIDFragment.GlobalTimers, nullptr);
		return;
	}

	// PerSpawner: every available spawner runs the rule on its own timers and spawns from itself.
	// Spawners on cooldown or out of range are not in the list, so their timers are frozen.
	// The mob limit is shared, so earlier spawners in the cache get priority when it is nearly full.
	for (const FRecallAIDSpawnPointCache& Spawner : AvailableSpawnPoints)
	{
		FRecallAIDSpawnTimerSet* Timers = AIDFragment.SpawnerTimers.Find(Spawner.Entity);
		if (!Timers)
		{
			Timers = &AIDFragment.SpawnerTimers.Add(Spawner.Entity);
			InitializeTimerSet(AIDAsset, RandomStream, *Timers);
		}

#if RECALL_DESYNC_LOG
		RECALL_DESYNC_LOG_STR(&SpawnContext.World, AIDProcessSpawner,
			FString::Printf(TEXT("%s Spawner: %s, SpawnerTimerSetCount: %d"),
			*AIDEntity.DebugGetDescription(), *Spawner.Entity.DebugGetDescription(), AIDFragment.SpawnerTimers.Num()));
#endif // RECALL_DESYNC_LOG

		RunRule(*Timers, &Spawner);
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
