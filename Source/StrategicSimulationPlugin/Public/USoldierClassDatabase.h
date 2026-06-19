#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "USoldierClassDefinition.h"
#include "USoldierClassDatabase.generated.h"

/** Master catalog of soldier class definitions for recruitment and AI training. */
UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API USoldierClassDatabase : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Catalog",
        meta = (ToolTip = "All soldier classes trainable at Living Quarters."))
    TArray<TSoftObjectPtr<USoldierClassDefinition>> AvailableSoldierClasses;
};