#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "StrategicSimulationTypes.h"
#include "UItemDefinition.h"
#include "USoldierClassDefinition.generated.h"

/** Primary data asset defining a soldier archetype: stats, training cost, and loadout rules. */
UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API USoldierClassDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier")
    FText ClassName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier")
    FSoldierStats BaseStats;

    /** Full cost to train one soldier of this class */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training")
    FResourceStockpile TrainingCost;

    /** How many days it takes to train one soldier of this class */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training")
    int32 TrainingDays = 4;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier")
    int32 StartingXP = 0;

    /** Items this soldier is allowed to equip (class restrictions) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
    TArray<TSoftObjectPtr<UItemDefinition>> AllowedItems;

    /** Maximum number of items this class can carry */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
    int32 MaxLoadoutSize = 6;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
    TArray<TSoftObjectPtr<UItemDefinition>> StartingGear;
};