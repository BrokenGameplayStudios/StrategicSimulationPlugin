#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "StrategicSimulationTypes.h"
#include "StrategyResourceDefinition.generated.h"

/** Display metadata for a resource type (UI labels and icons). */
UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UStrategyResourceDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    FText ResourceName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    EResourceType ResourceType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy",
        meta = (ClampMin = "0.0", ToolTip = "Relative trade/scoring weight for this resource."))
    float BaseValue = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation")
    TSoftObjectPtr<UTexture2D> Icon;
};