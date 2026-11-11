// Copyright (C) 2024 Van de Walle Bastien
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "AID/RecallAIDSpawnTypes.h"
#include "GameplayTagContainer.h"

#include "RecallAIDSpawnGroupModifierTypes.generated.h"

/**
 * Reads an attribute from the AI Director entity and uses it as a weight multiplier.
 */
USTRUCT(DisplayName="Attribute")
struct RECALLAID_API FRecallAIDSpawnGroupAttributeWeightModifier : public FRecallAIDSpawnGroupWeightModifierBase
{
	GENERATED_BODY()

	virtual float GetWeightMultiplier(const FRecallAttributeSet* AttributeSet) const override;

protected:
	UPROPERTY(EditAnywhere, Category="Attribute", meta=(GameplayTagFilter="Attribute"))
	FGameplayTag AttributeTag;
};
