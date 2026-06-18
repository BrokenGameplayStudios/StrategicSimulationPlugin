#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "StrategicSimulationTypes.h"
#include "UFacilityDefinition.h"
#include "UStrategyFacility.generated.h"

// Forward declarations only
class UStrategyVehicle;
class UStrategySoldier;
class UStrategyBase;
class USoldierClassDefinition;
class UVehicleDefinition;

UENUM(BlueprintType)
enum class EProductionType : uint8
{
    None = 0,
    Soldier = 1,
    Vehicle = 2,
    Item = 3,
    Research = 4,
    Facility = 5
};

USTRUCT(BlueprintType)
struct FProductionJob
{
    GENERATED_BODY()

    UPROPERTY()
    EProductionType Type = EProductionType::None;

    UPROPERTY()
    UObject* TargetAsset = nullptr;          // SoldierClass, VehicleDef, ItemDef, etc.

    UPROPERTY()
    int32 RemainingDays = 0;

    UPROPERTY()
    float SpeedMultiplier = 1.0f;

    UPROPERTY()
    UStrategyBase* AssignedBase = nullptr;
};

/**
 * Runtime facility at a base: construction state, power draw, production queue,
 * hangar parking, and daily repair/heal/containment simulation.
 */
UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UStrategyFacility : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Facility")
    UFacilityDefinition* FacilityDefinition;

    // === NEW: Data-driven prerequisites (your requested feature) ===
    // List of facility types that must already be operational in the same base
    // before this facility can be built. Example:
    //   Research Lab → { LivingQuarters }
    //   Hanger      → { Laboratory }
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build")
    TArray<EFacilityType> PrerequisiteFacilities;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Status")
    bool bIsOperational = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Build")
    int32 BuildProgressDays = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ownership")
    UStrategyBase* OwningBase = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Power")
    int32 CurrentPowerDraw = 0;

    // === UNIFIED PRODUCTION QUEUE ===
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Production")
    TArray<FProductionJob> ActiveProductionJobs;

    // Existing
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hanger")
    TArray<class UStrategyVehicle*> ParkedVehicles;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Barracks")
    TArray<class UStrategySoldier*> ParkedSoldiers;

    /** Full daily simulation (repair + production + construction) */
    UFUNCTION(BlueprintCallable, Category = "Simulation")
    void SimulateDaily();

    /** Legacy alias that runs repair/heal logic for a specific owning base. */
    UFUNCTION(BlueprintCallable, Category = "Repair")
    void SimulateDailyRepair(UStrategyBase* InOwningBase);

    // === PRODUCTION API ===
    /** Queues a soldier, vehicle, item, or research job if a slot is free. */
    UFUNCTION(BlueprintCallable, Category = "Production")
    bool StartProduction(EProductionType Type, UObject* TargetAsset, int32 BaseDays);

    /** Decrements remaining days on all jobs and completes finished ones. */
    UFUNCTION(BlueprintCallable, Category = "Production")
    void AdvanceProductionDay();

    /** True when ProductionSlots exceed active job count. */
    UFUNCTION(BlueprintCallable, Category = "Production")
    bool HasFreeProductionSlot() const;

    /** Returns unused production slot count for this facility. */
    UFUNCTION(BlueprintCallable, Category = "Production")
    int32 GetAvailableProductionSlots() const;    
    
    /** Grants daily research bonus based on POW count and production slots. */
    UFUNCTION(BlueprintCallable, Category = "Containment")
    void ProcessContainmentDaily();

    /** Grants research from KIA bodies and clears stored bodies. */
    UFUNCTION(BlueprintCallable, Category = "Autopsy")
    void ProcessAutopsyDaily();

    // Construction (kept exactly as before)
    /** Queues facility self-construction as a production job. */
    UFUNCTION(BlueprintCallable, Category = "Construction")
    bool StartConstruction(UFacilityDefinition* Def);

    /** Alias for AdvanceProductionDay (construction uses the same queue). */
    UFUNCTION(BlueprintCallable, Category = "Construction")
    void AdvanceConstructionDay();

    /** Cancels a construction job and optionally refunds build cost. */
    UFUNCTION(BlueprintCallable, Category = "Construction")
    bool CancelConstruction(int32 JobIndex, bool bFullRefund = true);

    /** True when this living-quarters facility has a free soldier training slot. */
    bool CanTrainMoreSoldiers() const { return HasFreeProductionSlot() && FacilityDefinition && FacilityDefinition->FacilityType == EFacilityType::LivingQuarters; }
    /** True when this hangar has a free vehicle production slot. */
    bool CanBuildMoreVehicles() const { return HasFreeProductionSlot() && FacilityDefinition && FacilityDefinition->FacilityType == EFacilityType::Hanger; }

private:
    /** Spawns the completed soldier, vehicle, item, research, or facility and removes the job. */
    void CompleteProductionJob(int32 Index);
};