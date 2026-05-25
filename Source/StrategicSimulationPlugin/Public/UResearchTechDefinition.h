#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UItemDefinition.h"
#include "UFacilityDefinition.h"
#include "UResearchTechDefinition.generated.h"

UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UResearchTechDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // OLD PROPERTIES (kept for compatibility)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Research")
    FText ProjectName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Research")
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cost")
    FResourceStockpile ResearchCost;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progress")
    int32 ResearchDays = 5;   // renamed from ResearchTimeDays for compatibility

    // NEW: What this research unlocks
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unlocks")
    TArray<TSoftObjectPtr<UItemDefinition>> UnlocksItems;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unlocks")
    TArray<TSoftObjectPtr<UFacilityDefinition>> UnlocksFacilities;
};