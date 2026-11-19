// Copyright (C) 2024 Van de Walle Bastien
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "Mass/EntityElementTypes.h"
#include "Mass/EntityHandle.h"
#include "Mass/ExternalSubsystemTraits.h"
#include "AID/RecallAIDSpawnPointSettings.h"

#include "RecallAIDFragments.generated.h"

/**
 * Runtime instance data for a spawn group within an AI Director.
 * Tracks the group's timer and whether it has spawned or completed its current burst.
 */
USTRUCT()
struct RECALLAID_API FRecallAIDSpawnGroupInstance
{
	GENERATED_BODY()

	/** Time remaining until this group can spawn. */
	UPROPERTY(VisibleAnywhere)
	float SpawnTimer = 0.0f;

	/** True after this group has spawned at least once. */
	UPROPERTY(VisibleAnywhere)
	bool bHasSpawned = false;
	
	/** True if this group has already spawned in burst mode */
	UPROPERTY(VisibleAnywhere)
	bool bBurstCompleted = false;
	
	/** Timer tracking how long burst mode has been active (used for BurstDuration) */
	UPROPERTY(VisibleAnywhere)
	float BurstTimer = 0.0f;
};

/**
 * A complete set of spawn timers: one instance per spawn group plus the Random Weight selection.
 * The director owns one set for the Global scope; with the PerSpawner scope it owns one set per spawner.
 */
USTRUCT()
struct RECALLAID_API FRecallAIDSpawnTimerSet
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere)
	TArray<FRecallAIDSpawnGroupInstance> GroupInstances;

	/** Group currently selected by the Random Weight rule while its timer counts down. */
	UPROPERTY(VisibleAnywhere)
	int32 PendingGroupIndex = INDEX_NONE;

	bool IsSpawnTimerInitialized() const
	{
		return PendingGroupIndex != INDEX_NONE;
	}
};

/**
 * Main fragment for AI Director entities.
 * Contains runtime state for managing spawn groups and mob counts.
 */
USTRUCT()
struct RECALLAID_API FRecallAIDFragment : public FMassFragment
{
	GENERATED_BODY()

	/** Timers used by states with the Global spawn scope. */
	UPROPERTY(VisibleAnywhere)
	FRecallAIDSpawnTimerSet GlobalTimers;

	/**
	 * Timers used by states with the PerSpawner spawn scope, keyed by spawner entity.
	 * Entries are created lazily when a spawner first becomes available and pruned when the spawner is destroyed.
	 * Iteration (prune, state-change reset, debug) is order-independent, so it does not affect determinism.
	 */
	UPROPERTY(VisibleAnywhere)
	TMap<FMassEntityHandle, FRecallAIDSpawnTimerSet> SpawnerTimers;

	UPROPERTY(VisibleAnywhere)
	int32 MobCount = 0;
	
	/** Current AID state name (empty = default state using all spawn groups) */
	UPROPERTY(VisibleAnywhere)
	FName CurrentAIDStateName;

	/**
	 * State name seen by URecallAIDStateChangeProcessor on its last run, used to detect state changes.
	 * Kept in the fragment so it is saved and restored with the simulation snapshots (rollback safe).
	 */
	UPROPERTY(VisibleAnywhere)
	FName PreviousAIDStateName = NAME_None;
};

template <>
struct TMassFragmentTraits<FRecallAIDFragment> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

/**
 * Tag marking an AI Director that is running. Only tagged directors update their spawn timers and spawn.
 * Added on start when the trait's bAutoStart is true, then toggled with Recall::AID::Signals::Start / Stop.
 */
USTRUCT() struct RECALLAID_API FRecallAIDRunningTag : public FMassTag { GENERATED_BODY() };

/**
 * Shared fragment containing configuration data for AI Directors.
 * References the AID asset with spawn group definitions.
 */
USTRUCT()
struct RECALLAID_API FRecallAIDConstSharedFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<class URecallAIDAsset> AIDAsset;
};

/**
 * Fragment that links spawned entities back to their AI Director for mob count tracking.
 * Added to entities spawned by the AI Director to enable proper cleanup when they are destroyed.
 */
USTRUCT()
struct RECALLAID_API FRecallAIDMobTrackingFragment : public FMassFragment
{
	GENERATED_BODY()
	
	/** The AI Director entity that spawned this mob */
	UPROPERTY(VisibleAnywhere)
	FMassEntityHandle AIDEntity;
};

/**
 * Tag to mark spawn points that are currently relevant and can be used to spawn entities.
 */
USTRUCT() struct RECALLAID_API FRecallAIDSpawnPointRelevantTag : public FMassTag { GENERATED_BODY() };

/**
 * Fragment for spawn point entities containing runtime data.
 * Tracks spawn point usage for cooldown management.
 */
USTRUCT()
struct RECALLAID_API FRecallAIDSpawnPointFragment : public FMassFragment
{
	GENERATED_BODY()

	/** Time (in seconds since game start) when this spawn point was last used for spawning */
	UPROPERTY(VisibleAnywhere)
	float LastSpawnTime = -1.0f;
};

/**
 * Shared fragment containing configuration for spawn point entities.
 * Defines relevancy range and spawn area settings.
 */
USTRUCT()
struct RECALLAID_API FRecallAIDSpawnPointConstSharedFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleAnywhere)
	FRecallAIDSpawnPointSettings SpawnPointSettings;
};
