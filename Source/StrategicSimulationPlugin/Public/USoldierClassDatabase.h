#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "USoldierClassDefinition.h"
#include "USoldierClassDatabase.generated.h"

/** Registry of soldier class definitions available for recruitment and AI training. */
UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API USoldierClassDatabase : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    /** Soft references to all soldier classes that can be trained at bases. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldiers")
    TArray<TSoftObjectPtr<USoldierClassDefinition>> AvailableSoldierClasses;
};