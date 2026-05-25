#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UItemDefinition.h"
#include "UStrategyTechDefinition.h"
#include "UFacilityDefinition.h"
#include "UResearchTechDefinition.generated.h"

UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UResearchTechDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Research")
    FText ProjectName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Research")
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cost")
    FResourceStockpile ResearchCost;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progress")
    int32 ResearchDays = 5;

    // Research unlocks the next layer (Tech)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unlocks")
    TArray<TSoftObjectPtr<UStrategyTechDefinition>> UnlocksTech;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unlocks")
    TArray<TSoftObjectPtr<UFacilityDefinition>> UnlocksFacilities;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unlocks")
    TArray<TSoftObjectPtr<UResearchTechDefinition>> UnlocksResearch;
};