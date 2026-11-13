// Copyright (C) 2024 Van de Walle Bastien
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0

#include "RecallAIDDebugRepresentationProcessor.h"

#include "AID/RecallAIDAsset.h"
#include "AID/RecallAIDSpawnTypes.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "MassExecutionContext.h"
#include "Simulation/AID/RecallAIDFragments.h"
#include "Simulation/Attribute/RecallAttributeFragments.h"
#include "Simulation/Transform/RecallTransformFragments.h"
#include "Utility/AID/RecallAIDSpawnUtils.h"

//----------------------------------------------------------------------//
// URecallAIDDebugRepresentationProcessor
//----------------------------------------------------------------------//
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
static bool bDebugRecallAIDOnScreenLog = false;
static FAutoConsoleVariableRef CVarRecallAIDOnScreenLog(
	TEXT("Recall.AID.DebugOnScreen"),
	bDebugRecallAIDOnScreenLog,
	TEXT("Display AI Director state and spawn groups on screen")
);

static bool bDebugRecallAIDInWorldLog = false;
static FAutoConsoleVariableRef CVarRecallAIDInWorldLog(
	TEXT("Recall.AID.DebugInWorld"),
	bDebugRecallAIDInWorldLog,
	TEXT("Display AI Director state and spawn groups in world (requires a transform on the AID entity)")
);
#endif // UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT

URecallAIDDebugRepresentationProcessor::URecallAIDDebugRepresentationProcessor()
	: EntityQuery(*this)
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::All);
	ProcessingPhase = EMassProcessingPhase::Render;
	bRequiresGameThreadExecution = true;
}

void URecallAIDDebugRepresentationProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	EntityQuery.AddRequirement<FRecallAIDFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddTagRequirement<FRecallAIDRunningTag>(EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FRecallTransformFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FRecallAttributeFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddConstSharedRequirement<FRecallAIDConstSharedFragment>();
#endif // UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
}

void URecallAIDDebugRepresentationProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	QUICK_SCOPE_CYCLE_COUNTER(Recall_AID_DebugRender);

	if (!bDebugRecallAIDOnScreenLog && !bDebugRecallAIDInWorldLog)
	{
		return;
	}

	EntityQuery.ForEachEntityChunk(Context, [](FMassExecutionContext& Context)
	{
		const FRecallAIDConstSharedFragment& AIDConstSharedFragment = Context.GetConstSharedFragment<FRecallAIDConstSharedFragment>();
		const URecallAIDAsset* AIDAsset = AIDConstSharedFragment.AIDAsset;
		if (!AIDAsset)
		{
			return;
		}

		const bool bIsRunning = Context.DoesArchetypeHaveTag<FRecallAIDRunningTag>();

		const TConstArrayView<FRecallAIDFragment> AIDList = Context.GetFragmentView<FRecallAIDFragment>();
		const TConstArrayView<FRecallTransformFragment> TransformList = Context.GetFragmentView<FRecallTransformFragment>();
		const TConstArrayView<FRecallAttributeFragment> AttributeList = Context.GetFragmentView<FRecallAttributeFragment>();

		for (int32 EntityIndex = 0; EntityIndex < Context.GetNumEntities(); EntityIndex++)
		{
			const FRecallAIDFragment& AIDFragment = AIDList[EntityIndex];
			const FRecallTransformFragment* TransformFragmentPtr = TransformList.IsValidIndex(EntityIndex)
				? &TransformList[EntityIndex]
				: nullptr;
			const FRecallAttributeSet* AttributeSet = AttributeList.IsValidIndex(EntityIndex)
				? &AttributeList[EntityIndex].AttributeSet
				: nullptr;

			const FRecallAIDState* CurrentState = AIDFragment.CurrentAIDStateName.IsNone()
				? nullptr
				: AIDAsset->AIDStates.FindByPredicate([&](const FRecallAIDState& State)
					{
						return State.StateName == AIDFragment.CurrentAIDStateName;
					});

			const TCHAR* RuleText = TEXT("Parallel (no state)");
			if (CurrentState)
			{
				RuleText = CurrentState->SpawnRule == ERecallAIDSpawnRule::RandomWeight ? TEXT("RandomWeight") : TEXT("Parallel");
			}

			// One entry per line: the on-screen message API only supports a single colour per message.
			TArray<TPair<FString, FColor>> Lines;
			Lines.Emplace(FString::Printf(TEXT("AID: %s%s"), *AIDAsset->GetName(), bIsRunning ? TEXT("") : TEXT(" (PAUSED)")),
				bIsRunning ? FColor::Orange : FColor::Yellow);
			const bool bStateNotFound = !AIDFragment.CurrentAIDStateName.IsNone() && !CurrentState;
			Lines.Emplace(FString::Printf(TEXT("State: %s%s"), *AIDFragment.CurrentAIDStateName.ToString(),
				bStateNotFound ? TEXT(" <Not found>") : TEXT("")), bStateNotFound ? FColor::Red : FColor::Orange);
			Lines.Emplace(FString::Printf(TEXT("Rule: %s | Mobs: %d/%d"), RuleText, AIDFragment.MobCount, AIDAsset->MobLimit), FColor::Orange);
			if (CurrentState)
			{
				Lines.Emplace(FString::Printf(TEXT("Speed: %.2fx | Rate: %.2fx"),
					CurrentState->SpawnSpeedModifier, CurrentState->SpawnRateModifier), FColor::Orange);
			}

			for (int32 GroupIndex = 0; GroupIndex < AIDAsset->SpawnGroups.Num(); GroupIndex++)
			{
				const FRecallAIDSpawnGroup& SpawnGroup = AIDAsset->SpawnGroups[GroupIndex];
				const FRecallAIDStateGroupEntry* Entry = CurrentState
					? CurrentState->SpawnGroupEntries.FindByPredicate([&](const FRecallAIDStateGroupEntry& E)
						{
							return E.SpawnGroupName == SpawnGroup.GroupName;
						})
					: nullptr;
				const bool bEnabled = !CurrentState || (Entry && Entry->bEnabled);

				FString Line = FString::Printf(TEXT("  [%d] %s"), GroupIndex, *SpawnGroup.GroupName.ToString());
				FColor LineColor = FColor::White;
				if (!bEnabled)
				{
					Line += TEXT(" (disabled)");
					LineColor = FColor(128, 128, 128);
				}
				else
				{
					if (AIDFragment.SpawnGroupInstances.IsValidIndex(GroupIndex))
					{
						const FRecallAIDSpawnGroupInstance& Instance = AIDFragment.SpawnGroupInstances[GroupIndex];
						Line += FString::Printf(TEXT(" Timer: %.1fs"), Instance.SpawnTimer);
						if (Instance.bBurstCompleted)
						{
							Line += TEXT(" (burst done)");
						}
					}
					if (CurrentState && CurrentState->SpawnRule == ERecallAIDSpawnRule::RandomWeight && Entry)
					{
						Line += FString::Printf(TEXT(" Weight: %.2f"),
							Recall::AID::Utils::GetSpawnGroupEntryWeight(*Entry, AttributeSet));
					}
					if (AIDFragment.PendingSpawnGroupIndex == GroupIndex)
					{
						Line += TEXT(" <Pending>");
						LineColor = FColor::Green;
					}
				}
				Lines.Emplace(MoveTemp(Line), LineColor);
			}

			if (bDebugRecallAIDInWorldLog && TransformFragmentPtr != nullptr)
			{
				// DrawDebugString is single-colour, so the pending group is only marked by its text.
				FString WorldText;
				for (const TPair<FString, FColor>& Line : Lines)
				{
					WorldText += Line.Key + TEXT("\n");
				}
				DrawDebugString(Context.GetWorld(), TransformFragmentPtr->Position, WorldText, nullptr, FColor::Orange, -1);
			}

			if (bDebugRecallAIDOnScreenLog && GEngine)
			{
				for (const TPair<FString, FColor>& Line : Lines)
				{
					GEngine->AddOnScreenDebugMessage(-1, 0.f, Line.Value, Line.Key, /*bNewerOnTop*/ false);
				}
			}
		}
	});
#endif // UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
}
