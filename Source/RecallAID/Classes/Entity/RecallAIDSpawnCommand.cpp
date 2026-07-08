// Copyright (C) 2024 Van de Walle Bastien
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0


#include "RecallAIDSpawnCommand.h"

#include "MassEntityManager.h"
#include "MassEntityView.h"
#include "Simulation/AID/RecallAIDFragments.h"
#include "Simulation/Physics/RecallPhysicsBodyFragment.h"
#include "Simulation/Transform/RecallTransformFragments.h"

void FRecallAIDSpawnCommand::OnSpawn(FMassEntityManager& System,
                                       const TArray<FMassEntityHandle>& Entities) const
{
	if (!ensureAlwaysMsgf(Entities.Num() == SpawnInfos.Num(),
		TEXT("%hs Must have spawn info for each entity"), __FUNCTION__))
	{
		return;
	}
	
	for (int32 EntityIndex = 0; EntityIndex < Entities.Num(); EntityIndex++)
	{
		const FRecallAIDSpawnInfo& SpawnInfo = SpawnInfos[EntityIndex];
		const FMassEntityHandle& Entity = Entities[EntityIndex];
		
		// Add the fragment if it doesn't exist
		System.AddFragmentToEntity(Entity, FRecallAIDMobTrackingFragment::StaticStruct(),
			[this](void* Fragment, const UScriptStruct& FragmentType)
		{
			if (auto* MobTrackingFragmentPtr = static_cast<FRecallAIDMobTrackingFragment*>(Fragment))
			{
				MobTrackingFragmentPtr->AIDEntity = AIDEntity;
			}
		});
		
		// Create EntityView after potential archetype change
		const FMassEntityView EntityView(System, Entity);

		if (FRecallTransformFragment* TransformFragmentPtr = EntityView.GetFragmentDataPtr<FRecallTransformFragment>())
		{
			TransformFragmentPtr->Position = SpawnInfo.Position;

			if (const FRecallPhysicsBodyFragment* BodyFragmentPtr = EntityView.GetFragmentDataPtr<FRecallPhysicsBodyFragment>())
			{
				TransformFragmentPtr->Position.Z += BodyFragmentPtr->Extents.Z;
			}
		}
	}	
}
