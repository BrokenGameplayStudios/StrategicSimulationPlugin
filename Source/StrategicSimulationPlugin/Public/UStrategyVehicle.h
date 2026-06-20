#pragma once

#include "CoreMinimal.h"
#include "MissionPatrolRouteBuilder.h"
#include "UObject/NoExportTypes.h"
#include "UVehicleDefinition.h"
#include "StrategicSimulationTypes.h"
#include "StrategicSiteDefinition.h"
#include "UStrategyVehicle.generated.h"

// Forward declaration required for self-referential delegate parameter
class UStrategyVehicle;

/** Broadcast when this vehicle detects a new site (fires only once per site per faction) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSiteDetected, EFactionType, DetectingFaction, UStrategySiteDefinition*, DetectedSite);

/** Broadcast when this vehicle detects a new enemy vehicle (can fire multiple times for new contacts) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnVehicleDetected, UStrategyVehicle*, DetectingVehicle, UStrategyVehicle*, DetectedVehicle);

class UStrategyBase;
class UMissionGroup;
class UStrategyFacility;
class UBaseManagerSubsystem;
class UFactionIntelSubsystem;

/**
 * Runtime vehicle instance with mission movement, radar, combat behavior,
 * hardpoints, range/fuel, and salvage extraction during live simulation.
 */
UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UStrategyVehicle : public UObject
{
    GENERATED_BODY()

public:
    /** Default-constructs movement, health, and radar state. */
    UStrategyVehicle();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
    UVehicleDefinition* VehicleDefinition;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
    UStrategyBase* HomeBase;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
    UStrategyFacility* CurrentHanger;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
    UStrategyFacility* HomeHanger = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
    UMissionGroup* CurrentMission;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
    TArray<class UStrategySoldier*> CurrentPassengers;

	//=== Behavior & State ===

    /** Movement lifecycle phase — drives positioning */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|State")
    EVehicleMissionPhase CurrentPhase = EVehicleMissionPhase::Docked;

    /** Tactical behavior (AI decisions) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Behavior")
    EVehicleBehavior CurrentBehavior = EVehicleBehavior::Idle;

    /** Sets tactical behavior and updates mission phase (combat, returning, idle). */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Behavior")
    void SetBehavior(EVehicleBehavior NewBehavior, UStrategyVehicle* Target = nullptr);

    /** Returns the current tactical behavior enum. */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Behavior")
    EVehicleBehavior GetBehavior() const { return CurrentBehavior; }

    /** Returns the current mission movement phase. */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|State")
    EVehicleMissionPhase GetMissionPhase() const { return CurrentPhase; }

    /** Dock at home hangar: repark, refuel, reset movement state (keeps CurrentMission for manager resolution) */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|State")
    void DockAtHomeHangar();

    /** Park at home base without clearing an assigned mission (used on build complete and deferred launch queue) */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|State")
    void InitializeParkedAtBase();

    /** Number of soldiers currently aboard. */
    UFUNCTION(BlueprintPure, Category = "Vehicle|Crew")
    int32 GetCrewCount() const { return CurrentPassengers.Num(); }

    /** True when at least one soldier is aboard (minimum to depart). */
    UFUNCTION(BlueprintPure, Category = "Vehicle|Crew")
    bool HasMinimumCrew() const { return CurrentPassengers.Num() > 0; }

    /** Begin live mission movement toward a target */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Live Movement")
    void BeginMissionMovement(FVector2D TargetLocation, float CurrentGameHours, float SearchHoursAtTarget, EMissionType MissionType);

    /** Begin multi-waypoint patrol movement using a pre-built route (recon / defensive). */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Live Movement")
    void BeginPatrolMovement(const FMissionPatrolRoute& Route, float CurrentGameHours, float SearchHoursAtTarget, EMissionType MissionType);

    /** Current target vehicle (used when Attacking or Evading) */
    UPROPERTY(VisibleAnywhere, Category = "Vehicle|Behavior")
    TWeakObjectPtr<UStrategyVehicle> CurrentTargetVehicle;

    /** Game time when current combat behavior (Attacking/Evading) started */
    UPROPERTY(VisibleAnywhere, Category = "Vehicle|Behavior")
    float CombatBehaviorStartTime = -1.0f;

    // === Hardpoint System ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Vehicle|Hardpoints")
    TArray<TSoftObjectPtr<UItemDefinition>> EquippedWeapons;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Vehicle|Hardpoints")
    TArray<TSoftObjectPtr<UItemDefinition>> EquippedDefenseSystems;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, SaveGame, Category = "Vehicle|Hardpoints")
    TArray<int32> WeaponAmmoCounts;

    /** Returns maximum weapon hardpoint count from vehicle definition. */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Hardpoints")
    int32 GetMaxWeaponSlots() const;

    /** Returns maximum defense hardpoint count from vehicle definition. */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Hardpoints")
    int32 GetMaxDefenseSlots() const;

    /** True when weapon is valid and a hardpoint slot is available. */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Hardpoints")
    bool CanEquipWeapon(UItemDefinition* Weapon) const;

    /** Equips a weapon and initializes its ammo count. */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Hardpoints")
    bool EquipWeapon(UItemDefinition* Weapon);

    /** Equips a defense system if a slot is available. */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Hardpoints")
    bool EquipDefenseSystem(UItemDefinition* DefenseItem);

    /** Resolves equipped weapon soft pointers to UObject instances. */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Hardpoints")
    TArray<UItemDefinition*> GetEquippedWeapons() const;

    /** Computes offensive rating from base attack, weapons, and ammo. */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Stats")
    int32 GetVehicleOffensiveRating() const;

    /** Sums defensive bonuses from equipped defense systems. */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Stats")
    int32 GetVehicleDefensiveRating() const;

    // === NEW: Range / Fuel System ===
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Range")
    float CurrentRangeLeft = 0.0f;

    /** Returns maximum mission range from vehicle definition. */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Range")
    float GetMaxRange() const;

    /** True when CurrentRangeLeft covers the required round-trip distance. */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Range")
    bool HasEnoughRangeForMission(float RequiredDistance) const;

    // === Damage & Repair ===
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Damage & Repair")
    EVehicleDamageState DamageState = EVehicleDamageState::Undamaged;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Damage & Repair")
    int32 CurrentHealth = 100;

    /** True once a salvage wreck site has been created for this destroyed vehicle */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Damage & Repair")
    bool bWreckSalvageProcessed = false;

    /** True when docked with no active mission assignment. */
    UFUNCTION(BlueprintCallable, Category = "Vehicle")
    bool IsAtHome() const { return CurrentPhase == EVehicleMissionPhase::Docked && CurrentMission == nullptr; }

    /** Applies damage, updates damage state, and triggers wreck on destruction. */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Damage")
    void ApplyDamage(int32 DamageAmount);

    /** True when health is below max or damage state is not Undamaged. */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Repair")
    bool NeedsRepair() const;

    /** Maps health percentage to EVehicleDamageState and handles destroy side effects. */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Damage")
    void UpdateDamageStateFromHealth();

    /** True when damage state is Destroyed. */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Damage")
    bool IsDestroyed() const { return DamageState == EVehicleDamageState::Destroyed; }

    /** Returns true if this vehicle finished its mission (docked or destroyed in combat) */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|State")
    bool IsMissionFinished() const;

    // ===========================================================================
    // === NEW: LIVE MOVEMENT + RADAR PING SYSTEM 
    // ===========================================================================
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Live Movement")
    FVector2D CurrentPosition;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Live Movement")
    TArray<FVector2D> CurrentWaypoints;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Live Movement")
    float LaunchGameTimeHours = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Live Movement")
    float TotalTravelTimeHours = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Live Movement")
    float CruiseSpeedPixelsPerHour = 250.0f;  // pixels per game hour — tweak per vehicle type later

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Live Movement")
    float OutboundTravelTime = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Live Movement")
    float ReturnTravelTime = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Live Movement")
    float SearchTimeAtTarget = 0.0f;

    /** Patrol focal point (survey site, spoke target, or entry lane center). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Live Movement")
    FVector2D PatrolFocalPoint = FVector2D::ZeroVector;

    /** True when CurrentWaypoints uses multi-leg patrol routing instead of the legacy 3-point path. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Live Movement")
    bool bPatrolRouteActive = false;

    /** Waypoints used when returning to base (separate from mission waypoints) */
    UPROPERTY(VisibleAnywhere, Category = "Vehicle|Movement")
    TArray<FVector2D> ReturningWaypoints;

    /** Distance traveled along the returning waypoints (game-hour speed based) */
    UPROPERTY(VisibleAnywhere, Category = "Vehicle|Movement")
    float ReturningDistanceTraveled = 0.0f;

    /** Total length of the returning path in map pixels */
    UPROPERTY(VisibleAnywhere, Category = "Vehicle|Movement")
    float ReturningPathLength = 0.0f;

    /** Round-trip range budget allocated when the current mission leg began */
    UPROPERTY(VisibleAnywhere, Category = "Vehicle|Range")
    float PlannedRoundTripRange = 0.0f;

    /** Distance already flown during the active mission (all phases) */
    UPROPERTY(VisibleAnywhere, Category = "Vehicle|Range")
    float RangeTraveledThisMission = 0.0f;

    /** Wreck being salvaged during the current Salvage mission leg */
    UPROPERTY(VisibleAnywhere, Transient, Category = "Vehicle|Salvage")
    TObjectPtr<class UStrategySiteDefinition> ActiveSalvageSite = nullptr;

    /** Resources recovered this mission (already credited to faction pool during extraction) */
    UPROPERTY(VisibleAnywhere, Transient, Category = "Vehicle|Salvage")
    FResourceStockpile SalvageExtractedThisMission;

    /** Potential base site being claimed or guarded during a BaseExpansion mission. */
    UPROPERTY(VisibleAnywhere, Transient, Category = "Vehicle|Expansion")
    TObjectPtr<class UStrategySiteDefinition> ActiveExpansionSite = nullptr;

    /** True while holding on-station until Command Center construction completes. */
    UPROPERTY(VisibleAnywhere, Transient, Category = "Vehicle|Expansion")
    bool bExpansionGuardActive = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Radar")
    float LastPingGameTimeHours = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Radar")
    FVector2D LastRadarSweepOrigin = FVector2D::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Radar", meta = (ClampMin = "0.1", ClampMax = "4.0"))
    float PingIntervalHours = 0.5f;  // every 30 game minutes by default

    /** Returns effective radar range from definition and vehicle type minimums. */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Radar")
    float GetRadarRange() const;
    
    /** Main live-simulation tick: movement, combat, salvage, and radar pings. */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Live Movement")
    void UpdatePositionAndPings(float CurrentGameHours, float DeltaGameHours);

    /** Hourly resource transfer while on-station at a salvage wreck (Salvage missions). */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Salvage")
    bool ProcessSalvageExtractionTick(float DeltaGameHours);

    /** Holds on-station during BaseExpansion until CC is operational or construction is cancelled. */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Expansion")
    bool ProcessBaseExpansionGuardTick(float DeltaGameHours);

    /** Executes one radar sweep: site discovery, base scan, and vehicle detection. */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Radar")
    void PerformRadarPing();

    /** Interpolates map position along outbound, on-station, and return legs. */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Live Movement")
    FVector2D GetPositionOnPath(float Progress) const;

    /** True when the vehicle has docked after an active mission leg. */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Live Movement")
    bool IsMissionComplete(float CurrentGameHours) const;

    /** Builds linear return waypoints from current position to home base. */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Live Movement")
    void GenerateReturnPath();

    /** Called when this vehicle detects a new site (fires only once per site) */
    UPROPERTY(BlueprintAssignable, Category = "Vehicle|Detection")
    FOnSiteDetected OnSiteDetected;

    /** Called when this vehicle detects a new enemy vehicle (can fire multiple times) */
    UPROPERTY(BlueprintAssignable, Category = "Vehicle|Detection")
    FOnVehicleDetected OnVehicleDetected;

    /** Attempts to detect another vehicle within radar sweep with cooldown. */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Detection")
    void TryDetectVehicle(UStrategyVehicle* OtherVehicle);
