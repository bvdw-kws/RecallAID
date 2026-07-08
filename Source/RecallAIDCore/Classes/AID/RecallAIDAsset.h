// Copyright (C) 2024 Van de Walle Bastien
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "Engine/DataAsset.h"
#include "RecallAIDSpawnTypes.h"

#include "RecallAIDAsset.generated.h"

/**
 * MS AI Director data asset.
 */
UCLASS(DisplayName="Recall AI Director Asset")
class RECALLAIDCORE_API URecallAIDAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * Waves that can be spawned by the AI director.
	 */
	UPROPERTY(EditAnywhere, Category="AI Director", meta=(TitleProperty="{GroupName} - Count:{EntityCount} Timer:{InitialTimer}s Interval:{SpawnInterval}s"))
	TArray<FRecallAIDSpawnGroup> SpawnGroups;
	
	/**
	 * Named AID states that configure which spawn groups are active and their modifiers.
	 */
	UPROPERTY(EditAnywhere, Category="AI Director", DisplayName="AID States", meta=(TitleProperty="{StateName} - Speed:{SpawnSpeedModifier}x Rate:{SpawnRateModifier}x Groups:{SpawnGroupEntries}"))
	TArray<FRecallAIDState> AIDStates;

	/**
	 * Default AID state name to use when initializing AI directors.
	 * If empty or not found, will default to all spawn groups being active.
	 */
	UPROPERTY(EditAnywhere, Category="AI Director", meta=(GetOptions="GetAIDStateNames"))
	FName DefaultAIDState = TEXT("Default");

	/**
	 * How many mobs are allowed simultaneously at most before they start to be despawned?
	 */
	UPROPERTY(EditAnywhere, Category="AI Director", meta=(ClampMin=1, ClampMax=10000))
	int32 MobLimit = 1000;

	/**
	 * Cooldown time in seconds before the same spawn point can be selected again.
	 * Prevents mobs from stacking by spawning too many on the same spawn point in a short time.
	 */
	UPROPERTY(EditAnywhere, Category="AI Director", meta=(Units=Seconds, ClampMin=0.0f, ClampMax=60.0f))
	float SpawnPointCooldown = 0.5f;
	
	/**
	 * Get spawn group names for editor dropdown.
	 */
	UFUNCTION()
	TArray<FString> GetSpawnGroupNames() const;

	/**
	 * Get AID state names for editor dropdown.
	 */
	UFUNCTION()
	TArray<FString> GetAIDStateNames() const;
};
