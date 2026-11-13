// Copyright (C) 2024 Van de Walle Bastien
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0


#include "RecallAIDSignalProcessors.h"

#include "Desync/RecallDesyncLog.h"
#include "MassExecutionContext.h"
#include "Simulation/AID/RecallAIDFragments.h"
#include "Simulation/AID/RecallAIDSignalTypes.h"

//----------------------------------------------------------------------//
// URecallAIDSignalProcessor
//----------------------------------------------------------------------//
URecallAIDSignalProcessor::URecallAIDSignalProcessor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void URecallAIDSignalProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& InEntityManager)
{
	Super::InitializeInternal(Owner, InEntityManager);

	SubscribeToSignal(Recall::AID::Signals::Start);
	SubscribeToSignal(Recall::AID::Signals::Stop);
}

void URecallAIDSignalProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FRecallAIDFragment>(EMassFragmentAccess::ReadOnly);
}

void URecallAIDSignalProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FRecallSignalNameLookup& EntitySignals)
{
	QUICK_SCOPE_CYCLE_COUNTER(Recall_AID_Signal);

	EntityQuery.ForEachEntityChunk(Context, [&EntitySignals](FMassExecutionContext& Context)
	{
		const bool bIsRunning = Context.DoesArchetypeHaveTag<FRecallAIDRunningTag>();

		for (int32 EntityIndex = 0; EntityIndex < Context.GetNumEntities(); EntityIndex++)
		{
			const FMassEntityHandle Entity = Context.GetEntity(EntityIndex);

			TArray<FName> Signals;
			EntitySignals.GetSignalsForEntity(Entity, Signals);

			// If both signals were raised the same frame, the last one in the buffer wins.
			bool bShouldRun = bIsRunning;
			for (const FName& Signal : Signals)
			{
				if (Signal == Recall::AID::Signals::Start)
				{
					bShouldRun = true;
				}
				else if (Signal == Recall::AID::Signals::Stop)
				{
					bShouldRun = false;
				}
			}

			if (bShouldRun == bIsRunning)
			{
				continue;
			}

			if (bShouldRun)
			{
				Context.Defer().AddTag<FRecallAIDRunningTag>(Entity);
			}
			else
			{
				Context.Defer().RemoveTag<FRecallAIDRunningTag>(Entity);
			}

#if RECALL_DESYNC_LOG
			RECALL_DESYNC_LOG_STR(Context.GetWorld(), AIDRunningChanged,
				FString::Printf(TEXT("%s Running: %s"),
				*Entity.DebugGetDescription(), *Recall::Desync::Conv_BoolToString(bShouldRun)));
#endif // RECALL_DESYNC_LOG
		}
	});
}
