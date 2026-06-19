#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "StrategicSimulationTypes.h"
#include "UItemDefinition.h"
#include "USoldierClassDefinition.generated.h"

/**
 * Soldier archetype: base stats, training cost/time, and loadout rules.
 *
 * Designer order: Identity → Stats → Training → Progression → Loadout.
 */
UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API USoldierClassDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // === Identity ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    FText ClassName;

    // === Stats ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats",
        meta = (ShowOnlyInnerProperties, ToolTip = "Base combat and mobility attributes before gear bonuses."))
    FSoldierStats BaseStats;

    // === Training ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training|Cost",
        meta = (ToolTip = "Resources spent when training starts at Living Quarters."))
    FResourceStockpile TrainingCost;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training|Timing",
        meta = (ClampMin = "1", ToolTip = "Days to complete training in a Living Quarters slot."))
    int32 TrainingDays = 4;

    // === Progression ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progression",
        meta = (ClampMin = "0", ToolTip = "Experience granted when the soldier finishes training."))
    int32 StartingXP = 0;

    // === Loadout ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout|Rules",
        meta = (ToolTip = "Item definitions this class is allowed to equip (empty = unrestricted)."))
    TArray<TSoftObjectPtr<UItemDefinition>> AllowedItems;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout|Rules",
        meta = (ClampMin = "1", ToolTip = "Maximum equipped items for this class."))
    int32 MaxLoadoutSize = 6;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout|Starting Gear",
        meta = (ToolTip = "Items granted when training completes."))
    TArray<TSoftObjectPtr<UItemDefinition>> StartingGear;
};