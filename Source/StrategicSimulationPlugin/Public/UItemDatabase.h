#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UItemDefinition.h"
#include "UItemDatabase.generated.h"

/** Master catalog of item definitions for AI purchase and workshop production. */
UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UItemDatabase : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Catalog",
        meta = (ToolTip = "All item assets the AI may buy or queue. Split infantry vs vehicle databases on the game initializer if desired."))
    TArray<TSoftObjectPtr<UItemDefinition>> BuyableItems;
};