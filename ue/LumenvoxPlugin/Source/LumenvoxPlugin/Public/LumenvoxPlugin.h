#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

// Presentation layer entry point.
//
// Responsibilities (and hard limits) of this module:
//   - consumes immutable chunk snapshots published by the core
//   - performs meshing on worker threads, submits on the game thread
//   - owns camera, input translation, HUD, audio
//   - never holds mutable game state, never decides game outcomes
//
// See docs/design/vertical-slice-01.md section 5 for the snapshot contract.
class FLumenvoxPluginModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
