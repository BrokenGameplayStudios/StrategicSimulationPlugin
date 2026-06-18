#pragma once

#include "Modules/ModuleManager.h"

/** Unreal module entry for the Strategic Simulation plugin. */
class FStrategicSimulationPluginModule : public IModuleInterface
{
public:
    /** Logs module load; subsystems register via UGameInstanceSubsystem. */
    virtual void StartupModule() override;
    /** Logs module unload on editor or game shutdown. */
    virtual void ShutdownModule() override;
};