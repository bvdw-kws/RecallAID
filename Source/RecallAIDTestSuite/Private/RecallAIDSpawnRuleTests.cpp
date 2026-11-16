// Copyright (C) 2024 Van de Walle Bastien
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AID/RecallAIDAsset.h"
#include "Math/RandomStream.h"
#include "RecallAIDTestModifiers.h"
#include "Simulation/AID/RecallAIDFragments.h"
#include "Utility/AID/RecallAIDSpawnInternal.h"
#include "UObject/Package.h"

using namespace Recall::AID::Utils::Private;

/**
 * These tests run the spawn rules on plain timer sets (no world, no Mass), which is what both the Global scope
 * (the director's timer set) and the PerSpawner scope (one timer set per spawner) do.
 * All ranges are min == max so rolled timers are deterministic.
 */
namespace
{
	constexpr float InitialDelay = 5.0f;
	constexpr float IntervalDelay = 9.0f;

	struct FTestSetup
	{
		TStrongObjectPtr<URecallAIDAsset> Asset;
		FRecallAIDState State;
		FRandomStream RandomStream{ 1234 };

		FTestSetup(int32 GroupCount, ERecallAIDSpawnRule Rule)
		{
			Asset.Reset(NewObject<URecallAIDAsset>(GetTransientPackage()));
			State.StateName = TEXT("Test");
			State.SpawnRule = Rule;
			for (int32 GroupIndex = 0; GroupIndex < GroupCount; GroupIndex++)
			{
				FRecallAIDSpawnGroup Group;
				Group.GroupName = *FString::Printf(TEXT("Group%d"), GroupIndex);
				Group.InitialTimerRange = FVariableCollectionFloatRange(InitialDelay, InitialDelay);
				Group.SpawnIntervalRange = FVariableCollectionFloatRange(IntervalDelay, IntervalDelay);
				Asset->SpawnGroups.Add(Group);

				FRecallAIDStateGroupEntry Entry;
				Entry.SpawnGroupName = Group.GroupName;
				State.SpawnGroupEntries.Add(Entry);
			}
		}

