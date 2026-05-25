#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UItemDefinition.h"
#include "UItemDatabase.generated.h"

UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UItemDatabase : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // Add all items you want the AI to be able to buy here
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Items")
    TArray<TSoftObjectPtr<UItemDefinition>> BuyableItems;
};