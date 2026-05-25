#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "StrategicSimulationTypes.h"
#include "UStrategyTechDefinition.h"     // <-- Fixed: added this
#include "UFacilityDefinition.h"         // <-- Fixed: added this
#include "UResearchTechDefinition.generated.h"

UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UResearchTechDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Research")
    FText ProjectName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Research")
    FResourceStockpile ResearchCost;   // ResearchPoints mostly

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Research")
    int32 ResearchDays = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unlocks")
    TSoftObjectPtr<UStrategyTechDefinition> UnlocksTech;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unlocks")
    TArray<TSoftObjectPtr<UFacilityDefinition>> UnlocksFacility;
};