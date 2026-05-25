#pragma once

#include "Modules/ModuleManager.h"

class FStrategicSimulationPluginModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};