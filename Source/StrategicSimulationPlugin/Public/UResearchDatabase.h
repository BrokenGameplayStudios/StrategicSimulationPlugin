#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UResearchTechDefinition.h"
#include "UResearchDatabase.generated.h"

UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UResearchDatabase : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Research")
    TArray<TSoftObjectPtr<UResearchTechDefinition>> AvailableTechs;
};