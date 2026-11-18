// Copyright (C) 2024 Van de Walle Bastien
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0

#include "Utility/AID/RecallAIDStateUtils.h"

#include "AID/RecallAIDAsset.h"
#include "Simulation/AID/RecallAIDFragments.h"

namespace Recall::AID::Utils
{
	const FRecallAIDState* FindAIDState(const URecallAIDAsset* AIDAsset, const FName StateName)
	{
		if (!AIDAsset || StateName.IsNone())
		{
			return nullptr;
		}

		return AIDAsset->AIDStates.FindByPredicate([StateName](const FRecallAIDState& State)
		{
			return State.StateName == StateName;
		});
	}

	bool SetAIDState(FRecallAIDFragment& AIDFragment, const URecallAIDAsset* AIDAsset, const FName StateName)
	{
		if (!FindAIDState(AIDAsset, StateName))
		{
			return false;
		}

		AIDFragment.CurrentAIDStateName = StateName;
		return true;
	}

	void ResetAIDState(FRecallAIDFragment& AIDFragment, const URecallAIDAsset* AIDAsset)
	{
		const bool bHasDefaultState = AIDAsset && FindAIDState(AIDAsset, AIDAsset->DefaultAIDState);
		AIDFragment.CurrentAIDStateName = bHasDefaultState ? AIDAsset->DefaultAIDState : NAME_None;
	}
}
