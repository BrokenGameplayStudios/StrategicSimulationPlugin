#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "StrategicSimulationTypes.h"
#include "UMissionGroup.h"
#include "UMissionManagerSubsystem.generated.h"

class UResourceManagerSubsystem;
class USoldierManagerSubsystem;
class UStrategyBase;
class UStrategyVehicle;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMissionCompleted, UMissionGroup*, Mission);

/**
 * Game-instance subsystem that schedules, launches, and resolves vehicle missions
 * including live movement, interception, salvage, and combat outcomes.
 */
UCLASS()
class STRATEGICSIMULATIONPLUGIN_API UMissionManagerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /** Initializes the mission manager subsystem. */
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /** Creates and optionally defers a mission; assigns soldiers and activates live movement when due. */
    UFUNCTION(BlueprintCallable, Category = "Mission")
    UMissionGroup* StartMission(UStrategyBase* OriginBase, TArray<UStrategyVehicle*> Vehicles, int32 DurationDays, const TArray<UStrategySoldier*>& SoldiersToAssign, EMissionType MissionType = EMissionType::Offensive, EFactionType AttackingFaction = EFactionType::Enemy, float ScheduledLaunchGameHours = -1.f);

    /** Schedule one mission per idle vehicle at a base, evenly spaced across the 24-hour day */
    UFUNCTION(BlueprintCallable, Category = "Mission|Schedule")
    int32 ScheduleVehicleMissionsForBase(UStrategyBase* Base, EFactionType Faction, EMissionType MissionType = EMissionType::Recon);

    /** Schedules one mission per idle vehicle using per-vehicle mission types (array must match idle count). */
    int32 ScheduleVehicleMissionsForBase(UStrategyBase* Base, EFactionType Faction, const TArray<EMissionType>& PerVehicleMissionTypes);

    /** Returns true if the vehicle has an in-range enemy base it could attack */
    UFUNCTION(BlueprintCallable, Category = "Mission|Schedule")
    bool HasOffensiveTargetInRange(UStrategyVehicle* Vehicle) const;

    /** Returns true when passive radar has a contact this vehicle can intercept. */
    UFUNCTION(BlueprintCallable, Category = "Mission|Schedule")
    bool HasInterceptionTargetFromContacts(UStrategyVehicle* Vehicle) const;

    /** Player/AI: launch Interception at a base-radar contact (immediate departure). */
    UFUNCTION(BlueprintCallable, Category = "Mission|Interception")
    bool LaunchInterceptionAtContact(UStrategyBase* OriginBase, UStrategyVehicle* Vehicle, FGuid ContactId);

    /** Player UI: pick nearest idle combat vehicle and launch interception at a contact. */
    UFUNCTION(BlueprintCallable, Category = "Mission|Interception")
    bool TryLaunchInterceptionAtContactAuto(EFactionType Faction, FGuid ContactId, UStrategyBase*& OutOriginBase,
        UStrategyVehicle*& OutVehicle);

    /** True when any idle combat vehicle at a faction base can reach this contact. */
    UFUNCTION(BlueprintPure, Category = "Mission|Interception")
    bool CanFactionInterceptContact(EFactionType Faction, FGuid ContactId) const;

    /** Returns true if the vehicle can reach a known active wreck for salvage */
    UFUNCTION(BlueprintCallable, Category = "Mission|Schedule")
    bool HasSalvageTargetInRange(UStrategyVehicle* Vehicle) const;

    /** Heuristic score for AI salvage prioritization (higher = better). */
    UFUNCTION(BlueprintPure, Category = "Mission|Salvage")
    float ComputeSalvageTargetScore(EFactionType Faction, const class UStrategySiteDefinition* Site,
        const FVector2D& Origin) const;

    /** Active + scheduled Salvage missions for a faction. */
    UFUNCTION(BlueprintPure, Category = "Mission|Salvage")
    int32 CountActiveSalvageMissions(EFactionType Faction) const;

    /** AI salvage gate: thresholds, caps, post-combat decline, and loser recovery rules. */
    bool EvaluateAISalvageScheduling(UStrategyVehicle* Vehicle, class UStrategySiteDefinition*& OutBestSite,
        float& OutBestScore) const;

    /** Logs known wreck salvage opportunities for AI debugging (PR-7). */
    void LogSalvageOpportunitiesForFaction(EFactionType Faction, int32 CurrentDay) const;

    /** Parked idle vehicles at a base that are ready to fly today */
    UFUNCTION(BlueprintCallable, Category = "Mission|Schedule")
    TArray<UStrategyVehicle*> GatherIdleVehiclesAtBase(UStrategyBase* Base) const;

    /** Advances non-live missions by one day and resolves completed ones. */
    UFUNCTION(BlueprintCallable, Category = "Mission")
    void SimulateOneDay();

    UPROPERTY(BlueprintAssignable, Category = "Mission")
    FOnMissionCompleted OnMissionCompleted;

    /** Launches a mission from a base. Pass VehiclesOverride to launch a specific subset; empty = all parked vehicles. */
    UFUNCTION(BlueprintCallable, Category = "Mission", meta = (AutoCreateRefTerm = "VehiclesOverride"))
    UMissionGroup* LaunchMissionFromBase(UStrategyBase* OriginBase, int32 DurationDays, EMissionType MissionType, const TArray<UStrategyVehicle*>& VehiclesOverride);

    UPROPERTY(VisibleAnywhere, Transient, Category = "Missions")
    TArray<UMissionGroup*> ActiveMissions;

    /** Bound to time manager; cancels stale missions and runs SimulateOneDay. */
    UFUNCTION()
    void OnDayPassed(int32 NewDay);

    /** Returns the game instance resource manager subsystem. */
    UResourceManagerSubsystem* GetResourceManager() const;
    /** Returns the game instance soldier manager subsystem. */
    USoldierManagerSubsystem* GetSoldierManager() const;

    /** Returns elapsed simulation hours from the time manager. */
    UFUNCTION(BlueprintCallable, Category = "Mission|Live Movement")
    float GetCurrentGameHours() const;

    /** Picks targets and begins live movement for each vehicle in the mission fleet. */
    UFUNCTION(BlueprintCallable, Category = "Mission|Live Movement")
    bool ActivateLiveMovementForVehicles(UMissionGroup* Mission, EMissionType MissionType);

    /** Ticks all live vehicles, processes deferred launches, and resolves completed missions. */
    UFUNCTION(BlueprintCallable, Category = "Mission|Live Movement")
    void UpdateAllLiveVehicles(float DeltaGameHours);

    /** Called when a vehicle is destroyed on any path (combat, crash, debug). Creates salvage wreck when enabled. */
    UFUNCTION(BlueprintCallable, Category = "Mission|Combat")
    void HandleVehicleDestroyed(UStrategyVehicle* Vehicle, UStrategyVehicle* DestroyedBy = nullptr);

    /** Log placeholder when an Offensive mission reaches the target base */
    void HandleBaseAttackArrival(UStrategyVehicle* Vehicle, UMissionGroup* Mission);

    /** True when an active mission already targets this site (waypoint within SiteMatchTolerance). */
    UFUNCTION(BlueprintPure, Category = "Mission|Sites")
    bool IsSiteTargetedByActiveMissions(const class UStrategySiteDefinition* Site,
        const UMissionGroup* IgnoreMission = nullptr) const;

    /** Clears in-flight missions and vehicle movement state before a site-map load (PR-4 QA path). */
    UFUNCTION(BlueprintCallable, Category = "Mission|Save")
    void ClearRuntimeMissionStateForSiteMapLoad();

    /** Forces a salvage mission fleet to withdraw (contest loser / abort outcome). */
    UFUNCTION(BlueprintCallable, Category = "Mission|Salvage Contest")
    void AbortSalvageMission(UMissionGroup* Mission, bool bReturnVehiclesHome = true);

    /**
     * Starts contested salvage resolution: pauses strategic clock, snapshots both fleets,
     * and broadcasts OnSalvageContestStarted. No-op if a contest is already active.
     */
    void BeginSalvageContest(UStrategySiteDefinition* Site, UMissionGroup* HumanMission, UMissionGroup* EnemyMission);

