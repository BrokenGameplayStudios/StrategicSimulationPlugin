#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "StrategicSimulationTypes.h" 
#include "UStrategyTechDefinition.generated.h"

// Forward declaration only — breaks the circular include with UItemDefinition
class UItemDefinition;

/** Primary data asset for an item-tech node: category, tier, and unlocked craftable items. */
UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UStrategyTechDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tech")
    FText TechName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tech")
    EItemCategory Category;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tech")
    ETechTier Tier = ETechTier::Tier1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    FText Description;

    // Tech ONLY unlocks Items (this is the final layer of the chain)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unlocks")
    TArray<TSoftObjectPtr<UItemDefinition>> UnlocksItems;
};