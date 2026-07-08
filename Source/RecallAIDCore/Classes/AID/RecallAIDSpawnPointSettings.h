// Copyright (C) 2024 Van de Walle Bastien
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "CoreMinimal.h"

#include "RecallAIDSpawnPointSettings.generated.h"

/**
 * Configuration settings for AI Director spawn points.
 * Defines the area and relevancy range for entity spawning.
 */
USTRUCT()
struct RECALLAIDCORE_API FRecallAIDSpawnPointSettings
{
	GENERATED_BODY()

	/**
	 * Distance from controller entities at which this spawn point becomes active.
	 * Only spawn points within this range of any controller will be used for spawning.
	 */
	UPROPERTY(EditAnywhere, meta=(Units=Centimeters))
	float RelevancyRange = 10000.0f;

	/**
	 * Size of the area around the spawn point where entities can be spawned.
	 * Entities will be randomly positioned within this area.
	 */
	UPROPERTY(EditAnywhere, meta=(Units=Centimeters))
	float SpawnArea = 200.0f;
};
