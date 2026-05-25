#include "StrategicSimulationPlugin.h"
#include "Modules/ModuleManager.h"

void FStrategicSimulationPluginModule::StartupModule()
{
    UE_LOG(LogTemp, Display, TEXT("StrategicSimulationPlugin — Module started successfully!"));
}

void FStrategicSimulationPluginModule::ShutdownModule()
{
    UE_LOG(LogTemp, Display, TEXT("StrategicSimulationPlugin — Module shut down."));
}

IMPLEMENT_MODULE(FStrategicSimulationPluginModule, StrategicSimulationPlugin)