// Copyright (C) 2024 Van de Walle Bastien
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "MassProcessor.h"

#include "RecallAIDSpawnPointProcessors.generated.h"

/**
 * Processor that manages spawn point relevancy based on proximity to controller entities.
 * Spawn points are marked as relevant when within range of any controller, allowing
 * the AI Director to use them for entity spawning.
 */
UCLASS()
class URecallAIDSpawnPointRelevancyProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	URecallAIDSpawnPointRelevancyProcessor();

	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& InEntityManager) override final;

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override final;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override final;

private:
	FMassEntityQuery ControllerEntityQuery;
	FMassEntityQuery SpawnPointEntityQuery;

	TSharedPtr<struct FRecallAIDSpawnPointRelevancyCacheManager> CacheManager;
};
