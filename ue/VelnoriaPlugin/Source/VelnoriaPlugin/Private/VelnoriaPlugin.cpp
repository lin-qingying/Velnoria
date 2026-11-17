#include "VelnoriaPlugin.h"

#include "Modules/ModuleManager.h"

void FVelnoriaPluginModule::StartupModule()
{
    // Slice S4: register the snapshot consumer and the meshing scheduler here.
    // Intentionally empty until the core library exposes its snapshot queue.
}

void FVelnoriaPluginModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FVelnoriaPluginModule, VelnoriaPlugin)
