// Copyright (C) 2024 Van de Walle Bastien
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "AID/RecallAIDSpawnTypes.h"

#include "RecallAIDTestModifiers.generated.h"

/**
 * Test modifier that returns a fixed multiplier, so modifier math can be tested without an attribute set.
 * Hidden so it does not appear in the modifier pickers of the editor.
 */
USTRUCT(DisplayName="Test Fixed Multiplier (Tests Only)", meta=(Hidden))
struct FRecallAIDTestFixedModifier : public FRecallAIDSpawnGroupWeightModifierBase
{
	GENERATED_BODY()

	FRecallAIDTestFixedModifier() = default;
	explicit FRecallAIDTestFixedModifier(float InMultiplier) : Multiplier(InMultiplier) {}

	virtual float GetWeightMultiplier(const FRecallAttributeSet*) const override { return Multiplier; }

	UPROPERTY()
	float Multiplier = 1.0f;
};
