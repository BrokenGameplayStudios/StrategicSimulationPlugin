#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "StrategicSimulationTypes.h"
#include "UFacilityDefinition.generated.h"

// Forward declaration only
class UResearchTechDefinition;

UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UFacilityDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facility")
    FText FacilityName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facility")
    EFacilityType FacilityType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cost")
    FResourceStockpile BuildCost;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build")
    int32 BuildTimeDays = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Power")
    int32 PowerProvided = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Power")
    int32 PowerDraw = 0;

    // Production
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Production")
    int32 ProductionSlots = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Production")
    float ProductionSpeedMultiplier = 1.0f;

    // Facility ONLY unlocks Research Projects
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unlocks")
    TArray<TSoftObjectPtr<UResearchTechDefinition>> UnlocksResearch;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capacity")
    int32 Capacity = 0;

    // Economy & Power
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy")
    int32 MoneyIncomePerDay = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy")
    int32 SuppliesIncomePerDay = 0;

    // Maximum number of this facility type the faction can build
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build")
    int32 MaxBuilt = 1;

    // === NEW: Repair Bay Support (Phase 3.6) ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Repair")
    int32 RepairHealthPerDay = 0;   // Health restored per day per vehicle (e.g. 25 for a basic repair bay)
};