		FSpawnRuleContext MakeRule(float DeltaTime) const
		{
			return FSpawnRuleContext{ *Asset, &State, nullptr, RandomStream, DeltaTime };
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRecallAIDInitializeTimerSetTest,
	"Recall.AID.SpawnRules.InitializeTimerSet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FRecallAIDInitializeTimerSetTest::RunTest(const FString& Parameters)
{
	FTestSetup Setup(2, ERecallAIDSpawnRule::Parallel);

	FRecallAIDSpawnTimerSet Timers;
	InitializeTimerSet(*Setup.Asset, Setup.RandomStream, Timers);

	TestEqual(TEXT("One instance per spawn group"), Timers.GroupInstances.Num(), 2);
	TestEqual(TEXT("Group 0 starts at its initial delay"), Timers.GroupInstances[0].SpawnTimer, InitialDelay);
	TestEqual(TEXT("Group 1 starts at its initial delay"), Timers.GroupInstances[1].SpawnTimer, InitialDelay);
	TestFalse(TEXT("No Random Weight group is pending yet"), Timers.IsSpawnTimerInitialized());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRecallAIDParallelRuleTest,
	"Recall.AID.SpawnRules.Parallel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FRecallAIDParallelRuleTest::RunTest(const FString& Parameters)
{
	FTestSetup Setup(2, ERecallAIDSpawnRule::Parallel);

	FRecallAIDSpawnTimerSet Timers;
	InitializeTimerSet(*Setup.Asset, Setup.RandomStream, Timers);
	Timers.GroupInstances[0].SpawnTimer = 0.0f;   // ready
	Timers.GroupInstances[1].SpawnTimer = 10.0f;  // not ready

	TArray<int32> SpawnedGroups;
	const FMassEntityHandle Director;
	ProcessParallelRule(Setup.MakeRule(1.0f), Timers, nullptr, Director,
		[&SpawnedGroups](int32 GroupIndex, const FSpawnGroupConfiguration&)
		{
			SpawnedGroups.Add(GroupIndex);
			return true;
		});

	TestEqual(TEXT("Only the ready group spawns"), SpawnedGroups.Num(), 1);
	TestTrue(TEXT("The ready group is group 0"), SpawnedGroups.Contains(0));
	TestTrue(TEXT("The spawned group is flagged as having spawned"), Timers.GroupInstances[0].bHasSpawned);
	TestEqual(TEXT("The spawned group rolls its interval"), Timers.GroupInstances[0].SpawnTimer, IntervalDelay);
	TestEqual(TEXT("The other group keeps counting down"), Timers.GroupInstances[1].SpawnTimer, 9.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRecallAIDParallelRuleRetryTest,
	"Recall.AID.SpawnRules.ParallelRetriesWhenSpawnRefused",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FRecallAIDParallelRuleRetryTest::RunTest(const FString& Parameters)
{
	FTestSetup Setup(1, ERecallAIDSpawnRule::Parallel);

	FRecallAIDSpawnTimerSet Timers;
	InitializeTimerSet(*Setup.Asset, Setup.RandomStream, Timers);
	Timers.GroupInstances[0].SpawnTimer = 0.0f;

	// A refused spawn (e.g. mob limit reached) must keep the timer at zero so it retries next frame.
	const FMassEntityHandle Director;
	ProcessParallelRule(Setup.MakeRule(1.0f), Timers, nullptr, Director,
		[](int32, const FSpawnGroupConfiguration&) { return false; });

	TestEqual(TEXT("Timer stays at zero"), Timers.GroupInstances[0].SpawnTimer, 0.0f);
	TestFalse(TEXT("Group is not flagged as spawned"), Timers.GroupInstances[0].bHasSpawned);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRecallAIDPerSpawnerIndependenceTest,
	"Recall.AID.SpawnRules.PerSpawnerTimersAreIndependent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FRecallAIDPerSpawnerIndependenceTest::RunTest(const FString& Parameters)
{
	FTestSetup Setup(1, ERecallAIDSpawnRule::Parallel);

	// Two spawners of one director, each with its own timer set (as the PerSpawner scope does).
	FRecallAIDFragment Director;
	const FMassEntityHandle SpawnerA(1, 1);
	const FMassEntityHandle SpawnerB(2, 1);
	InitializeTimerSet(*Setup.Asset, Setup.RandomStream, Director.SpawnerTimers.Add(SpawnerA));
	InitializeTimerSet(*Setup.Asset, Setup.RandomStream, Director.SpawnerTimers.Add(SpawnerB));
	Director.SpawnerTimers[SpawnerB].GroupInstances[0].SpawnTimer = 20.0f; // B was seeded with a longer delay

	int32 SpawnCountA = 0;
	int32 SpawnCountB = 0;
	const FMassEntityHandle DirectorEntity;
	for (int32 Frame = 0; Frame < 5; Frame++)
	{
		ProcessParallelRule(Setup.MakeRule(1.0f), Director.SpawnerTimers[SpawnerA], nullptr, DirectorEntity,
			[&SpawnCountA](int32, const FSpawnGroupConfiguration&) { SpawnCountA++; return true; });
		ProcessParallelRule(Setup.MakeRule(1.0f), Director.SpawnerTimers[SpawnerB], nullptr, DirectorEntity,
			[&SpawnCountB](int32, const FSpawnGroupConfiguration&) { SpawnCountB++; return true; });
	}

	TestEqual(TEXT("Spawner A reached its initial delay and spawned once"), SpawnCountA, 1);
	TestEqual(TEXT("Spawner A re-rolled its own interval"),
		Director.SpawnerTimers[SpawnerA].GroupInstances[0].SpawnTimer, IntervalDelay);
	TestEqual(TEXT("Spawner B has not spawned"), SpawnCountB, 0);
	TestEqual(TEXT("Spawner B timer was not affected by A's spawn"),
		Director.SpawnerTimers[SpawnerB].GroupInstances[0].SpawnTimer, 15.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRecallAIDRandomWeightRuleTest,
	"Recall.AID.SpawnRules.RandomWeight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FRecallAIDRandomWeightRuleTest::RunTest(const FString& Parameters)
{
	FTestSetup Setup(1, ERecallAIDSpawnRule::RandomWeight);

	FRecallAIDSpawnTimerSet Timers;
	InitializeTimerSet(*Setup.Asset, Setup.RandomStream, Timers);

	int32 SpawnCount = 0;
	const FMassEntityHandle Director;
	const auto TrySpawn = [&SpawnCount](int32, const FSpawnGroupConfiguration&) { SpawnCount++; return true; };

	// First frame selects the group and rolls its initial timer; it then counts down 1s.
	ProcessRandomWeightRule(Setup.MakeRule(1.0f), Timers, nullptr, Director, TrySpawn);
	TestEqual(TEXT("The single group is selected"), Timers.PendingGroupIndex, 0);
	TestEqual(TEXT("Nothing spawned yet"), SpawnCount, 0);
	TestEqual(TEXT("Timer counted down from the initial delay"), Timers.GroupInstances[0].SpawnTimer, InitialDelay - 1.0f);

	for (int32 Frame = 0; Frame < 4; Frame++)
	{
		ProcessRandomWeightRule(Setup.MakeRule(1.0f), Timers, nullptr, Director, TrySpawn);
	}
	TestEqual(TEXT("Spawned once the timer reached zero"), SpawnCount, 1);
	TestTrue(TEXT("Group is flagged as having spawned"), Timers.GroupInstances[0].bHasSpawned);
	TestEqual(TEXT("Next event is scheduled with the interval"), Timers.GroupInstances[0].SpawnTimer, IntervalDelay);
	TestEqual(TEXT("The next group is already pending"), Timers.PendingGroupIndex, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRecallAIDBurstTest,
	"Recall.AID.SpawnRules.BurstCompletesPerTimerSet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FRecallAIDBurstTest::RunTest(const FString& Parameters)
{
	FTestSetup Setup(1, ERecallAIDSpawnRule::Parallel);
	Setup.State.bBurstMode = true;
	Setup.State.BurstDuration = 2.0f;

	FRecallAIDSpawnTimerSet TimersA;
	FRecallAIDSpawnTimerSet TimersB;
	InitializeTimerSet(*Setup.Asset, Setup.RandomStream, TimersA);
	InitializeTimerSet(*Setup.Asset, Setup.RandomStream, TimersB);

	UpdateBurstMode(Setup.MakeRule(1.0f), TimersA);
	UpdateBurstMode(Setup.MakeRule(1.0f), TimersA);
	UpdateBurstMode(Setup.MakeRule(1.0f), TimersB);

	TestTrue(TEXT("Set A completed its 2s burst"), TimersA.GroupInstances[0].bBurstCompleted);
	TestFalse(TEXT("Set B (only 1s elapsed) did not"), TimersB.GroupInstances[0].bBurstCompleted);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRecallAIDIntervalBoundsTest,
	"Recall.AID.SpawnRules.IntervalBoundModifiers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FRecallAIDIntervalBoundsTest::RunTest(const FString& Parameters)
{
	FTestSetup Setup(1, ERecallAIDSpawnRule::Parallel);
	Setup.Asset->SpawnGroups[0].SpawnIntervalRange = FVariableCollectionFloatRange(10.0f, 20.0f);
	FRecallAIDSpawnGroup& Group = Setup.Asset->SpawnGroups[0];

	FFloatInterval Bounds = GetSpawnIntervalBounds(Group, nullptr);
	TestEqual(TEXT("No modifiers: lower bound is authored"), Bounds.Min, 10.0f);
	TestEqual(TEXT("No modifiers: upper bound is authored"), Bounds.Max, 20.0f);

	// Each bound has its own modifiers, and several modifiers on one bound multiply.
	Group.SpawnIntervalMinModifiers.Add(FInstancedStruct::Make(FRecallAIDTestFixedModifier(0.5f)));
	Group.SpawnIntervalMaxModifiers.Add(FInstancedStruct::Make(FRecallAIDTestFixedModifier(2.0f)));
	Group.SpawnIntervalMaxModifiers.Add(FInstancedStruct::Make(FRecallAIDTestFixedModifier(1.5f)));
	Bounds = GetSpawnIntervalBounds(Group, nullptr);
	TestEqual(TEXT("Lower bound uses only the min modifiers"), Bounds.Min, 5.0f);
	TestEqual(TEXT("Upper bound uses only the max modifiers"), Bounds.Max, 60.0f);

	// Crossing bounds: the upper bound is raised to the lower bound.
	Group.SpawnIntervalMinModifiers.Reset();
	Group.SpawnIntervalMaxModifiers.Reset();
	Group.SpawnIntervalMinModifiers.Add(FInstancedStruct::Make(FRecallAIDTestFixedModifier(3.0f)));
	Bounds = GetSpawnIntervalBounds(Group, nullptr);
	TestEqual(TEXT("Crossed: lower bound is modified"), Bounds.Min, 30.0f);
	TestEqual(TEXT("Crossed: upper bound is raised to the lower bound"), Bounds.Max, 30.0f);

	// Negative results clamp to zero; a non-finite (NaN) result keeps the authored bound.
	Group.SpawnIntervalMinModifiers.Reset();
	Group.SpawnIntervalMinModifiers.Add(FInstancedStruct::Make(FRecallAIDTestFixedModifier(-1.0f)));
	Group.SpawnIntervalMaxModifiers.Add(FInstancedStruct::Make(FRecallAIDTestFixedModifier(NAN)));
	Bounds = GetSpawnIntervalBounds(Group, nullptr);
	TestEqual(TEXT("Negative lower bound clamps to zero"), Bounds.Min, 0.0f);
	TestEqual(TEXT("Non-finite upper bound keeps the authored value"), Bounds.Max, 20.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRecallAIDIntervalModifierRollTest,
	"Recall.AID.SpawnRules.IntervalModifiersAffectRolledTimers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FRecallAIDIntervalModifierRollTest::RunTest(const FString& Parameters)
{
	const FMassEntityHandle Director;
	const auto AlwaysSpawn = [](int32, const FSpawnGroupConfiguration&) { return true; };
	const auto Halve = []() { return FInstancedStruct::Make(FRecallAIDTestFixedModifier(0.5f)); };

	{
		FTestSetup Setup(1, ERecallAIDSpawnRule::Parallel);
		Setup.Asset->SpawnGroups[0].SpawnIntervalMinModifiers.Add(Halve());
		Setup.Asset->SpawnGroups[0].SpawnIntervalMaxModifiers.Add(Halve());

		FRecallAIDSpawnTimerSet Timers;
		InitializeTimerSet(*Setup.Asset, Setup.RandomStream, Timers);
		Timers.GroupInstances[0].SpawnTimer = 0.0f;
		ProcessParallelRule(Setup.MakeRule(1.0f), Timers, nullptr, Director, AlwaysSpawn);
		TestEqual(TEXT("Parallel: interval after a spawn uses the modified bounds"),
			Timers.GroupInstances[0].SpawnTimer, IntervalDelay * 0.5f);
	}

	{
		FTestSetup Setup(1, ERecallAIDSpawnRule::RandomWeight);
		Setup.Asset->SpawnGroups[0].SpawnIntervalMinModifiers.Add(Halve());
		Setup.Asset->SpawnGroups[0].SpawnIntervalMaxModifiers.Add(Halve());

		FRecallAIDSpawnTimerSet Timers;
		InitializeTimerSet(*Setup.Asset, Setup.RandomStream, Timers);
		Timers.GroupInstances[0].SpawnTimer = 0.0f;
		Timers.PendingGroupIndex = 0;
		ProcessRandomWeightRule(Setup.MakeRule(1.0f), Timers, nullptr, Director, AlwaysSpawn);
		TestEqual(TEXT("Random Weight: the next event uses the modified bounds"),
			Timers.GroupInstances[0].SpawnTimer, IntervalDelay * 0.5f);
	}

	{
		// The first spawn is governed by Initial Timer, which the interval modifiers do not touch.
		FTestSetup Setup(1, ERecallAIDSpawnRule::Parallel);
		Setup.Asset->SpawnGroups[0].SpawnIntervalMinModifiers.Add(Halve());
		Setup.Asset->SpawnGroups[0].SpawnIntervalMaxModifiers.Add(Halve());

		FRecallAIDSpawnTimerSet Timers;
		InitializeTimerSet(*Setup.Asset, Setup.RandomStream, Timers);
		TestEqual(TEXT("Initial delay is not modified"), Timers.GroupInstances[0].SpawnTimer, InitialDelay);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
