#include "LumenvoxPlugin.h"

#include "Modules/ModuleManager.h"

void FLumenvoxPluginModule::StartupModule()
{
    // Slice S4: register the snapshot consumer and the meshing scheduler here.
    // Intentionally empty until the core library exposes its snapshot queue.
}

void FLumenvoxPluginModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FLumenvoxPluginModule, LumenvoxPlugin)
