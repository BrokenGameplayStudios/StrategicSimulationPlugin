#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "StrategicSimulationTypes.h"
#include "UStrategyTechDefinition.h"
#include "UItemDefinition.generated.h"

UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UItemDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
    FText ItemName; 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cost")
    FResourceStockpile PurchaseCost;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    int32 Damage = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    int32 ArmorBonus = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    int32 AimBonus = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    bool bIsConsumable = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    int32 QuantityPerStack = 1;
};