// Copyright (C) 2024 Van de Walle Bastien
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "Collection/VariableCollectionTypes.h"

#include "RecallAIDSpawnTypes.generated.h"

struct FRecallAttributeSet;

/**
 * Base type for modifiers that return a multiplier computed from the director's attributes.
 * Used for a spawn group entry's weight and for the lower and upper bounds of its spawn interval.
 */
USTRUCT()
struct RECALLAIDCORE_API FRecallAIDSpawnGroupWeightModifierBase
{
	GENERATED_BODY()

	virtual ~FRecallAIDSpawnGroupWeightModifierBase() = default;
	virtual float GetWeightMultiplier(const FRecallAttributeSet* AttributeSet) const;
};

/** Controls how the enabled spawn groups in an AID state are scheduled. */
UENUM(BlueprintType)
enum class ERecallAIDSpawnRule : uint8
{
	/** Each enabled group has an independent timer and can spawn in the same frame as another group. This preserves the original behavior. Spawn weights are ignored. */
	Parallel UMETA(DisplayName="Parallel"),
	/** Select one enabled group for each spawn event using entry weights. Only its timer counts down; its first event uses InitialTimer and later events use SpawnInterval. Burst settings are ignored. */
	RandomWeight UMETA(DisplayName="Random Weight")
};

/** Controls whether spawn timers are owned by the director or by each individual spawner. */
UENUM(BlueprintType)
enum class ERecallAIDSpawnScope : uint8
{
	/** The director owns one set of timers. Each spawn event picks a random available spawner. This preserves the original behavior. */
	Global UMETA(DisplayName="Global"),
	/** Every spawner in range owns its own set of timers (initial delay and interval per group) and spawns from itself. The rule (Parallel / Random Weight) is applied independently per spawner. */
	PerSpawner UMETA(DisplayName="Per Spawner")
};

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
	 * Re-applies loaded data to the current layout. Migrates the deprecated timer ranges
	 * into InitialTimerRange and SpawnIntervalRange (editor only). Called by the owning data asset on load.
	 * @return True if any data was changed, so the owner can mark itself dirty.
	 */
	bool Reload();

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
	UPROPERTY(EditAnywhere, DisplayName="Initial Timer", meta=(Units=Seconds))
	FVariableCollectionFloatRange InitialTimerRange{ 10.0f, 20.0f };

	/**
	 * Spawn interval of this wave.
	 */
	UPROPERTY(EditAnywhere, DisplayName="Spawn Interval", meta=(Units=Seconds))
	FVariableCollectionFloatRange SpawnIntervalRange{ 60.0f, 240.0f };

	/**
	 * Multiplicative modifiers applied to the lower bound of Spawn Interval each time the next spawn is scheduled.
	 * The first spawn still uses Initial Timer.
	 */
	UPROPERTY(EditAnywhere, meta=(BaseStruct="/Script/RecallAIDCore.RecallAIDSpawnGroupWeightModifierBase", ExcludeBaseStruct, ToolTip="These modifiers multiply the lower bound of Spawn Interval. If the modified lower bound exceeds the upper bound, the upper bound is raised to match."))
	TArray<FInstancedStruct> SpawnIntervalMinModifiers;

	/**
	 * Multiplicative modifiers applied to the upper bound of Spawn Interval each time the next spawn is scheduled.
	 * The first spawn still uses Initial Timer.
	 */
	UPROPERTY(EditAnywhere, meta=(BaseStruct="/Script/RecallAIDCore.RecallAIDSpawnGroupWeightModifierBase", ExcludeBaseStruct, ToolTip="These modifiers multiply the upper bound of Spawn Interval. If the modified upper bound is below the lower bound, it is raised to the lower bound."))
	TArray<FInstancedStruct> SpawnIntervalMaxModifiers;

	/** Migrated into InitialTimerRange on load. A range of (0, 0) means "not loaded from old data". */
	UPROPERTY(meta=(DeprecatedProperty))
	FFloatRange InitialTimer_DEPRECATED{ 0.0f, 0.0f };

	/** Migrated into SpawnIntervalRange on load. A range of (0, 0) means "not loaded from old data". */
	UPROPERTY(meta=(DeprecatedProperty))
	FFloatRange SpawnInterval_DEPRECATED{ 0.0f, 0.0f };

	/**
	 * True, if spawned position should be projected on navigation.
	 */
	UPROPERTY(EditAnywhere)
	bool bProjectOntoNavigation = true;
};

/**
 * Entry that links a spawn group to an AID state and defines its availability and relative weight there.
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

	/** Relative probability for this group when Random Weight is selected. A weight of zero excludes it. */
	UPROPERTY(EditAnywhere, meta=(ClampMin="0.0", ToolTip="Relative selection weight when the state's rule is Random Weight. A weight of zero excludes this group."))
	float Weight = 1.0f;

	/** Multiplicative modifiers applied to this entry's weight under Random Weight. */
	UPROPERTY(EditAnywhere, meta=(BaseStruct="/Script/RecallAIDCore.RecallAIDSpawnGroupWeightModifierBase", ExcludeBaseStruct, ToolTip="These modifiers multiply this entry's weight under the Random Weight rule; count and timing are unchanged."))
	TArray<FInstancedStruct> WeightModifiers;
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

	/** Rule used to schedule this state's enabled spawn groups. */
	UPROPERTY(EditAnywhere)
	ERecallAIDSpawnRule SpawnRule = ERecallAIDSpawnRule::Parallel;

	/**
	 * Who owns the spawn timers. Global: the director (random spawner per spawn). Per Spawner: every spawner in range
	 * has its own initial delay and interval per group, and the rule above is applied independently per spawner.
	 */
	UPROPERTY(EditAnywhere)
	ERecallAIDSpawnScope SpawnScope = ERecallAIDSpawnScope::Global;

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
	UPROPERTY(EditAnywhere, meta=(EditCondition="SpawnRule == ERecallAIDSpawnRule::Parallel", EditConditionHides))
	bool bBurstMode = false;

	/**
	 * Burst duration in seconds (only used when bBurstMode is true).
	 * 0.0 = spawn only on first frame (instant burst), > 0.0 = spawn for specified duration (spread burst).
	 */
	UPROPERTY(EditAnywhere, meta=(Units=Seconds, ClampMin="0.0", EditCondition="SpawnRule == ERecallAIDSpawnRule::Parallel && bBurstMode", EditConditionHides))
	float BurstDuration = 0.0f;
};
