#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "StrategicSimulationTypes.h"
#include "UFacilityDefinition.generated.h"

class UResearchTechDefinition;

/**
 * Buildable base facility: costs, power, production queues, extraction, and unlocks.
 *
 * Designer order: Identity → Build → Power → Production → Extraction → Service → Capacity → Unlocks.
 */
UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UFacilityDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // === Identity ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    FText FacilityName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity",
        meta = (ToolTip = "Drives build rules, daily simulation hooks, and prerequisite checks."))
    EFacilityType FacilityType;

    // === Build ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build|Cost")
    FResourceStockpile BuildCost;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build|Timing",
        meta = (ClampMin = "1", ToolTip = "Days to construct once queued at a base."))
    int32 BuildTimeDays = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build|Limits",
        meta = (ClampMin = "1", ToolTip = "Maximum copies of this facility type allowed per base."))
    int32 MaxBuilt = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build|Requirements",
        meta = (ToolTip = "Other facility types that must exist at the base before this can be built."))
    TArray<EFacilityType> PrerequisiteFacilities;

    // === Power ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Power",
        meta = (ClampMin = "0", ToolTip = "Net power contributed when operational (e.g. Power Plant)."))
    int32 PowerProvided = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Power",
        meta = (ClampMin = "0", ToolTip = "Power consumed while operational."))
    int32 PowerDraw = 0;

    // === Production ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Production|Queue",
        meta = (ClampMin = "0", ToolTip = "Concurrent jobs: soldier training, vehicle builds, research, workshop items, repair/heal throughput."))
    int32 ProductionSlots = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Production|Queue",
        meta = (ClampMin = "0.1", ToolTip = "Multiplier applied to queued job duration."))
    float ProductionSpeedMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Production|Income",
        meta = (ToolTip = "Flat resources granted every day while operational (not tied to site extraction)."))
    FResourceStockpile ProductionPerDay;

    // === Extraction ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extraction",
        meta = (ToolTip = "Resources pulled from the site's remaining stockpile each day while operational."))
    FResourceStockpile ExtractionPerDay;

    // === Service ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Service|Repair",
        meta = (ClampMin = "0", EditCondition = "FacilityType == EFacilityType::VehicleRepair", EditConditionHides,
            ToolTip = "HP restored per vehicle per day (limited by ProductionSlots)."))
    int32 RepairHealthPerDay = 0;

    // === Capacity ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capacity",
        meta = (ClampMin = "0", ToolTip = "Stationed units supported: soldiers (Living Quarters), parked vehicles (Hangar), medical/repair totals."))
    int32 Capacity = 0;

    // === Unlocks ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unlocks|Research",
        meta = (ToolTip = "Research projects made available when this facility is built."))
    TArray<TSoftObjectPtr<UResearchTechDefinition>> UnlocksResearch;
};