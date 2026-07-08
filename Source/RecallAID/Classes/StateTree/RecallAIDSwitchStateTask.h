// Copyright (C) 2024 Van de Walle Bastien
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "StateTree/RecallStateTreeTaskBase.h"

#include "RecallAIDSwitchStateTask.generated.h"

USTRUCT()
struct RECALLAID_API FRecallAIDSwitchStateInstanceData
{
	GENERATED_BODY()

	/**
	 * The name of the AID state to switch to (must match a StateName in the AIDAsset).
	 * Empty/None name will revert to default behavior (all spawn groups enabled).
	 */
	UPROPERTY(EditAnywhere, Category="Parameter", DisplayName="State Name")
	FName TargetStateName = TEXT("Default");
};

/**
 * State tree task to switch AID state on entities with FRecallAIDFragment.
 */
USTRUCT(meta=(DisplayName="AID Switch State"))
struct RECALLAID_API FRecallAIDSwitchStateTask : public FRecallStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FRecallAIDSwitchStateInstanceData;

	FRecallAIDSwitchStateTask() = default;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	UPROPERTY(EditAnywhere, Category=Parameter)
	bool bSucceedOnSwitchState = true;
	
private:
	TStateTreeExternalDataHandle<struct FRecallAIDFragment> AIDFragmentHandle;
	TStateTreeExternalDataHandle<struct FRecallAIDConstSharedFragment> AIDConstSharedFragmentHandle;
};
