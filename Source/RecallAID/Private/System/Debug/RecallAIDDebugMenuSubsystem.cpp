// Copyright (C) 2024 Van de Walle Bastien
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0

#include "RecallAIDDebugMenuSubsystem.h"

#ifdef WITH_DEBUG_MENU
#include "Debug/DebugMenuInterface.h"
#include "System/Debug/DebugMenuSubsystem.h"
#endif // WITH_DEBUG_MENU

void URecallAIDDebugMenuSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

#ifdef WITH_DEBUG_MENU
	Collection.InitializeDependency<UDebugMenuSubsystem>();
	DebugMenuSubsystem = UGameInstance::GetSubsystem<UDebugMenuSubsystem>(GetGameInstance());
	if (DebugMenuSubsystem.IsValid())
	{
		CreateDebugMenuItems(DebugMenuSubsystem->GetMutableDebugMenu());
	}
#endif // WITH_DEBUG_MENU
}

void URecallAIDDebugMenuSubsystem::Deinitialize()
{
	Super::Deinitialize();

#ifdef WITH_DEBUG_MENU
	DebugMenuSubsystem.Reset();
#endif // WITH_DEBUG_MENU
}

void URecallAIDDebugMenuSubsystem::CreateDebugMenuItems(IDebugMenu& DebugMenu)
{
#ifdef WITH_DEBUG_MENU
	DebugMenu.AddItem_Bool(TEXT("AID"), "Show AID On Screen", false, TEXT("Recall.AID.DebugOnScreen"));
	DebugMenu.AddItem_Bool(TEXT("AID"), "Show AID In World", false, TEXT("Recall.AID.DebugInWorld"));
#endif // WITH_DEBUG_MENU
}
