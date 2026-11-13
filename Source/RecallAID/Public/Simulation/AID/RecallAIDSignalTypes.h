// Copyright (C) 2024 Van de Walle Bastien
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "CoreMinimal.h"

namespace Recall::AID::Signals
{

	/** Resume the AI Director: adds FRecallAIDRunningTag so the spawn processor updates it again. */
	const FName Start		= FName(TEXT("RecallAIDStart"));
	/** Pause the AI Director: removes FRecallAIDRunningTag, spawn timers and spawning are frozen. */
	const FName Stop		= FName(TEXT("RecallAIDStop"));

} // namespace Recall::AID::Signals
