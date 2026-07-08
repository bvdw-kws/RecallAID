// Copyright (C) 2024 Van de Walle Bastien
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "MassEntityTraitBase.h"
#include "AID/RecallAIDSpawnPointSettings.h"

#include "RecallAIDTraits.generated.h"

/*
* Trait to define an AI director.
*/
UCLASS(meta=(DisplayName="MS AI Director"))
class RECALLAID_API URecallAIDTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override final;

protected:
	UPROPERTY(EditAnywhere, Category="AI Director", DisplayName="AI Director")
	TObjectPtr<class URecallAIDAsset> AIDAsset;
};

/*
* Trait to define an AI director spawn point.
*/
UCLASS(meta=(DisplayName="MS AI Director Spawn Point"))
class RECALLAID_API URecallAIDSpawnPointTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override final;

protected:
	UPROPERTY(EditAnywhere, Category="AI Director", DisplayName="AI Director")
	FRecallAIDSpawnPointSettings SpawnPointSettings;
};
