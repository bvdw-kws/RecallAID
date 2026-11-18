// Copyright (C) 2024 Van de Walle Bastien
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "CoreMinimal.h"

struct FRecallAIDFragment;
class URecallAIDAsset;

namespace Recall::AID::Utils
{
	/** @return The AID state named StateName in AIDAsset, or nullptr when it does not exist. */
	RECALLAID_API const struct FRecallAIDState* FindAIDState(const URecallAIDAsset* AIDAsset, FName StateName);

	/**
	 * Switches the director to the AID state StateName.
	 * @return False, leaving the current state untouched, when the asset is null, the name is None or the state does not exist.
	 */
	RECALLAID_API bool SetAIDState(FRecallAIDFragment& AIDFragment, const URecallAIDAsset* AIDAsset, FName StateName);

	/**
	 * Switches the director back to the asset's DefaultAIDState.
	 * Falls back to None (all spawn groups active) when the asset has no valid default state.
	 */
	RECALLAID_API void ResetAIDState(FRecallAIDFragment& AIDFragment, const URecallAIDAsset* AIDAsset);
}
