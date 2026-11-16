// Copyright (C) 2024 Van de Walle Bastien
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "CoreMinimal.h"
#include "AID/RecallAIDSpawnTypes.h"
#include "Simulation/AID/RecallAIDFragments.h"
#include "Utility/AID/RecallAIDSpawnUtils.h"

/**
 * Internal helpers shared by the RecallAIDSpawn*.cpp files. Not part of the module's public API.
 *
 * File layout:
 *  - RecallAIDSpawnConfig.cpp    : state/group configuration and weighted selection
 *  - RecallAIDSpawnTiming.cpp    : timers, burst, scheduling the next Random Weight group
 *  - RecallAIDSpawnRules.cpp     : Parallel and Random Weight rules over one timer set
 *  - RecallAIDSpawnPlacement.cpp : spawner pick, spawn positions and the async spawn request
 *  - RecallAIDSpawnUtils.cpp     : orchestration (Global vs PerSpawner scope) and mob count
 */
namespace Recall::AID::Utils::Private
{
	/** Effective configuration of one spawn group inside the current AID state. */
	struct FSpawnGroupConfiguration
	{
		bool bGroupEnabled = true;
		float SpawnSpeedModifier = 1.0f;
		float SpawnRateModifier = 1.0f;
		bool bBurstMode = false;
		float BurstDuration = 0.0f;
	};

	/** Everything a rule needs besides the timer set and the spawn callback. */
	struct FSpawnRuleContext
	{
		const URecallAIDAsset& AIDAsset;
		const FRecallAIDState* CurrentAIDState;
		const FRecallAttributeSet* AttributeSet;
		const FRandomStream& RandomStream;
		float DeltaTime;
	};

	/**
	 * Tries to spawn the given group. Returns true if entities were spawned.
	 * Rolling the entity count and the mob limit check are the callback's responsibility.
	 */
	using FTrySpawnGroupFunc = TFunctionRef<bool(int32 GroupIndex, const FSpawnGroupConfiguration& Config)>;

	// Only the symbols used by RecallAIDTestSuite are exported (RECALLAID_API); the rest is module-internal.

	// --- RecallAIDSpawnConfig.cpp ---
	FSpawnGroupConfiguration GetSpawnGroupConfiguration(
		const FRecallAIDSpawnGroup& SpawnGroup,
		const FRecallAIDState* CurrentAIDState);

	/** Selects one enabled group using its effective state-entry weight. Returns INDEX_NONE if none qualifies. */
	int32 SelectWeightedSpawnGroup(const FSpawnRuleContext& Rule);

	/**
	 * The group's SpawnIntervalRange with its lower-bound and upper-bound modifiers applied.
	 * Bounds are clamped to be non-negative and Max is never below Min.
	 */
	RECALLAID_API FFloatInterval GetSpawnIntervalBounds(
		const FRecallAIDSpawnGroup& SpawnGroup,
		const FRecallAttributeSet* AttributeSet);

	// --- RecallAIDSpawnTiming.cpp ---
	/** Creates the group instances of a timer set and seeds each timer from the group's InitialTimerRange. */
	RECALLAID_API void InitializeTimerSet(
		const URecallAIDAsset& AIDAsset,
		const FRandomStream& RandomStream,
		FRecallAIDSpawnTimerSet& OutTimers);

	/** Advances duration-based burst timers for enabled groups. */
	RECALLAID_API void UpdateBurstMode(const FSpawnRuleContext& Rule, FRecallAIDSpawnTimerSet& Timers);

	/** Counts the timer down. Returns true if spawning should occur this frame. */
	bool ProcessSpawnTiming(float& SpawnTimer, const FSpawnGroupConfiguration& Config, float DeltaTime);

	/** Rolls the delay until the group's next spawn from its modified Spawn Interval. Consumes one random value. */
	float RollSpawnInterval(const FSpawnRuleContext& Rule, int32 GroupIndex);

	/** Picks the next Random Weight group and rolls its timer. Returns false if no group can be selected. */
	bool ScheduleNextSpawnGroup(const FSpawnRuleContext& Rule, FRecallAIDSpawnTimerSet& Timers);

	// --- RecallAIDSpawnRules.cpp ---
	/** Every enabled group owns an independent timer. */
	RECALLAID_API void ProcessParallelRule(
		const FSpawnRuleContext& Rule,
		FRecallAIDSpawnTimerSet& Timers,
		const UWorld* World,
		const FMassEntityHandle& AIDEntity,
		FTrySpawnGroupFunc TrySpawnGroup);

	/** One group is selected by weight per spawn event; only its timer counts down. */
	RECALLAID_API void ProcessRandomWeightRule(
		const FSpawnRuleContext& Rule,
		FRecallAIDSpawnTimerSet& Timers,
		const UWorld* World,
		const FMassEntityHandle& AIDEntity,
		FTrySpawnGroupFunc TrySpawnGroup);

	// --- RecallAIDSpawnPlacement.cpp ---
	/**
	 * Spawns EntityCount entities of the group at one specific spawner and starts that spawner's cooldown.
	 * Returns the number of entities requested.
	 */
	int32 SpawnEnemyGroupAt(
		const FRecallAIDSpawnContext& SpawnContext,
		const FMassEntityHandle& AIDEntity,
		const FRecallAIDSpawnGroup& SpawnGroup,
		const FRecallAIDSpawnPointCache& SpawnPoint,
		int32 EntityCount,
		float CurrentTime);
}
