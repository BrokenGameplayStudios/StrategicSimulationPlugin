#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UItemDefinition.h"
#include "UStrategyBase.h"
#include "UActiveProductionJob.generated.h"

UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UActiveProductionJob : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Production")
    UItemDefinition* ItemToProduce = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Production")
    int32 Quantity = 1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Production")
    int32 RemainingDays = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Production")
    bool bIsCompleted = false;

    // NEW: Used by the production queue system
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Production")
    bool bIsQueued = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Production")
    UStrategyBase* OwningBase = nullptr;
};