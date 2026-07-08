// Copyright (C) 2024 Van de Walle Bastien
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "CoreMinimal.h"

#include "RecallAIDSpawnTypes.generated.h"

/**
 * Configuration data for a spawn group managed by the AI Director.
 * Defines what entities to spawn, when to spawn them, and how many.
 */
USTRUCT()
struct RECALLAIDCORE_API FRecallAIDSpawnGroup
{
	GENERATED_BODY()
	
public:
	/**
	 * Name identifier for this spawn group (used by AID states to reference specific groups).
	 */
	UPROPERTY(EditAnywhere)
	FName GroupName;
	
	/**
	 * Entity to spawn.
	 */
	UPROPERTY(EditAnywhere, meta=(AllowedClasses="/Script/MassSpawner.MassEntityConfigAsset"))
	FSoftObjectPath EntityConfigAsset;

	/**
	 * Define how many entities can be spawned by this wave.
	 */
	UPROPERTY(EditAnywhere)
	FInt32Interval EntityCount{ 1, 4 };

	/**
	 * Delay before the first spawn can be initiated.
	 */
	UPROPERTY(EditAnywhere, meta=(Units=Seconds))
	FFloatRange InitialTimer{ 10, 20 };
	
	/**
	 * Spawn interval of this wave.
	 */
	UPROPERTY(EditAnywhere, meta=(Units=Seconds))
	FFloatRange SpawnInterval{ 60, 240 };

	/**
	 * True, if spawned position should be projected on navigation.
	 */
	UPROPERTY(EditAnywhere)
	bool bProjectOntoNavigation = true;
};

/**
 * Entry that links a spawn group to an AID state.
 * Controls whether a specific spawn group is active in a given AI Director state.
 */
USTRUCT()
struct RECALLAIDCORE_API FRecallAIDStateGroupEntry
{
	GENERATED_BODY()
	
	/**
	 * Name of the spawn group to use (must match a GroupName in SpawnGroups array).
	 */
	UPROPERTY(EditAnywhere, meta=(GetOptions="GetSpawnGroupNames"))
	FName SpawnGroupName;
	
	/**
	 * Whether this spawn group is enabled in this state.
	 */
	UPROPERTY(EditAnywhere)
	bool bEnabled = true;
};

/**
 * Named AI Director state that configures spawn behavior.
 * States can enable/disable spawn groups and modify spawn rates/speeds.
 */
USTRUCT()
struct RECALLAIDCORE_API FRecallAIDState
{
	GENERATED_BODY()
	
	/**
	 * Name identifier for this AID state.
	 */
	UPROPERTY(EditAnywhere)
	FName StateName = TEXT("Default");
	
	/**
	 * Spawn groups active in this state.
	 */
	UPROPERTY(EditAnywhere)
	TArray<FRecallAIDStateGroupEntry> SpawnGroupEntries;
	
	/**
	 * Speed modifier for spawn timers (2.0 = twice as fast, 0.5 = half speed).
	 */
	UPROPERTY(EditAnywhere, meta=(ClampMin="0.001", ClampMax="100.0"))
	float SpawnSpeedModifier = 1.0f;
	
	/**
	 * Rate modifier for entity count (2.0 = spawn twice as many entities, 0.5 = half entities).
	 */
	UPROPERTY(EditAnywhere, meta=(ClampMin="0.001", ClampMax="100.0"))
	float SpawnRateModifier = 1.0f;
	
	/**
	 * If true, spawn groups will only spawn once then stop until state changes.
	 */
	UPROPERTY(EditAnywhere)
	bool bBurstMode = false;

	/**
	 * Burst duration in seconds (only used when bBurstMode is true).
	 * 0.0 = spawn only on first frame (instant burst), > 0.0 = spawn for specified duration (spread burst).
	 */
	UPROPERTY(EditAnywhere, meta=(Units=Seconds, ClampMin="0.0", EditCondition="bBurstMode"))
	float BurstDuration = 0.0f;
};
