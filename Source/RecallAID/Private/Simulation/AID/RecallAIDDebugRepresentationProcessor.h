// Copyright (C) 2024 Van de Walle Bastien
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "MassEntityQuery.h"
#include "MassProcessor.h"

#include "RecallAIDDebugRepresentationProcessor.generated.h"

/**
 * Processor to debug AI Director entities on Render.
 * Shows the current AID state, spawn rule, mob count and per spawn group timers/weights.
 */
UCLASS()
class URecallAIDDebugRepresentationProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	URecallAIDDebugRepresentationProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
