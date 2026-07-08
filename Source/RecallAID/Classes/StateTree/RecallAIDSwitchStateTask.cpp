// Copyright (C) 2024 Van de Walle Bastien
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0


#include "RecallAIDSwitchStateTask.h"

#include "IRecallAID.h"
#include "AID/RecallAIDAsset.h"
#include "Simulation/AID/RecallAIDFragments.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeLinker.h"

//----------------------------------------------------------------------//
// FRecallAIDSwitchStateTask
//----------------------------------------------------------------------//
bool FRecallAIDSwitchStateTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(AIDFragmentHandle);
	Linker.LinkExternalData(AIDConstSharedFragmentHandle);
	return true;
}

EStateTreeRunStatus FRecallAIDSwitchStateTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
	// Get fragments via external data	
	const FRecallAIDConstSharedFragment& AIDConstSharedFragment = Context.GetExternalData(AIDConstSharedFragmentHandle);
	const TObjectPtr<URecallAIDAsset>& AIDAsset = AIDConstSharedFragment.AIDAsset;
	
	if (!AIDAsset)
	{
		return EStateTreeRunStatus::Failed;
	}
	
	FRecallAIDFragment& AIDFragment = Context.GetExternalData(AIDFragmentHandle);
	
	// Empty/None state name is always valid (default behavior)
	if (InstanceData.TargetStateName.IsNone())
	{
		return EStateTreeRunStatus::Failed;
	}
	
	// Validate that the target state exists
	const bool bStateExists = AIDAsset->AIDStates.ContainsByPredicate([&](const FRecallAIDState& State) {
		return State.StateName == InstanceData.TargetStateName;
	});
	
	if (bStateExists)
	{
		AIDFragment.CurrentAIDStateName = InstanceData.TargetStateName;

		if (bSucceedOnSwitchState)
		{
			return EStateTreeRunStatus::Succeeded;
		}

		return Super::EnterState(Context, Transition);
	}
	else
	{
		UE_LOG(LogRecallAID, Warning,
			TEXT("%hs Failed to switch to state '%s' because it does not exist in the AID asset '%s'"), __FUNCTION__,
			*InstanceData.TargetStateName.ToString(), *AIDAsset->GetName());
		return EStateTreeRunStatus::Failed;
	}
}
