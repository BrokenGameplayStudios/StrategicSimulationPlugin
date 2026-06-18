#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "StrategicSimulationTypes.h"
#include "UProductionManagerSubsystem.generated.h"

/** Game-instance subsystem that finalizes facility production jobs when their timers expire. */
UCLASS()
class STRATEGICSIMULATIONPLUGIN_API UProductionManagerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /** Registers the subsystem; job completion is driven by UStrategyFacility daily ticks. */
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /** Dispatches Job to the appropriate Complete* handler based on EProductionType. */
    UFUNCTION(BlueprintCallable, Category = "Production")
    void CompleteJob(FProductionJob Job, UStrategyFacility* Facility);

private:
    /** Finishes soldier training via USoldierManagerSubsystem::FinishSoldierTraining. */
    void CompleteSoldierJob(const FProductionJob& Job, UStrategyFacility* Facility);

    /** Spawns a completed vehicle and parks it in the facility hanger. */
    void CompleteVehicleJob(const FProductionJob& Job, UStrategyFacility* Facility);

    /** Marks the facility operational when its self-build job completes. */
    void CompleteFacilityJob(const FProductionJob& Job, UStrategyFacility* Facility);

    /** Broadcasts research completion events and updates the research list for the job's faction. */
    void CompleteResearchJob(const FProductionJob& Job, UStrategyFacility* Facility);
};