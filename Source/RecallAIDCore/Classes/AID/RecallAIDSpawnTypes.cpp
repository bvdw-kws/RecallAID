// Copyright (C) 2024 Van de Walle Bastien
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0


#include "RecallAIDSpawnTypes.h"

float FRecallAIDSpawnGroupWeightModifierBase::GetWeightMultiplier(const FRecallAttributeSet*) const
{
	return 1.0f;
}

bool FRecallAIDSpawnGroup::Reload()
{
	bool bChanged = false;
#if WITH_EDITOR
	// The deprecated fields default to (0, 0), so a range is only migrated if a bound holds a positive value.
	// Bounds are checked before being read, as GetLowerBoundValue()/GetUpperBoundValue() assert on open bounds.
	const auto HasLoadedData = [](const FFloatRange& Range)
	{
		return (Range.HasLowerBound() && Range.GetLowerBoundValue() > 0.0f)
			|| (Range.HasUpperBound() && Range.GetUpperBoundValue() > 0.0f);
	};

	if (HasLoadedData(InitialTimer_DEPRECATED))
	{
		InitialTimerRange = FVariableCollectionFloatRange(InitialTimer_DEPRECATED);
		InitialTimer_DEPRECATED = FFloatRange(0.0f, 0.0f);
		bChanged = true;
	}

	if (HasLoadedData(SpawnInterval_DEPRECATED))
	{
		SpawnIntervalRange = FVariableCollectionFloatRange(SpawnInterval_DEPRECATED);
		SpawnInterval_DEPRECATED = FFloatRange(0.0f, 0.0f);
		bChanged = true;
	}
#endif // WITH_EDITOR
	return bChanged;
}
