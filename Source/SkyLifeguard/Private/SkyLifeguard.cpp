// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkyLifeguard.h"

#include "LifeFloodlight.h"

#define LOCTEXT_NAMESPACE "FSkyLifeguardModule"

void FSkyLifeguardModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	FLifeDomainErrorFloodlight::FConfig Config;
	Config.MaxBudget = 15;
	Config.WarningCost = 1;
	Config.ErrorCost = 3;
	Config.FlashDuration = 2.0f;
	
	FLifeDomainErrorFloodlight::Initialize(Config);
}

void FSkyLifeguardModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FSkyLifeguardModule, SkyLifeguard)