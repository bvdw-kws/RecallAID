// Copyright (C) 2024 Van de Walle Bastien
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0


#include "RecallAIDProcessors.h"

#include "AID/RecallAIDAsset.h"
#include "Desync/RecallDesyncLog.h"
#include "MassExecutionContext.h"
#include "Simulation/Attribute/RecallAttributeFragments.h"
#include "Simulation/AID/RecallAIDFragments.h"
#include "Simulation/StateTree/RecallStateTreeProcessorGroupTypes.h"
#include "Simulation/Transform/RecallTransformFragments.h"
#include "System/Entity/RecallEntityAsyncSpawnSubsystem.h"
#include "System/Random/RecallRandomNumberSubsystem.h"
#include "Utility/AID/RecallAIDSpawnUtils.h"
#include "Utility/AID/RecallAIDSpawnInternal.h"

//----------------------------------------------------------------------//
// URecallAIDirectorInitializer
//----------------------------------------------------------------------//
URecallAIDirectorInitializer::URecallAIDirectorInitializer()
	: EntityQuery(*this)
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::All);
	ObservedTypes.Add(FRecallAIDFragment::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Add;
}

void URecallAIDirectorInitializer::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& InEntityManager)
{
	Super::InitializeInternal(Owner, InEntityManager);
}

void URecallAIDirectorInitializer::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FRecallAIDFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FRecallAIDConstSharedFragment>();
	EntityQuery.AddSubsystemRequirement<URecallRandomNumberSubsystem>(EMassFragmentAccess::ReadWrite);
}

void URecallAIDirectorInitializer::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk(Context, [](FMassExecutionContext& Context)
	{
		URecallRandomNumberSubsystem& RandomNumberSystem = Context.GetMutableSubsystemChecked<URecallRandomNumberSubsystem>();
		const FRandomStream& RandomStream = RandomNumberSystem.GetRandomStream();
		const auto& AIDConstSharedFragment = Context.GetConstSharedFragment<FRecallAIDConstSharedFragment>();
		if (!AIDConstSharedFragment.AIDAsset)
		{
			return;
		}

		const TObjectPtr<URecallAIDAsset>& AIDAsset = AIDConstSharedFragment.AIDAsset;

		const TArrayView<FRecallAIDFragment> AIDList = Context.GetMutableFragmentView<FRecallAIDFragment>();

		for (int32 EntityIndex = 0; EntityIndex < Context.GetNumEntities(); EntityIndex++)
		{
			FRecallAIDFragment& AIDFragment = AIDList[EntityIndex];
			Recall::AID::Utils::Private::InitializeTimerSet(*AIDAsset, RandomStream, AIDFragment.GlobalTimers);

			// Set default AID state if specified
			if (!AIDAsset->DefaultAIDState.IsNone())
			{
				const bool bStateExists = AIDAsset->AIDStates.ContainsByPredicate([&](const FRecallAIDState& State) {
					return State.StateName == AIDAsset->DefaultAIDState;
				});
				
				if (bStateExists)
				{
					AIDFragment.CurrentAIDStateName = AIDAsset->DefaultAIDState;
				}
			}
		}
	});
}

//----------------------------------------------------------------------//
// URecallAIDSpawnProcessor
//----------------------------------------------------------------------//
URecallAIDSpawnProcessor::URecallAIDSpawnProcessor()
	: SpawnPointEntityQuery(*this)
	, DirectorEntityQuery(*this)
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::All);
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionOrder.ExecuteAfter.Add(Recall::StateTree::ProcessorGroupNames::StateTreeUpdate);
}

/**
 * Manager for caching spawn point data during processor execution.
 */
struct FRecallAIDSpawnCacheManager
{
	TArray<FRecallAIDSpawnPointCache> SpawnPoints;
	int32 SpawnPointCount = 0;

	FORCEINLINE void ResetCache()
	{
		SpawnPoints.Reset();
		SpawnPointCount = 0;
	}
};

void URecallAIDSpawnProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& InEntityManager)
{
	Super::InitializeInternal(Owner, InEntityManager);

	CacheManager = MakeShared<FRecallAIDSpawnCacheManager>();
}

void URecallAIDSpawnProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	FMassTagBitSet SpawnPointRequiredTags;
	SpawnPointRequiredTags.Add(FRecallAIDSpawnPointRelevantTag::StaticStruct());
	
	SpawnPointEntityQuery.AddRequirement<FRecallTransformFragment>(EMassFragmentAccess::ReadOnly);
	SpawnPointEntityQuery.AddRequirement<FRecallAIDSpawnPointFragment>(EMassFragmentAccess::ReadWrite);
	SpawnPointEntityQuery.AddTagRequirements<EMassFragmentPresence::All>(SpawnPointRequiredTags);
	SpawnPointEntityQuery.AddConstSharedRequirement<FRecallAIDSpawnPointConstSharedFragment>();
	
	DirectorEntityQuery.AddRequirement<FRecallAIDFragment>(EMassFragmentAccess::ReadWrite);
	// Paused directors (no running tag) are skipped: timers and spawning are frozen.
	DirectorEntityQuery.AddTagRequirement<FRecallAIDRunningTag>(EMassFragmentPresence::All);
	DirectorEntityQuery.AddRequirement<FRecallAttributeFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	DirectorEntityQuery.AddConstSharedRequirement<FRecallAIDConstSharedFragment>();
	DirectorEntityQuery.AddSubsystemRequirement<URecallRandomNumberSubsystem>(EMassFragmentAccess::ReadWrite);
	DirectorEntityQuery.AddSubsystemRequirement<URecallEntityAsyncSpawnSubsystem>(EMassFragmentAccess::ReadWrite);
}

void URecallAIDSpawnProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	QUICK_SCOPE_CYCLE_COUNTER(Recall_AIDSpawn_Execute);

	check(CacheManager.IsValid());
	CacheManager->ResetCache();

	TArray<FRecallAIDSpawnPointCache>& SpawnPoints = CacheManager->SpawnPoints;
	int32& SpawnPointCount = CacheManager->SpawnPointCount;

	// Phase 1: Cache all relevant spawn points for efficient access
	SpawnPointEntityQuery.ForEachEntityChunk(Context,
		[&SpawnPointCount, &SpawnPoints](FMassExecutionContext& Context)
	{
		const auto& SpawnPointConstSharedFragment = Context.GetConstSharedFragment<FRecallAIDSpawnPointConstSharedFragment>();
		
		const TConstArrayView<FRecallTransformFragment> TransformList = Context.GetFragmentView<FRecallTransformFragment>();
		const TArrayView<FRecallAIDSpawnPointFragment> SpawnPointList = Context.GetMutableFragmentView<FRecallAIDSpawnPointFragment>();

		for (int32 EntityIndex = 0; EntityIndex < Context.GetNumEntities(); EntityIndex++)
		{
			const FRecallTransformFragment& TransformFragment = TransformList[EntityIndex];
			FRecallAIDSpawnPointFragment& SpawnPointFragment = SpawnPointList[EntityIndex];

			if (SpawnPointCount == SpawnPoints.Num())
			{
				SpawnPoints.AddDefaulted();
			}

			FRecallAIDSpawnPointCache& SpawnPoint = SpawnPoints[SpawnPointCount++];
			SpawnPoint.Entity = Context.GetEntity(EntityIndex);
			SpawnPoint.SpawnPointSettings = &SpawnPointConstSharedFragment.SpawnPointSettings;
			SpawnPoint.SpawnPointFragment = &SpawnPointFragment;
			SpawnPoint.Location = TransformFragment.Position;
			SpawnPoint.Rotation = TransformFragment.Rotation;
		}
	});

	// Phase 2: Process AI Directors and spawn entities based on their configuration
	DirectorEntityQuery.ForEachEntityChunk(Context,
		[&SpawnPoints, &EntityManager](FMassExecutionContext& Context)
	{
		const auto& AIDConstSharedFragment = Context.GetConstSharedFragment<FRecallAIDConstSharedFragment>();
		if (!AIDConstSharedFragment.AIDAsset)
		{
			return;
		}
			
		URecallRandomNumberSubsystem& RandomNumberSystem = Context.GetMutableSubsystemChecked<URecallRandomNumberSubsystem>();
		URecallEntityAsyncSpawnSubsystem& EntityAsyncSpawnSystem = Context.GetMutableSubsystemChecked<URecallEntityAsyncSpawnSubsystem>();
		const FRandomStream& RandomStream = RandomNumberSystem.GetRandomStream();			

		const TObjectPtr<URecallAIDAsset>& AIDAsset = AIDConstSharedFragment.AIDAsset;
		const TArrayView<FRecallAIDFragment> AIDList = Context.GetMutableFragmentView<FRecallAIDFragment>();
		const TConstArrayView<FRecallAttributeFragment> AttributeList = Context.GetFragmentView<FRecallAttributeFragment>();

		// Create spawn context once for this chunk
		const FRecallAIDSpawnContext SpawnContext(
			*Context.GetWorld(),
			RandomNumberSystem,
			EntityAsyncSpawnSystem
		);

		for (int32 EntityIndex = 0; EntityIndex < Context.GetNumEntities(); EntityIndex++)
		{
			FRecallAIDFragment& AIDFragment = AIDList[EntityIndex];
			const FMassEntityHandle AIDEntity = Context.GetEntity(EntityIndex);
			const FRecallAttributeFragment* AttributeFragmentPtr = AttributeList.IsValidIndex(EntityIndex)
				? &AttributeList[EntityIndex]
				: nullptr;

			// Drop per-spawner timers of spawners that no longer exist (e.g. destroyed on wave cleanup).
			if (!AIDFragment.SpawnerTimers.IsEmpty())
			{
				for (auto TimerIt = AIDFragment.SpawnerTimers.CreateIterator(); TimerIt; ++TimerIt)
				{
					if (!EntityManager.IsEntityValid(TimerIt.Key()))
					{
						TimerIt.RemoveCurrent();
					}
				}
			}

			// Find current AID state (if any)
			const FRecallAIDState* CurrentAIDState = nullptr;
			if (!AIDFragment.CurrentAIDStateName.IsNone())
			{
				CurrentAIDState = AIDAsset->AIDStates.FindByPredicate([&](const FRecallAIDState& State) {
					return State.StateName == AIDFragment.CurrentAIDStateName;
				});
			}

#if RECALL_DESYNC_LOG
			RECALL_DESYNC_LOG_STR(Context.GetWorld(), AIDSpawnPreProcess,
				FString::Printf(TEXT("%s CurrentAIDStateName: %s, MobCount: %d, SpawnPointCount: %d"),
				*AIDEntity.DebugGetDescription(), *AIDFragment.CurrentAIDStateName.ToString(),
				AIDFragment.MobCount, SpawnPoints.Num()));
#endif // RECALL_DESYNC_LOG
			
			// Process all spawn groups for this AI Director entity
			Recall::AID::Utils::ProcessAIDSpawnGroups(
				SpawnContext,
				*AIDAsset,
				AIDEntity,
				AIDFragment,
				CurrentAIDState,
				SpawnPoints,
				RandomStream,
				AttributeFragmentPtr ? &AttributeFragmentPtr->AttributeSet : nullptr,
				Context.GetDeltaTimeSeconds()
			);

#if RECALL_DESYNC_LOG
			const FRecallAIDSpawnTimerSet& GlobalTimers = AIDFragment.GlobalTimers;
			const float PendingSpawnTimer = GlobalTimers.GroupInstances.IsValidIndex(GlobalTimers.PendingGroupIndex)
				? GlobalTimers.GroupInstances[GlobalTimers.PendingGroupIndex].SpawnTimer
				: 0.0f;
			RECALL_DESYNC_LOG_STR(Context.GetWorld(), AIDSpawnPostProcess,
				FString::Printf(TEXT("%s MobCount: %d, PendingSpawnGroupIndex: %d, SpawnTimer: %f"),
				*AIDEntity.DebugGetDescription(), AIDFragment.MobCount,
				GlobalTimers.PendingGroupIndex, PendingSpawnTimer));

			for (int32 GroupIndex = 0; GroupIndex < GlobalTimers.GroupInstances.Num(); GroupIndex++)
			{
				const FRecallAIDSpawnGroupInstance& Instance = GlobalTimers.GroupInstances[GroupIndex];
				RECALL_DESYNC_LOG_STR(Context.GetWorld(), AIDSpawnGroupInstance,
					FString::Printf(TEXT("%s GroupIndex: %d, SpawnTimer: %f, bHasSpawned: %s, bBurstCompleted: %s, BurstTimer: %f"),
					*AIDEntity.DebugGetDescription(), GroupIndex,
					Instance.SpawnTimer, *Recall::Desync::Conv_BoolToString(Instance.bHasSpawned),
					*Recall::Desync::Conv_BoolToString(Instance.bBurstCompleted), Instance.BurstTimer));
			}
#endif // RECALL_DESYNC_LOG
		}
	});
}

//----------------------------------------------------------------------//
// URecallAIDStateChangeProcessor
//----------------------------------------------------------------------//
URecallAIDStateChangeProcessor::URecallAIDStateChangeProcessor()
	: EntityQuery(*this)
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::All);
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
}

void URecallAIDStateChangeProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FRecallAIDFragment>(EMassFragmentAccess::ReadWrite);
}

void URecallAIDStateChangeProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	// Track state changes and reset burst flags when state changes
	EntityQuery.ForEachEntityChunk(Context, [](FMassExecutionContext& Context)
	{
		const TArrayView<FRecallAIDFragment> AIDList = Context.GetMutableFragmentView<FRecallAIDFragment>();
		
		for (int32 EntityIndex = 0; EntityIndex < Context.GetNumEntities(); EntityIndex++)
		{
			FRecallAIDFragment& AIDFragment = AIDList[EntityIndex];
			
			// Store previous state name in a thread-safe manner
			// TODO: This should be stored as a fragment on the entity rather than a static map
			static FCriticalSection MapCriticalSection;
			static TMap<FMassEntityHandle, FName> PreviousStateMap;
			
			FScopeLock Lock(&MapCriticalSection);
			const FMassEntityHandle Entity = Context.GetEntity(EntityIndex);
			
			FName* PreviousStateName = PreviousStateMap.Find(Entity);
			if (!PreviousStateName)
			{
				// First time seeing this entity
				PreviousStateMap.Add(Entity, AIDFragment.CurrentAIDStateName);

#if RECALL_DESYNC_LOG
				RECALL_DESYNC_LOG_STR(Context.GetWorld(), AIDStateChangeFirstSeen,
					FString::Printf(TEXT("%s CurrentAIDStateName: %s"),
					*Entity.DebugGetDescription(), *AIDFragment.CurrentAIDStateName.ToString()));
#endif // RECALL_DESYNC_LOG
			}
			else if (*PreviousStateName != AIDFragment.CurrentAIDStateName)
			{
#if RECALL_DESYNC_LOG
				RECALL_DESYNC_LOG_STR(Context.GetWorld(), AIDStateChanged,
					FString::Printf(TEXT("%s PreviousStateName: %s, NewStateName: %s"),
					*Entity.DebugGetDescription(), *PreviousStateName->ToString(), *AIDFragment.CurrentAIDStateName.ToString()));
#endif // RECALL_DESYNC_LOG

				// State has changed - reset burst tracking and let Random Weight choose again.
				const auto ResetTimerSet = [](FRecallAIDSpawnTimerSet& Timers)
				{
					for (FRecallAIDSpawnGroupInstance& Instance : Timers.GroupInstances)
					{
						Instance.bBurstCompleted = false;
						Instance.BurstTimer = 0.0f;
					}
					Timers.PendingGroupIndex = INDEX_NONE;
				};
				ResetTimerSet(AIDFragment.GlobalTimers);
				for (TPair<FMassEntityHandle, FRecallAIDSpawnTimerSet>& SpawnerTimerPair : AIDFragment.SpawnerTimers)
				{
					ResetTimerSet(SpawnerTimerPair.Value);
				}
				
				// Update the stored state
				*PreviousStateName = AIDFragment.CurrentAIDStateName;
			}
		}
	});
}

//----------------------------------------------------------------------//
// URecallAIDMobDestroyObserver
//----------------------------------------------------------------------//
URecallAIDMobDestroyObserver::URecallAIDMobDestroyObserver()
	: EntityQuery(*this)
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::All);
	ObservedTypes.Add(FRecallAIDMobTrackingFragment::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Remove;
}

void URecallAIDMobDestroyObserver::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& InEntityManager)
{
	Super::InitializeInternal(Owner, InEntityManager);
}

void URecallAIDMobDestroyObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FRecallAIDMobTrackingFragment>(EMassFragmentAccess::ReadOnly);
}

void URecallAIDMobDestroyObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk(Context, [&EntityManager](FMassExecutionContext& Context)
	{
		const TConstArrayView<FRecallAIDMobTrackingFragment> TrackingFragments = Context.GetFragmentView<FRecallAIDMobTrackingFragment>();
		
		for (int32 EntityIndex = 0; EntityIndex < Context.GetNumEntities(); EntityIndex++)
		{
			const FRecallAIDMobTrackingFragment& TrackingFragment = TrackingFragments[EntityIndex];
			
			// Delegate mob count decrement to spawn utils
			Recall::AID::Utils::DecrementAIDMobCount(EntityManager, TrackingFragment.AIDEntity);
		}
	});
}
