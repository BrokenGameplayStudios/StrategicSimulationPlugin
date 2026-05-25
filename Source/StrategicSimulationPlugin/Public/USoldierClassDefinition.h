#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "StrategicSimulationTypes.h"
#include "UItemDefinition.h"
#include "USoldierClassDefinition.generated.h"

UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API USoldierClassDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier")
    FText ClassName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier")
    FSoldierStats BaseStats;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier")
    int32 StartingXP = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
    TArray<TSoftObjectPtr<UItemDefinition>> StartingGear;
};