private:
    /** Short-term memory of combat wrecks for AI post-win salvage decline heuristics. */
    struct FCombatSalvageWreckRecord
    {
        FGuid SiteId;
        EFactionType WinnerFaction = EFactionType::Neutral;
        int32 CreatedOnDay = 0;
    };

    TArray<FCombatSalvageWreckRecord> RecentCombatSalvageWrecks;

    /** Records a combat-created wreck for post-win salvage AI memory. */
    void RecordCombatSalvageWreck(UStrategySiteDefinition* Site, EFactionType WinnerFaction, int32 CurrentDay);
    /** Removes combat salvage records older than the campaign memory window. */
    void PruneOldCombatSalvageRecords(int32 CurrentDay);
    /** True if the faction won combat at this site within the memory window. */
    bool DidFactionWinCombatAtSite(EFactionType Faction, const UStrategySiteDefinition* Site, int32 CurrentDay) const;
    /** Picks the highest-scoring unreserved salvage site in range for a vehicle. */
    bool FindBestSalvageTargetForVehicle(UStrategyVehicle* Vehicle, TSet<class UStrategySiteDefinition*>& InOutReservedSites,
        UStrategySiteDefinition*& OutSite, float& OutScore) const;

    /** Resolves the salvage wreck site targeted by a mission. */
    UStrategySiteDefinition* GetSalvageTargetSite(const UMissionGroup* Mission) const;
    /** Builds a force snapshot for salvage contest resolution. */
    FSalvageContestForceSnapshot BuildSalvageContestSnapshot(const UMissionGroup* Mission) const;
    /**
     * Contested salvage detection: when Human and Enemy salvage missions with active movement
     * target the same wreck SiteId, calls BeginSalvageContest and pauses strategic simulation.
     */
    void DetectSalvageContests();
    /** Calculates overall fleet combat effectiveness (0–100) using soldier stats and vehicle health. */
    float CalculateFleetEffectiveness(const UMissionGroup* Mission) const;

    /** Finalizes mission rewards, docks survivors, and broadcasts completion. */
    void ResolveMissionOutcome(UMissionGroup* Mission);

    /** Selects a mission waypoint target based on type, range, and reservation set. */
    bool TryPickMissionTarget(UStrategyVehicle* Vehicle, EMissionType MissionType, FVector2D& OutTarget,
        TSet<class UStrategySiteDefinition*>& InOutReservedSites, UStrategyBase** OutTargetBase = nullptr) const;

    /** Reads logical map dimensions and border padding from campaign settings. */
    void GetMapBounds(float& OutWidth, float& OutHeight, float& OutPadding) const;
    /** Collects sites already targeted by active mission waypoints. */
    void CollectSitesTargetedByActiveMissions(TSet<class UStrategySiteDefinition*>& OutSites, const UMissionGroup* IgnoreMission = nullptr) const;
    /** Finds the nearest site within tolerance of a map location. */
    class UStrategySiteDefinition* FindSiteAtLocation(const FVector2D& Location, float Tolerance = SiteMatchTolerance) const;
    /** Picks a random patrol point within round-trip range and map bounds. */
    FVector2D PickPatrolPointWithinRange(const FVector2D& Origin, float MaxRoundTripRange, float MinX, float MinY, float MaxX, float MaxY) const;
    /** True when Location lies inside the padded map rectangle and is non-zero. */
    static bool IsValidMapLocation(const FVector2D& Location, float MinX, float MinY, float MaxX, float MaxY);

    /** Computes a future launch hour evenly spaced across the 24-hour day. */
    float ComputeEvenlySpacedLaunchHour(int32 SlotIndex, int32 TotalSlots) const;
    /** Unparks vehicles from hangars and sets home hangar before departure. */
    void PrepareVehiclesForDeparture(UMissionGroup* Mission);
    /** Activates deferred missions whose scheduled launch hour has arrived. */
    void ProcessPendingMissionLaunches(float CurrentHours);

    /** True if the vehicle is assigned to any in-progress mission. */
    bool IsVehicleCommittedToAnyMission(UStrategyVehicle* Vehicle, const UMissionGroup* IgnoreMission = nullptr) const;
    /** True when a vehicle is docked, healthy, and ready for its scheduled launch. */
    bool IsVehicleReadyForMissionLaunch(UStrategyVehicle* Vehicle, const UMissionGroup* Mission) const;
    /** Cancels deferred missions from prior days that never launched. */
    void CancelStaleDeferredMissions(int32 CurrentSimulationDay);
};