// Copyright (C) 2024 Van de Walle Bastien
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0


#include "CoreMinimal.h"
#include "IRecallAID.h"

DEFINE_LOG_CATEGORY(LogRecallAID);

class FRecallAID : public IRecallAID
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE(FRecallAID, RecallAID)

void FRecallAID::StartupModule()
{
}

void FRecallAID::ShutdownModule()
{
}