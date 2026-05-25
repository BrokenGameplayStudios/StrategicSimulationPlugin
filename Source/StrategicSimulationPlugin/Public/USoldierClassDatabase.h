#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "USoldierClassDefinition.h"
#include "USoldierClassDatabase.generated.h"

UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API USoldierClassDatabase : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldiers")
    TArray<TSoftObjectPtr<USoldierClassDefinition>> AvailableSoldierClasses;
};