private:

    /** Game hour when each vehicle was last detected (for cooldown) */
    TMap<TWeakObjectPtr<UStrategyVehicle>, float> LastDetectedGameHour;

    /** How long (in game hours) before we can detect the same vehicle again */
    UPROPERTY(EditAnywhere, Category = "Vehicle|Detection")
    float VehicleDetectionCooldownHours = 2.0f;

    int32 LoiterWaypointStart = 0;
    int32 LoiterWaypointEnd = 0;
    float LoiterPathLength = 0.0f;

    /** Returns effective cruise speed in pixels per game hour. */
    float GetCruiseSpeed() const;
    /** Accumulates distance flown against the current mission range budget. */
    void ConsumeMissionRange(float Distance);
    /** True when range traveled exceeds planned round-trip by 5%. */
    bool HasExceededMissionRangeBudget() const;
    /** Applies mutual combat damage while in the Combat phase. */
    void ProcessCombatTick(float DeltaGameHours);
    /** Sets EnRoute vs OnStation from normalized path progress. */
    void UpdatePhaseFromPathProgress(float Progress);
    /** Moves the vehicle along ReturningWaypoints toward home base. */
    void AdvanceReturningMovement(float DeltaGameHours);
    /** Total pixel length of the returning waypoint path. */
    float GetReturningPathLength() const;
    /** Interpolates position along the returning path at a given distance. */
    FVector2D GetPositionOnReturningPath(float DistanceAlongPath) const;
    /** Interpolates position along a polyline sub-range at a given distance. */
    FVector2D GetPositionAlongPolyline(const TArray<FVector2D>& Points, int32 StartIndex, int32 EndIndexExclusive,
        float DistanceAlongPath) const;
    /** Interpolates position along the closed loiter loop at a wrapped distance. */
    FVector2D GetPositionOnLoiterLoop(float DistanceAlongLoop) const;
    /** Fires radar pings at PingIntervalHours while in flight. */
    void TickRadarPings(float CurrentGameHours);
    /** True when a world position lies within the current radar sweep segment. */
    bool IsPositionWithinRadarSweep(const FVector2D& WorldPosition) const;
    /** True when terrain does not block radar line of sight to a position. */
    bool HasLineOfSightToPosition(const FVector2D& WorldPosition) const;
    /** Registers site discovery and intel for a site within radar range. */
    void DiscoverSiteInRange(UStrategySiteDefinition* Site, EFactionType VehicleFaction, float CurrentGameHours,
        UBaseManagerSubsystem* BaseManager, UFactionIntelSubsystem* IntelMgr);
    /** Scans enemy bases and parked vehicles along the radar sweep path. */
    void ScanEnemyBasesAlongSweep(EFactionType VehicleFaction, float CurrentGameHours, UBaseManagerSubsystem* BaseManager,
        UFactionIntelSubsystem* IntelMgr);

    /** Forwards detection to UAIControllerSubsystem for engagement decisions. */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Detection")
    virtual void HandleVehicleDetected(UStrategyVehicle* DetectedVehicle);
};