#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "StrategicSimulationTypes.h"
#include "UStrategyTechDefinition.generated.h"

UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UStrategyTechDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tech")
    FText TechName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tech")
    ETechCategory Category;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tech")
    ETechTier Tier = ETechTier::Tier1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    FResourceStockpile ProductionCost;   // what it costs to build/produce

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    int32 PowerDraw = 0;                 // for base facilities later

    // Example stats that apply to weapons/armor/defenses
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    int32 Damage = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    float Range = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    int32 ArmorBonus = 0;
};