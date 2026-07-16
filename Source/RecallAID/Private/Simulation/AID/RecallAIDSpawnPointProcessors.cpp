// Copyright (C) 2024 Van de Walle Bastien
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0


#include "RecallAIDSpawnPointProcessors.h"

#include "Algo/AnyOf.h"
#include "Desync/RecallDesyncLog.h"
#include "MassExecutionContext.h"
#include "Simulation/AID/RecallAIDFragments.h"
#include "Simulation/Controller/RecallControllerFragments.h"
#include "Simulation/Transform/RecallTransformFragments.h"

//----------------------------------------------------------------------//
// URecallAIDSpawnPointRelevancyProcessor
//----------------------------------------------------------------------//
URecallAIDSpawnPointRelevancyProcessor::URecallAIDSpawnPointRelevancyProcessor()
	: ControllerEntityQuery(*this)
	, SpawnPointEntityQuery(*this)
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::All);
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
}

/**
 * Cache manager for spawn point relevancy calculations.
 * Stores controller positions to avoid repeated queries.
 */
struct FRecallAIDSpawnPointRelevancyCacheManager
{
	TArray<FVector> ControllerLocations;
	
	FORCEINLINE void ResetCache()
	{
		ControllerLocations.Reset();
	}
};

void URecallAIDSpawnPointRelevancyProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& InEntityManager)
{
	Super::InitializeInternal(Owner, InEntityManager);

	CacheManager = MakeShared<FRecallAIDSpawnPointRelevancyCacheManager>();
}

void URecallAIDSpawnPointRelevancyProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ControllerEntityQuery.AddRequirement<FRecallTransformFragment>(EMassFragmentAccess::ReadOnly);
	ControllerEntityQuery.AddRequirement<FRecallControllerFragment>(EMassFragmentAccess::ReadOnly);
	
	SpawnPointEntityQuery.AddRequirement<FRecallTransformFragment>(EMassFragmentAccess::ReadOnly);
	SpawnPointEntityQuery.AddRequirement<FRecallAIDSpawnPointFragment>(EMassFragmentAccess::ReadOnly);
	SpawnPointEntityQuery.AddConstSharedRequirement<FRecallAIDSpawnPointConstSharedFragment>();
}

void URecallAIDSpawnPointRelevancyProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	QUICK_SCOPE_CYCLE_COUNTER(Recall_AIDSpawnRelevancy_Execute);

	check(CacheManager.IsValid());
	CacheManager->ResetCache();

	TArray<FVector>& ControllerLocations = CacheManager->ControllerLocations;
	
	// Phase 1: Collect all controller positions
	ControllerEntityQuery.ForEachEntityChunk(Context,
		[&ControllerLocations](FMassExecutionContext& Context)
	{
		const TConstArrayView<FRecallTransformFragment> TransformList = Context.GetFragmentView<FRecallTransformFragment>();
		
		for (int32 EntityIndex = 0; EntityIndex < Context.GetNumEntities(); EntityIndex++)
		{
			const FRecallTransformFragment& TransformFragment = TransformList[EntityIndex];

			ControllerLocations.Add(TransformFragment.Position);
		}
	});
	
	// Phase 2: Update spawn point relevancy based on controller proximity
	SpawnPointEntityQuery.ForEachEntityChunk(Context, [&ControllerLocations](FMassExecutionContext& Context)
	{
		const auto& SpawnPointConstSharedFragment = Context.GetConstSharedFragment<FRecallAIDSpawnPointConstSharedFragment>();
		const FRecallAIDSpawnPointSettings& Settings = SpawnPointConstSharedFragment.SpawnPointSettings;
		const float RelevancySqrRange = Settings.RelevancyRange * Settings.RelevancyRange;
		
		const TConstArrayView<FRecallTransformFragment> TransformList = Context.GetFragmentView<FRecallTransformFragment>();

		const bool bWasRelevant = Context.DoesArchetypeHaveTag<FRecallAIDSpawnPointRelevantTag>();
		
		for (int32 EntityIndex = 0; EntityIndex < Context.GetNumEntities(); EntityIndex++)
		{
			const FRecallTransformFragment& TransformFragment = TransformList[EntityIndex];
			const FVector& SpawnPointLocation = TransformFragment.Position;

			const bool bIsRelevant = Algo::AnyOf(ControllerLocations,
				[RelevancySqrRange, &SpawnPointLocation](const FVector& PlayerLocation)
			{
				return FVector::DistSquared2D(PlayerLocation, SpawnPointLocation) < RelevancySqrRange;
			});

			if (bIsRelevant == bWasRelevant)
			{
				continue;
			}

			const FMassEntityHandle Entity = Context.GetEntity(EntityIndex);

			if (bIsRelevant)
			{
				Context.Defer().AddTag<FRecallAIDSpawnPointRelevantTag>(Entity);

#if RECALL_DESYNC_LOG
				RECALL_DESYNC_LOG_STR(Context.GetWorld(), AIDSpawnPointRelevancy_AddTag, FString::Printf(
					TEXT("Entity: %s, Tag: FRecallAIDSpawnPointRelevantTag"), *Entity.DebugGetDescription()));
#endif // RECALL_DESYNC_LOG
			}
			else
			{
				Context.Defer().RemoveTag<FRecallAIDSpawnPointRelevantTag>(Entity);

#if RECALL_DESYNC_LOG
				RECALL_DESYNC_LOG_STR(Context.GetWorld(), AIDSpawnPointRelevancy_RemoveTag, FString::Printf(
					TEXT("Entity: %s, Tag: FRecallAIDSpawnPointRelevantTag"), *Entity.DebugGetDescription()));
#endif // RECALL_DESYNC_LOG
			}
		}
	});
}
