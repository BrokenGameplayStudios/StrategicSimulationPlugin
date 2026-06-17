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
    UMissionGroup* StartMission(UStrategyBase* OriginBase, TArray<UStrategyVehicle*> Vehicles, int32 DurationDays, const TArray<UStrategySoldier*>& SoldiersToAssign, EMissionType MissionType = EMissionType::Offensive, EFactionType AttackingFaction = EFactionType::Enemy, float ScheduledLaunchGameHours = -1.f);

    /** Schedule one mission per idle vehicle at a base, evenly spaced across the 24-hour day */
    UFUNCTION(BlueprintCallable, Category = "Mission|Schedule")
    int32 ScheduleVehicleMissionsForBase(UStrategyBase* Base, EFactionType Faction, EMissionType MissionType = EMissionType::Recon);

    /** Schedule per-vehicle mission types (array must match idle vehicle count) */
    int32 ScheduleVehicleMissionsForBase(UStrategyBase* Base, EFactionType Faction, const TArray<EMissionType>& PerVehicleMissionTypes);

    /** Returns true if the vehicle has an in-range enemy base it could attack */
    UFUNCTION(BlueprintCallable, Category = "Mission|Schedule")
    bool HasOffensiveTargetInRange(UStrategyVehicle* Vehicle) const;

    /** Parked idle vehicles at a base that are ready to fly today */
    UFUNCTION(BlueprintCallable, Category = "Mission|Schedule")
    TArray<UStrategyVehicle*> GatherIdleVehiclesAtBase(UStrategyBase* Base) const;

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

    /** Called when a vehicle is destroyed during live vehicular combat */
    void HandleVehicleDestroyedInCombat(UStrategyVehicle* Vehicle);

    /** Log placeholder when an Offensive mission reaches the target base */
    void HandleBaseAttackArrival(UStrategyVehicle* Vehicle, UMissionGroup* Mission);

private:
    /** Calculates overall fleet combat effectiveness (0–100) using soldier effective stats + vehicle health. */
    float CalculateFleetEffectiveness(const UMissionGroup* Mission) const;

    void ResolveMissionOutcome(UMissionGroup* Mission);

    bool TryPickMissionTarget(UStrategyVehicle* Vehicle, EMissionType MissionType, FVector2D& OutTarget,
        TSet<class UStrategySiteDefinition*>& InOutReservedSites, UStrategyBase** OutTargetBase = nullptr) const;

    void GetMapBounds(float& OutWidth, float& OutHeight, float& OutPadding) const;
    void CollectSitesTargetedByActiveMissions(TSet<class UStrategySiteDefinition*>& OutSites, const UMissionGroup* IgnoreMission = nullptr) const;
    class UStrategySiteDefinition* FindSiteAtLocation(const FVector2D& Location, float Tolerance = 25.f) const;
    FVector2D PickPatrolPointWithinRange(const FVector2D& Origin, float MaxRoundTripRange, float MinX, float MinY, float MaxX, float MaxY) const;
    static bool IsValidMapLocation(const FVector2D& Location, float MinX, float MinY, float MaxX, float MaxY);

    float ComputeEvenlySpacedLaunchHour(int32 SlotIndex, int32 TotalSlots) const;
    void PrepareVehiclesForDeparture(UMissionGroup* Mission);
    void ProcessPendingMissionLaunches(float CurrentHours);

    bool IsVehicleCommittedToAnyMission(UStrategyVehicle* Vehicle, const UMissionGroup* IgnoreMission = nullptr) const;
    bool IsVehicleReadyForMissionLaunch(UStrategyVehicle* Vehicle, const UMissionGroup* Mission) const;
    void CancelStaleDeferredMissions(int32 CurrentSimulationDay);
};