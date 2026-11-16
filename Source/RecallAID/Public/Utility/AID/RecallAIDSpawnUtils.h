// Copyright (C) 2024 Van de Walle Bastien
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"

// Forward declarations
struct FMassEntityManager;
struct FRecallAIDFragment;
struct FRecallAIDSpawnGroup;
struct FRecallAIDSpawnPointCache;
struct FRecallAIDState;
struct FRecallAIDStateGroupEntry;
struct FRecallAttributeSet;
class URecallAIDAsset;
class URecallEntityAsyncSpawnSubsystem;
class URecallRandomNumberSubsystem;

/**
 * Cache structure for spawn point data to avoid repeated queries.
 */
struct FRecallAIDSpawnPointCache
{
	const struct FRecallAIDSpawnPointSettings* SpawnPointSettings = nullptr;
	struct FRecallAIDSpawnPointFragment* SpawnPointFragment = nullptr;
	/** The spawner entity. Used as the key of the director's per-spawner timers. */
	FMassEntityHandle Entity;
	FVector Location = FVector::ZeroVector;
	FQuat Rotation = FQuat::Identity;
};

/**
 * Context structure containing all systems needed for entity spawning.
 * This provides a cleaner interface than passing the full execution context.
 */
struct FRecallAIDSpawnContext
{
	/** World reference for navigation system access */
	UWorld& World;
	
	/** Random number subsystem for spawn calculations */
	URecallRandomNumberSubsystem& RandomNumberSystem;
	
	/** Async spawn subsystem for entity creation */
	URecallEntityAsyncSpawnSubsystem& EntityAsyncSpawnSystem;
	
	FRecallAIDSpawnContext(
		UWorld& InWorld,
		URecallRandomNumberSubsystem& InRandomNumberSystem,
		URecallEntityAsyncSpawnSubsystem& InEntityAsyncSpawnSystem)
		: World(InWorld)
		, RandomNumberSystem(InRandomNumberSystem)
		, EntityAsyncSpawnSystem(InEntityAsyncSpawnSystem)
	{
	}
};

namespace Recall::AID::Utils
{
	/**
	 * Returns the effective weight for an AID state group entry.
	 * Attribute-based modifiers return 1.0 when the entity has no attribute set or matching attribute.
	 */
	RECALLAID_API extern float GetSpawnGroupEntryWeight(
		const FRecallAIDStateGroupEntry& GroupEntry,
		const FRecallAttributeSet* AttributeSet);

	/**
	 * Spawns a group of entities based on AI Director spawn group configuration.
	 * Handles spawn point selection, position calculation, and navigation projection.
	 * 
	 * @param SpawnContext Context containing required systems for spawning
	 * @param AIDEntity The AI Director entity that owns these spawned entities
	 * @param SpawnEnemyGroup Configuration for the spawn group
	 * @param SpawnPoints Available spawn points to choose from
	 * @param EntityCount Number of entities to spawn
	 * @param CurrentTime Current game time for updating spawn point usage
	 * @return Number of entities actually spawned
	 */
	RECALLAID_API extern int32 SpawnEnemyGroup(
		const FRecallAIDSpawnContext& SpawnContext,
		const FMassEntityHandle& AIDEntity,
		const FRecallAIDSpawnGroup& SpawnEnemyGroup,
		const TArray<FRecallAIDSpawnPointCache>& SpawnPoints,
		int32 EntityCount,
		float CurrentTime);

	/**
	 * Processes one AI Director using its state's Parallel or Random Weight spawn rule.
	 * 
	 * @param SpawnContext Context containing required systems for spawning
	 * @param AIDAsset AI Director asset containing spawn group configuration
	 * @param AIDFragment AI Director fragment containing runtime state
	 * @param CurrentAIDState Current AI Director state (can be null for default behavior)
	 * @param SpawnPoints Available spawn points for entity placement
	 * @param RandomStream Random stream for spawn calculations
	 * @param AttributeSet Attributes belonging to the AI Director entity, if present
	 * @param DeltaTime Delta time for timer updates
	 * @param AIDEntity The AI Director entity that owns these spawned entities
	 */
	RECALLAID_API extern void ProcessAIDSpawnGroups(
		const FRecallAIDSpawnContext& SpawnContext,
		const URecallAIDAsset& AIDAsset,
		const FMassEntityHandle& AIDEntity,
		FRecallAIDFragment& AIDFragment,
		const FRecallAIDState* CurrentAIDState,
		const TArray<FRecallAIDSpawnPointCache>& SpawnPoints,
		const FRandomStream& RandomStream,
		const FRecallAttributeSet* AttributeSet,
		float DeltaTime);

	/**
	 * Decrements the mob count for an AI Director entity when one of its spawned mobs is destroyed.
	 * Includes safety checks and debug logging.
	 * 
	 * @param EntityManager Entity manager for accessing the AI Director
	 * @param AIDEntity The AI Director entity whose mob count should be decremented
	 */
	RECALLAID_API extern void DecrementAIDMobCount(
		FMassEntityManager& EntityManager,
		const FMassEntityHandle& AIDEntity);
}
