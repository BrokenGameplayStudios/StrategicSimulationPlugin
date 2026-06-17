#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UMissionGroup.h"
#include "UMissionManagerSubsystem.generated.h"

class UResourceManagerSubsystem;
class USoldierManagerSubsystem;
class UStrategyBase;
class UStrategyVehicle;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMissionCompleted, UMissionGroup*, Mission);

UCLASS()
class STRATEGICSIMULATIONPLUGIN_API UMissionManagerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    // Update: Added SoldiersToAssign parameter to StartMission for better soldier management and assignment during mission launch
    UFUNCTION(BlueprintCallable, Category = "Mission")
    UMissionGroup* StartMission(UStrategyBase* OriginBase, TArray<UStrategyVehicle*> Vehicles, int32 DurationDays, const TArray<UStrategySoldier*>& SoldiersToAssign, EMissionType MissionType = EMissionType::Offensive, EFactionType AttackingFaction = EFactionType::Enemy);

    UFUNCTION(BlueprintCallable, Category = "Mission")
    void SimulateOneDay();

    UPROPERTY(BlueprintAssignable, Category = "Mission")
    FOnMissionCompleted OnMissionCompleted;

    /** Launches a mission from a base. Pass VehiclesOverride to launch a specific subset; empty = all parked vehicles. */
    UFUNCTION(BlueprintCallable, Category = "Mission", meta = (AutoCreateRefTerm = "VehiclesOverride"))
    UMissionGroup* LaunchMissionFromBase(UStrategyBase* OriginBase, int32 DurationDays, EMissionType MissionType, const TArray<UStrategyVehicle*>& VehiclesOverride);

    UPROPERTY(VisibleAnywhere, Transient, Category = "Missions")
    TArray<UMissionGroup*> ActiveMissions;

    UFUNCTION()
    void OnDayPassed(int32 NewDay);

    /** Helper getters (required by the .cpp) */
    UResourceManagerSubsystem* GetResourceManager() const;
    USoldierManagerSubsystem* GetSoldierManager() const;

    // ===========================================================================
    // NEW: Live movement integration
    // ===========================================================================
    UFUNCTION(BlueprintCallable, Category = "Mission|Live Movement")
    float GetCurrentGameHours() const;

    UFUNCTION(BlueprintCallable, Category = "Mission|Live Movement")
    void ActivateLiveMovementForVehicles(UMissionGroup* Mission, EMissionType MissionType);

    // ===========================================================================
    // NEW: Live movement integration (keep all vehicles updated)
    // ===========================================================================
    UFUNCTION(BlueprintCallable, Category = "Mission|Live Movement")
    void UpdateAllLiveVehicles(float DeltaGameHours);

private:
    /** Calculates overall fleet combat effectiveness (0–100) using soldier effective stats + vehicle health. */
    float CalculateFleetEffectiveness(const UMissionGroup* Mission) const;

    void ResolveMissionOutcome(UMissionGroup* Mission);

    FVector2D PickMissionTarget(UStrategyVehicle* Vehicle, EMissionType MissionType) const;
    void GetMapBounds(float& OutWidth, float& OutHeight, float& OutPadding) const;
};