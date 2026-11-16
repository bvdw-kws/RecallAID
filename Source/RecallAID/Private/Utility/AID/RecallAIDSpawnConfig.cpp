// Copyright (C) 2024 Van de Walle Bastien
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0

#include "RecallAIDSpawnInternal.h"

#include "AID/RecallAIDAsset.h"
#include "Attribute/RecallAttributeSetTypes.h"
#include "Simulation/AID/RecallAIDSpawnGroupModifierTypes.h"

/** Multiplies Value by every modifier. The result is not sanitized. */
static float ApplyModifiers(float Value, const TArray<FInstancedStruct>& Modifiers, const FRecallAttributeSet* AttributeSet)
{
	for (const FInstancedStruct& ModifierData : Modifiers)
	{
		if (const FRecallAIDSpawnGroupWeightModifierBase* ModifierPtr = ModifierData.GetPtr<FRecallAIDSpawnGroupWeightModifierBase>())
		{
			Value *= ModifierPtr->GetWeightMultiplier(AttributeSet);
		}
	}

	return Value;
}

float Recall::AID::Utils::GetSpawnGroupEntryWeight(
	const FRecallAIDStateGroupEntry& GroupEntry,
	const FRecallAttributeSet* AttributeSet)
{
	const float Weight = ApplyModifiers(GroupEntry.Weight, GroupEntry.WeightModifiers, AttributeSet);
	return FMath::IsFinite(Weight) ? FMath::Max(0.0f, Weight) : 0.0f;
}

static const FRecallAIDStateGroupEntry* FindStateGroupEntry(const FRecallAIDState* State, FName GroupName)
{
	if (!State)
	{
		return nullptr;
	}

	return State->SpawnGroupEntries.FindByPredicate([GroupName](const FRecallAIDStateGroupEntry& Entry)
	{
		return Entry.SpawnGroupName == GroupName;
	});
}

FFloatInterval Recall::AID::Utils::Private::GetSpawnIntervalBounds(
	const FRecallAIDSpawnGroup& SpawnGroup,
	const FRecallAttributeSet* AttributeSet)
{
	float Min = SpawnGroup.SpawnIntervalRange.GetMinValue();
	float Max = SpawnGroup.SpawnIntervalRange.GetMaxValue();

	// A bound whose modifiers produce a non-finite value keeps its authored value.
	const float ModifiedMin = ApplyModifiers(Min, SpawnGroup.SpawnIntervalMinModifiers, AttributeSet);
	const float ModifiedMax = ApplyModifiers(Max, SpawnGroup.SpawnIntervalMaxModifiers, AttributeSet);
	Min = FMath::IsFinite(ModifiedMin) ? FMath::Max(0.0f, ModifiedMin) : Min;
	Max = FMath::IsFinite(ModifiedMax) ? FMath::Max(0.0f, ModifiedMax) : Max;

	// Modifiers act on each bound separately, so they can cross. Raise Max rather than swapping.
	return FFloatInterval(Min, FMath::Max(Min, Max));
}

Recall::AID::Utils::Private::FSpawnGroupConfiguration Recall::AID::Utils::Private::GetSpawnGroupConfiguration(
	const FRecallAIDSpawnGroup& SpawnGroup,
	const FRecallAIDState* CurrentAIDState)
{
	FSpawnGroupConfiguration Config;

	if (CurrentAIDState)
	{
		// Check if this group is configured in the current state
		if (const FRecallAIDStateGroupEntry* GroupEntry = FindStateGroupEntry(CurrentAIDState, SpawnGroup.GroupName))
		{
			Config.bGroupEnabled = GroupEntry->bEnabled;
		}
		else
		{
			// Group not mentioned in state = disabled
			Config.bGroupEnabled = false;
		}

		Config.SpawnSpeedModifier = CurrentAIDState->SpawnSpeedModifier;
		Config.SpawnRateModifier = CurrentAIDState->SpawnRateModifier;
		Config.bBurstMode = CurrentAIDState->SpawnRule == ERecallAIDSpawnRule::Parallel
			&& CurrentAIDState->bBurstMode;
		Config.BurstDuration = Config.bBurstMode ? CurrentAIDState->BurstDuration : 0.0f;
	}
	// else: No state = default behavior (all groups enabled)

	return Config;
}

int32 Recall::AID::Utils::Private::SelectWeightedSpawnGroup(const FSpawnRuleContext& Rule)
{
	TArray<int32, TInlineAllocator<16>> CandidateIndices;
	TArray<float, TInlineAllocator<16>> CandidateWeights;
	float TotalWeight = 0.0f;

	for (int32 GroupIndex = 0; GroupIndex < Rule.AIDAsset.SpawnGroups.Num(); GroupIndex++)
	{
		// Without a state (or an entry for the group) there is no weight, so the group cannot be selected.
		const FRecallAIDStateGroupEntry* GroupEntry = FindStateGroupEntry(
			Rule.CurrentAIDState, Rule.AIDAsset.SpawnGroups[GroupIndex].GroupName);
		if (!GroupEntry || !GroupEntry->bEnabled)
		{
			continue;
		}

		// Already clamped to a finite, non-negative value.
		const float EffectiveWeight = GetSpawnGroupEntryWeight(*GroupEntry, Rule.AttributeSet);
		if (EffectiveWeight <= 0.0f)
		{
			continue;
		}

		CandidateIndices.Add(GroupIndex);
		CandidateWeights.Add(EffectiveWeight);
		TotalWeight += EffectiveWeight;
	}

	if (CandidateIndices.IsEmpty() || TotalWeight <= 0.0f || !FMath::IsFinite(TotalWeight))
	{
		return INDEX_NONE;
	}

	float WeightRoll = Rule.RandomStream.FRand() * TotalWeight;
	for (int32 CandidateIndex = 0; CandidateIndex < CandidateIndices.Num(); CandidateIndex++)
	{
		WeightRoll -= CandidateWeights[CandidateIndex];
		if (WeightRoll < 0.0f)
		{
			return CandidateIndices[CandidateIndex];
		}
	}

	// Floating-point rounding can leave the roll at the upper bound.
	return CandidateIndices.Last();
}
