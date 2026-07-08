// Copyright (C) 2024 Van de Walle Bastien
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "System/Entity/RecallEntityAsyncSpawnTypes.h"
#include "GameplayTagContainer.h"

#include "RecallAIDSpawnCommand.generated.h"

USTRUCT()
struct RECALLAID_API FRecallAIDSpawnInfo
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleAnywhere)
	FVector Position = FVector::ZeroVector;
};

USTRUCT()
struct RECALLAID_API FRecallAIDSpawnCommand : public FRecallEntityAsyncSpawnCommand
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere)
	FMassEntityHandle AIDEntity;
	
	UPROPERTY(VisibleAnywhere)
	TArray<FRecallAIDSpawnInfo> SpawnInfos;
	
	// FRecallEntityAsyncSpawnCommand implementation Begin
public:	
	virtual void OnSpawn(FMassEntityManager& System, const TArray<FMassEntityHandle>& Entities) const override;
	// FRecallEntityAsyncSpawnCommand implementation End
};
