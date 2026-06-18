#include "StrategicSimulationPlugin.h"
#include "Modules/ModuleManager.h"

/** Module startup hook invoked when the plugin is loaded. */
void FStrategicSimulationPluginModule::StartupModule()
{
    UE_LOG(LogTemp, Display, TEXT("StrategicSimulationPlugin — Module started successfully!"));
}

/** Module shutdown hook invoked when the plugin is unloaded. */
void FStrategicSimulationPluginModule::ShutdownModule()
{
    UE_LOG(LogTemp, Display, TEXT("StrategicSimulationPlugin — Module shut down."));
}

IMPLEMENT_MODULE(FStrategicSimulationPluginModule, StrategicSimulationPlugin)