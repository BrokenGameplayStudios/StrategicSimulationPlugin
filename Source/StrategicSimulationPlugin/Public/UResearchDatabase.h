#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UResearchTechDefinition.h"
#include "UResearchDatabase.generated.h"

/** Master catalog of laboratory research projects (tech tree roots and branches). */
UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UResearchDatabase : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Catalog",
        meta = (ToolTip = "Ordered list of research projects. AI iterates this list; chain follow-ups via UnlocksResearch on each project."))
    TArray<TSoftObjectPtr<UResearchTechDefinition>> AvailableTechs;
};