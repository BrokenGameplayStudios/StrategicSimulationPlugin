#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "StrategicSimulationTypes.h"
#include "UResearchTechDefinition.generated.h"

class UStrategyTechDefinition;
class UFacilityDefinition;
class UResearchTechDefinition;

/**
 * Laboratory research project: cost, duration, and downstream unlocks.
 *
 * Unlock chain: Research → Strategy Tech → Items / Facilities / further Research.
 */
UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UResearchTechDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // === Identity ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    FText ProjectName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity", meta = (MultiLine = true))
    FText Description;

    // === Economy ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy|Cost",
        meta = (ToolTip = "Up-front resources consumed when research is queued (if enforced by UI)."))
    FResourceStockpile ResearchCost;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy|Timing",
        meta = (ClampMin = "1", ToolTip = "Days to complete in a Laboratory production slot."))
    int32 ResearchDays = 5;

    // === Unlocks ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unlocks|Tech",
        meta = (ToolTip = "Item-tech nodes unlocked when this project completes."))
    TArray<TSoftObjectPtr<UStrategyTechDefinition>> UnlocksTech;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unlocks|Facilities",
        meta = (ToolTip = "Facility types that become buildable when this project completes."))
    TArray<TSoftObjectPtr<UFacilityDefinition>> UnlocksFacilities;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unlocks|Research",
        meta = (ToolTip = "Follow-on research projects made available in the tech tree."))
    TArray<TSoftObjectPtr<UResearchTechDefinition>> UnlocksResearch;
};