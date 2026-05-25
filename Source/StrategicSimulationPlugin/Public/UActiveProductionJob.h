#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "StrategicSimulationTypes.h"
#include "UItemDefinition.h"
#include "UStrategyTechDefinition.h"
#include "UActiveProductionJob.generated.h"

UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UActiveProductionJob : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Production")
    UItemDefinition* ItemToProduce;          // or UStrategyTechDefinition for facilities

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Progress")
    int32 RemainingDays = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Progress")
    int32 Quantity = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Status")
    bool bIsCompleted = false;
};