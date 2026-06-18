#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "StrategicSimulationTypes.h"
#include "StrategyResourceDefinition.generated.h"

/** Primary data asset defining a displayable resource type, value, and icon. */
UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UStrategyResourceDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource")
    FText ResourceName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource")
    EResourceType ResourceType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource")
    float BaseValue = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource")
    TSoftObjectPtr<UTexture2D> Icon;
};