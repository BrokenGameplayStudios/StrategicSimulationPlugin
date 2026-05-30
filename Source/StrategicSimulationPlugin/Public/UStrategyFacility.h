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

UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UStrategyFacility : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Facility")
    UFacilityDefinition* FacilityDefinition;

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

    /** Legacy alias for compatibility */
    UFUNCTION(BlueprintCallable, Category = "Repair")
    void SimulateDailyRepair(UStrategyBase* InOwningBase);

    // === PRODUCTION API ===
    UFUNCTION(BlueprintCallable, Category = "Production")
    bool StartProduction(EProductionType Type, UObject* TargetAsset, int32 BaseDays);

    UFUNCTION(BlueprintCallable, Category = "Production")
    void AdvanceProductionDay();

    UFUNCTION(BlueprintCallable, Category = "Production")
    bool HasFreeProductionSlot() const;

    UFUNCTION(BlueprintCallable, Category = "Production")
    int32 GetAvailableProductionSlots() const;

    // Construction (kept exactly as before)
    UFUNCTION(BlueprintCallable, Category = "Construction")
    bool StartConstruction(UFacilityDefinition* Def);

    UFUNCTION(BlueprintCallable, Category = "Construction")
    void AdvanceConstructionDay();

    UFUNCTION(BlueprintCallable, Category = "Construction")
    bool CancelConstruction(int32 JobIndex, bool bFullRefund = true);

    // Helpers
    bool CanTrainMoreSoldiers() const { return HasFreeProductionSlot() && FacilityDefinition && FacilityDefinition->FacilityType == EFacilityType::LivingQuarters; }
    bool CanBuildMoreVehicles() const { return HasFreeProductionSlot() && FacilityDefinition && FacilityDefinition->FacilityType == EFacilityType::Hanger; }

private:
    void CompleteProductionJob(int32 Index);
};