#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "StrategicSimulationTypes.h"
#include "UFacilityDefinition.h"
#include "UStrategyBase.h"
#include "UStrategyFacility.generated.h"

class UStrategyVehicle;
class UStrategySoldier;

USTRUCT(BlueprintType)
struct FConstructionJob
{
    GENERATED_BODY()

    UPROPERTY()
    UFacilityDefinition* FacilityDef = nullptr;

    UPROPERTY()
    int32 RemainingDays = 0;

    UPROPERTY()
    bool bIsPaused = false;
};

UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UStrategyFacility : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Facility")
    UFacilityDefinition* FacilityDefinition;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ownership")
    UStrategyBase* OwningBase = nullptr;   // ← Added to fix all OwningBase errors

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Build")
    int32 BuildProgressDays = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Status")
    bool bIsOperational = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Status")
    int32 CurrentPowerDraw = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hanger")
    TArray<class UStrategyVehicle*> ParkedVehicles;

    /** Soldiers currently stationed in this barracks (parallel to ParkedVehicles) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Barracks")
    TArray<class UStrategySoldier*> ParkedSoldiers;

    // === NEW: Construction Queue System (added, nothing removed) ===
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Construction")
    TArray<FConstructionJob> ActiveConstructionJobs;

    /** Simulate one day of repairs for parked vehicles in this base */
    UFUNCTION(BlueprintCallable, Category = "Repair")
    void SimulateDailyRepair(UStrategyBase* OwningBase);

    // === Queue Management (new) ===
    UFUNCTION(BlueprintCallable, Category = "Construction")
    bool CanQueueMoreOfType(EFacilityType Type) const;

    UFUNCTION(BlueprintCallable, Category = "Construction")
    bool StartConstruction(UFacilityDefinition* Def);

    UFUNCTION(BlueprintCallable, Category = "Construction")
    void AdvanceConstructionDay();

    UFUNCTION(BlueprintCallable, Category = "Construction")
    bool CancelConstruction(int32 JobIndex, bool bFullRefund = true);

    UFUNCTION(BlueprintCallable, Category = "Construction")
    void SimulateDaily();
};