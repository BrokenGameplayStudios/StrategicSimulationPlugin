#pragma once

#include "CoreMinimal.h"
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

UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UStrategyVehicle : public UObject
{
    GENERATED_BODY()

public:
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

	//=== Behavuior & State ===

    /** Current behavior state of this vehicle */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Behavior")
    EVehicleBehavior CurrentBehavior = EVehicleBehavior::Scouting;

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Behavior")
    void SetBehavior(EVehicleBehavior NewBehavior, UStrategyVehicle* Target = nullptr);

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Behavior")
    EVehicleBehavior GetBehavior() const { return CurrentBehavior; }

    /** Current target vehicle (used when Attacking or Evading) */
    UPROPERTY(VisibleAnywhere, Category = "Vehicle|Behavior")
    TWeakObjectPtr<UStrategyVehicle> CurrentTargetVehicle;

    // === Hardpoint System ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Vehicle|Hardpoints")
    TArray<TSoftObjectPtr<UItemDefinition>> EquippedWeapons;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Vehicle|Hardpoints")
    TArray<TSoftObjectPtr<UItemDefinition>> EquippedDefenseSystems;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, SaveGame, Category = "Vehicle|Hardpoints")
    TArray<int32> WeaponAmmoCounts;

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Hardpoints")
    int32 GetMaxWeaponSlots() const;

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Hardpoints")
    int32 GetMaxDefenseSlots() const;

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Hardpoints")
    bool CanEquipWeapon(UItemDefinition* Weapon) const;

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Hardpoints")
    bool EquipWeapon(UItemDefinition* Weapon);

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Hardpoints")
    bool EquipDefenseSystem(UItemDefinition* DefenseItem);

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Hardpoints")
    TArray<UItemDefinition*> GetEquippedWeapons() const;

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Stats")
    int32 GetVehicleOffensiveRating() const;

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Stats")
    int32 GetVehicleDefensiveRating() const;

    // === NEW: Range / Fuel System ===
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Range")
    float CurrentRangeLeft = 0.0f;

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Range")
    float GetMaxRange() const;

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Range")
    bool HasEnoughRangeForMission(float RequiredDistance) const;

    // === Damage & Repair ===
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Damage & Repair")
    EVehicleDamageState DamageState = EVehicleDamageState::Undamaged;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Damage & Repair")
    int32 CurrentHealth = 100;

    UFUNCTION(BlueprintCallable, Category = "Vehicle")
    bool IsAtHome() const { return CurrentMission == nullptr; }

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Damage")
    void ApplyDamage(int32 DamageAmount);

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Repair")
    bool NeedsRepair() const;

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Damage")
    void UpdateDamageStateFromHealth();

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

    /** Waypoints used when returning to base (separate from mission waypoints) */
    UPROPERTY(VisibleAnywhere, Category = "Vehicle|Movement")
    TArray<FVector2D> ReturningWaypoints;

    /** Current progress along the returning waypoints */
    UPROPERTY(VisibleAnywhere, Category = "Vehicle|Movement")
    float ReturningProgress = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Radar")
    float LastPingGameTimeHours = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Radar", meta = (ClampMin = "0.1", ClampMax = "4.0"))
    float PingIntervalHours = 0.5f;  // every 30 game minutes by default

    // Deprecated - Radar range is now controlled ONLY by UVehicleDefinition->RadarRangePixels
    // Use GetRadarRange() to get the current value.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Radar", meta = (DeprecatedProperty))
    float PingRadiusPixels = 64.0f;

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Radar")
    float GetRadarRange() const;
    
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Live Movement")
    void LaunchScoutingMission(FVector2D TargetLocation, float CurrentGameHours, float SearchHoursAtTarget = 3.0f);

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Live Movement")
    void UpdatePositionAndPings(float CurrentGameHours);

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Radar")
    void PerformRadarPing();

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Live Movement")
    FVector2D GetPositionOnPath(float Progress) const;

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Live Movement")
    bool IsMissionComplete(float CurrentGameHours) const;

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Live Movement")
    void GenerateReturnPath();

    /** Called when this vehicle detects a new site (fires only once per site) */
    UPROPERTY(BlueprintAssignable, Category = "Vehicle|Detection")
    FOnSiteDetected OnSiteDetected;

    /** Called when this vehicle detects a new enemy vehicle (can fire multiple times) */
    UPROPERTY(BlueprintAssignable, Category = "Vehicle|Detection")
    FOnVehicleDetected OnVehicleDetected;

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Detection")
    void TryDetectVehicle(UStrategyVehicle* OtherVehicle);
private:

    /** Tracks vehicles we've recently detected to avoid spamming the delegate */
    UPROPERTY(VisibleAnywhere, Category = "Vehicle|Detection")
    TArray<TWeakObjectPtr<UStrategyVehicle>> RecentlyDetectedVehicles;

    /** How long (in game hours) before we can detect the same vehicle again */
    UPROPERTY(EditAnywhere, Category = "Vehicle|Detection")
    float VehicleDetectionCooldownHours = 2.0f;

    /** Called when this vehicle detects another vehicle.
 *  Decision logic lives in UAIControllerSubsystem (or player UI).
 *  This function can be overridden or extended. */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Detection")
    virtual void HandleVehicleDetected(UStrategyVehicle* DetectedVehicle);
};