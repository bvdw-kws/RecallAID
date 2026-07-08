// Copyright (C) 2024 Van de Walle Bastien
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0


#include "Simulation/AID/RecallAIDTraits.h"

#include "MassEntityTemplateRegistry.h"
#include "Simulation/AID/RecallAIDFragments.h"

//----------------------------------------------------------------------//
// URecallAIDTrait
//----------------------------------------------------------------------//
void URecallAIDTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);

	BuildContext.AddFragment<FRecallAIDFragment>();

	FRecallAIDConstSharedFragment ConstSharedFragment;
	ConstSharedFragment.AIDAsset = AIDAsset;

	BuildContext.AddConstSharedFragment(EntityManager.GetOrCreateConstSharedFragment(ConstSharedFragment));
}

//----------------------------------------------------------------------//
// URecallAIDSpawnPointTrait
//----------------------------------------------------------------------//
void URecallAIDSpawnPointTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext,
	const UWorld& World) const
{
	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);

	BuildContext.AddFragment<FRecallAIDSpawnPointFragment>();
	
	FRecallAIDSpawnPointConstSharedFragment ConstSharedFragment;
	ConstSharedFragment.SpawnPointSettings = SpawnPointSettings;

	BuildContext.AddConstSharedFragment(EntityManager.GetOrCreateConstSharedFragment(ConstSharedFragment));
}
