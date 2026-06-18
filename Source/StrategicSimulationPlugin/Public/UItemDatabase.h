#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UItemDefinition.h"
#include "UItemDatabase.generated.h"

/** Registry of item definitions available for AI purchase and production selection. */
UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UItemDatabase : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    /** Soft references to all items the AI is allowed to buy or queue for production. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Items")
    TArray<TSoftObjectPtr<UItemDefinition>> BuyableItems;
};