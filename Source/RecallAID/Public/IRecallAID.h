// Copyright (C) 2024 Van de Walle Bastien
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogRecallAID, Log, All);

/**
 * The public interface to the RecallAID module.
 * This module provides AI Director functionality for managing entity spawning
 * based on configurable states and spawn groups.
 */
class IRecallAID : public IModuleInterface
{
public:
	/**
	 * Singleton-like access to this module's interface.
	 * Beware of calling this during the shutdown phase, though. Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IRecallAID& Get()
	{
		return FModuleManager::LoadModuleChecked<IRecallAID>("RecallAID");
	}

	/**
	 * Checks to see if this module is loaded and ready.
	 * It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("RecallAID");
	}
};
