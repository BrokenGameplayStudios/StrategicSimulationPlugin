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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Production")
    int32 ProductionSlots = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Production")
    float ProductionSpeedMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unlocks")
    TArray<TSoftObjectPtr<UResearchTechDefinition>> UnlocksResearch;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capacity")
    int32 Capacity = 0;

    /** Resources this facility produces every day (always given) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facility|Production")
    FResourceStockpile ProductionPerDay;

    /** Resources this facility extracts from the site it is built on per day */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facility|Extraction")
    FResourceStockpile ExtractionPerDay;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build")
    int32 MaxBuilt = 1;

    // === NEW: Data-driven prerequisites (added for build order) ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build")
    TArray<EFacilityType> PrerequisiteFacilities;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Repair")
    int32 RepairHealthPerDay = 0;
};