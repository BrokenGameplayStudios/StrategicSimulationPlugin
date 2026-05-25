#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UItemDefinition.h"
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

	// What this Tech unlocks
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unlocks")
    TArray<TSoftObjectPtr<UItemDefinition>> UnlocksItem;
};