#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "StrategicSimulationTypes.h"
#include "UProductionManagerSubsystem.generated.h"

UCLASS()
class STRATEGICSIMULATIONPLUGIN_API UProductionManagerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    UFUNCTION(BlueprintCallable, Category = "Production")
    void CompleteJob(FProductionJob Job, UStrategyFacility* Facility);

private:
    void CompleteSoldierJob(const FProductionJob& Job, UStrategyFacility* Facility);
    void CompleteVehicleJob(const FProductionJob& Job, UStrategyFacility* Facility);
    void CompleteFacilityJob(const FProductionJob& Job, UStrategyFacility* Facility);
    void CompleteResearchJob(const FProductionJob& Job, UStrategyFacility* Facility);
};