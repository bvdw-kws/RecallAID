// Copyright (C) 2024 Van de Walle Bastien
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "MassObserverProcessor.h"

#include "RecallAIDProcessors.generated.h"

/**
 * Observer processor that initializes AI Director entities when they are spawned.
 * Sets up initial spawn timers for all configured spawn groups.
 */
UCLASS()
class URecallAIDirectorInitializer : public UMassObserverProcessor
{
	GENERATED_BODY()

	URecallAIDirectorInitializer();

public:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& InEntityManager) override final;

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override final;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override final;

private:
	FMassEntityQuery EntityQuery;
};

/**
 * Processor that tracks AI Director state changes and resets burst spawn flags
 * when the state changes to allow burst spawns to trigger again.
 */
UCLASS()
class URecallAIDStateChangeProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	URecallAIDStateChangeProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override final;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override final;

private:
	FMassEntityQuery EntityQuery;
};

/**
 * Main AI Director processor that manages entity spawning based on configured spawn groups.
 * Handles spawn timing, location selection, and mob count limits.
 */
UCLASS()
class URecallAIDSpawnProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	URecallAIDSpawnProcessor();

	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& InEntityManager) override final;

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override final;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override final;

private:
	FMassEntityQuery SpawnPointEntityQuery;
	FMassEntityQuery DirectorEntityQuery;

	TSharedPtr<struct FRecallAIDSpawnCacheManager> CacheManager;
};

/**
 * Observer processor that tracks when spawned mobs are destroyed and decrements 
 * the corresponding AI Director's mob count to maintain accurate spawn limits.
 */
UCLASS()
class URecallAIDMobDestroyObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

	URecallAIDMobDestroyObserver();

public:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& InEntityManager) override final;

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override final;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override final;

private:
	FMassEntityQuery EntityQuery;
};
