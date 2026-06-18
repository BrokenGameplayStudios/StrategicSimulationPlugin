#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UResearchTechDefinition.h"
#include "UResearchDatabase.generated.h"

/** Registry of research tech definitions available in the campaign tech tree. */
UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UResearchDatabase : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    /** Soft references to all research projects that can be started at laboratories. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Research")
    TArray<TSoftObjectPtr<UResearchTechDefinition>> AvailableTechs;
};