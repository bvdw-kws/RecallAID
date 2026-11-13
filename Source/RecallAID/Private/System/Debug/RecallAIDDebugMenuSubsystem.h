// Copyright (C) 2024 Van de Walle Bastien
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "Subsystems/GameInstanceSubsystem.h"

#include "RecallAIDDebugMenuSubsystem.generated.h"

class IDebugMenu;

/** Registers the AI Director debug console variables in the debug menu. */
UCLASS()
class URecallAIDDebugMenuSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
#ifdef WITH_DEBUG_MENU
	TWeakObjectPtr<class UDebugMenuSubsystem> DebugMenuSubsystem;
#endif // WITH_DEBUG_MENU

	void CreateDebugMenuItems(IDebugMenu& DebugMenu);
};
