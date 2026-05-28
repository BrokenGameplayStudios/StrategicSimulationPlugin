#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UMissionGroup.h"
#include "UMissionManagerSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMissionCompleted, UMissionGroup*, Mission);

UCLASS()
class STRATEGICSIMULATIONPLUGIN_API UMissionManagerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    UFUNCTION(BlueprintCallable, Category = "Mission")
    UMissionGroup* StartMission(UStrategyBase* OriginBase, TArray<UStrategyVehicle*> Vehicles, int32 DurationDays);

    UFUNCTION(BlueprintCallable, Category = "Mission")
    void SimulateOneDay();

    UPROPERTY(BlueprintAssignable, Category = "Mission")
    FOnMissionCompleted OnMissionCompleted;

    /** Launches a simple mission with all parked vehicles in a base (for testing/AI) */
    UFUNCTION(BlueprintCallable, Category = "Mission")
    UMissionGroup* LaunchMissionFromBase(UStrategyBase* OriginBase, int32 DurationDays = 15);

    UFUNCTION()
    void OnDayPassed(int32 NewDay);

private:
    UPROPERTY(VisibleAnywhere, Transient, Category = "Missions")
    TArray<UMissionGroup*> ActiveMissions;
};