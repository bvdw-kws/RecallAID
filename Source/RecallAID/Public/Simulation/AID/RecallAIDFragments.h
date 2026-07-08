// Copyright (C) 2024 Van de Walle Bastien
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "MassEntityElementTypes.h"
#include "MassEntityHandle.h"
#include "AID/RecallAIDSpawnPointSettings.h"

#include "RecallAIDFragments.generated.h"

/**
 * Runtime instance data for a spawn group within an AI Director.
 * Tracks spawn timer and burst completion state.
 */
USTRUCT()
struct RECALLAID_API FRecallAIDSpawnGroupInstance
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleAnywhere)
	float SpawnTimer = 0.0f;
	
	/** True if this group has already spawned in burst mode */
	UPROPERTY(VisibleAnywhere)
	bool bBurstCompleted = false;
	
	/** Timer tracking how long burst mode has been active (used for BurstDuration) */
	UPROPERTY(VisibleAnywhere)
	float BurstTimer = 0.0f;
};

/**
 * Main fragment for AI Director entities.
 * Contains runtime state for managing spawn groups and mob counts.
 */
USTRUCT()
struct RECALLAID_API FRecallAIDFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere)
	TArray<FRecallAIDSpawnGroupInstance> SpawnGroupInstances;
	
	UPROPERTY(VisibleAnywhere)
	int32 MobCount = 0;
	
	/** Current AID state name (empty = default state using all spawn groups) */
	UPROPERTY(VisibleAnywhere)
	FName CurrentAIDStateName;
};

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
