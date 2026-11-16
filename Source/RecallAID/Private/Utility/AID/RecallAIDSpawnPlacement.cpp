// Copyright (C) 2024 Van de Walle Bastien
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0

#include "RecallAIDSpawnInternal.h"

#include "AID/RecallAIDSpawnPointSettings.h"
#include "Desync/RecallDesyncLog.h"
#include "Entity/RecallAIDSpawnCommand.h"
#include "NavigationSystem.h"
#include "System/Entity/RecallEntityAsyncSpawnSubsystem.h"
#include "System/Entity/RecallEntityAsyncSpawnTypes.h"
#include "System/Random/RecallRandomNumberSubsystem.h"

/**
 * Creates spawn parameters for entity spawning.
 */
static FRecallEntityAsyncSpawnParameters CreateSpawnParameters(
	const FMassEntityHandle& AIDEntity,
	int32 EntityCount)
{
	FRecallEntityAsyncSpawnParameters SpawnParams;
	SpawnParams.EntityCount = EntityCount;
	SpawnParams.SpawnCommand.InitializeAs<FRecallAIDSpawnCommand>();

	FRecallAIDSpawnCommand& SpawnCommand = SpawnParams.SpawnCommand.GetMutable<FRecallAIDSpawnCommand>();
	SpawnCommand.SpawnInfos.SetNum(SpawnParams.EntityCount);
	SpawnCommand.AIDEntity = AIDEntity;

	return SpawnParams;
}

/**
 * Calculates spawn positions for entities within a spawn area.
 */
static void CalculateSpawnPositions(
	const FRecallAIDSpawnGroup& SpawnGroup,
	const FRecallAIDSpawnPointCache& SpawnPoint,
	const FRandomStream& RandomStream,
	UNavigationSystemV1* NavSys,
	TArray<FRecallAIDSpawnInfo>& OutSpawnInfos)
{
	const float SpawnArea = SpawnPoint.SpawnPointSettings->SpawnArea;
	const FBox SpawnBox(SpawnPoint.Location - FVector(SpawnArea, SpawnArea, 0.0f),
		SpawnPoint.Location + FVector(SpawnArea, SpawnArea, 1.0f));

	for (int32 EntityIndex = 0; EntityIndex < OutSpawnInfos.Num(); EntityIndex++)
	{
		FRecallAIDSpawnInfo& SpawnInfo = OutSpawnInfos[EntityIndex];

		FNavLocation NavLocation;
		const FVector RandomPosition = RandomStream.RandPointInBox(SpawnBox);
		if (!SpawnGroup.bProjectOntoNavigation)
		{
			SpawnInfo.Position = RandomPosition;
		}
		else if (NavSys && NavSys->ProjectPointToNavigation(RandomPosition, NavLocation))
		{
			SpawnInfo.Position = NavLocation.Location;
		}
		else
		{
			SpawnInfo.Position = SpawnPoint.Location;
		}
	}
}

int32 Recall::AID::Utils::Private::SpawnEnemyGroupAt(
	const FRecallAIDSpawnContext& SpawnContext,
	const FMassEntityHandle& AIDEntity,
	const FRecallAIDSpawnGroup& SpawnGroup,
	const FRecallAIDSpawnPointCache& SpawnPoint,
	int32 EntityCount,
	float CurrentTime)
{
	if (EntityCount == 0)
	{
		return 0;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(&SpawnContext.World);
	const FRandomStream& RandomStream = SpawnContext.RandomNumberSystem.GetRandomStream();

	// Create spawn parameters
	FRecallEntityAsyncSpawnParameters SpawnParams = CreateSpawnParameters(AIDEntity, EntityCount);
	FRecallAIDSpawnCommand& SpawnCommand = SpawnParams.SpawnCommand.GetMutable<FRecallAIDSpawnCommand>();

	// Calculate spawn positions
	CalculateSpawnPositions(SpawnGroup, SpawnPoint, RandomStream, NavSys, SpawnCommand.SpawnInfos);

#if RECALL_DESYNC_LOG
	for (int32 SpawnInfoIndex = 0; SpawnInfoIndex < SpawnCommand.SpawnInfos.Num(); SpawnInfoIndex++)
	{
		RECALL_DESYNC_LOG_STR(&SpawnContext.World, AIDSpawnEnemyGroupPosition,
			FString::Printf(TEXT("%s SpawnInfoIndex: %d, Position: %s"),
			*AIDEntity.DebugGetDescription(), SpawnInfoIndex, *SpawnCommand.SpawnInfos[SpawnInfoIndex].Position.ToString()));
	}
#endif // RECALL_DESYNC_LOG

	// Spawn entities
	SpawnContext.EntityAsyncSpawnSystem.SpawnEntityAsync(SpawnGroup.EntityConfigAsset,
		SpawnPoint.Location, SpawnPoint.Rotation, SpawnParams);

	// Update the spawn point's last spawn time to start its cooldown
	if (SpawnPoint.SpawnPointFragment)
	{
		SpawnPoint.SpawnPointFragment->LastSpawnTime = CurrentTime;
	}

	return EntityCount;
}

int32 Recall::AID::Utils::SpawnEnemyGroup(
	const FRecallAIDSpawnContext& SpawnContext,
	const FMassEntityHandle& AIDEntity,
	const FRecallAIDSpawnGroup& SpawnEnemyGroup,
	const TArray<FRecallAIDSpawnPointCache>& SpawnPoints,
	int32 EntityCount,
	float CurrentTime)
{
	// TODO: Filter spawn points based on gameplay tags to support different spawn zones

	if (SpawnPoints.Num() == 0 || EntityCount == 0)
	{
		return 0;
	}

	// Select spawn point. This RNG call must stay before the position rolls (determinism).
	const FRandomStream& RandomStream = SpawnContext.RandomNumberSystem.GetRandomStream();
	const int32 SpawnPointIndex = RandomStream.RandRange(0, SpawnPoints.Num() - 1);
	const FRecallAIDSpawnPointCache& SpawnPoint = SpawnPoints[SpawnPointIndex];

#if RECALL_DESYNC_LOG
	RECALL_DESYNC_LOG_STR(&SpawnContext.World, AIDSpawnEnemyGroup,
		FString::Printf(TEXT("%s EntityCount: %d, CurrentTime: %f, SpawnPointIndex: %d, SpawnPointCount: %d, SpawnPointLocation: %s"),
		*AIDEntity.DebugGetDescription(), EntityCount, CurrentTime, SpawnPointIndex, SpawnPoints.Num(), *SpawnPoint.Location.ToString()));
#endif // RECALL_DESYNC_LOG

	return Private::SpawnEnemyGroupAt(SpawnContext, AIDEntity, SpawnEnemyGroup, SpawnPoint, EntityCount, CurrentTime);
}
