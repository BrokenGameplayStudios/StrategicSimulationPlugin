#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "StrategicSimulationTypes.h"
#include "UStrategyTechDefinition.generated.h"

class UItemDefinition;

/**
 * Item-tech node unlocked by research — grants craftable/purchasable items.
 *
 * Typically the last step in the chain: Research → Strategy Tech → Items.
 */
UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UStrategyTechDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // === Identity ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    FText TechName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity", meta = (MultiLine = true))
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity",
        meta = (ToolTip = "Item family this tech belongs to (weapon type, medical, vehicle module, etc.)."))
    EItemCategory Category;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity",
        meta = (ToolTip = "Progression tier within the category's tech ladder."))
    ETechTier Tier = ETechTier::Tier1;

    // === Unlocks ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unlocks|Items",
        meta = (ToolTip = "Items made available for purchase or workshop production."))
    TArray<TSoftObjectPtr<UItemDefinition>> UnlocksItems;
};