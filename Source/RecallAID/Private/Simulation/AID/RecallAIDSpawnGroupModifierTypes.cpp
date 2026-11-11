// Copyright (C) 2024 Van de Walle Bastien
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0

#include "Simulation/AID/RecallAIDSpawnGroupModifierTypes.h"

#include "Attribute/RecallAttributeSetTypes.h"

float FRecallAIDSpawnGroupAttributeWeightModifier::GetWeightMultiplier(const FRecallAttributeSet* AttributeSet) const
{
	if (AttributeSet == nullptr || !AttributeTag.IsValid())
	{
		return 1.0f;
	}

	return AttributeSet->GetValue(AttributeTag, 1.0f);
}
