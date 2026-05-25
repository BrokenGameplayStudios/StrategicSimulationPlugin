#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "StrategicSimulationTypes.h"
#include "UStrategySaveGame.generated.h"

UCLASS()
class STRATEGICSIMULATIONPLUGIN_API UStrategySaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    int32 CurrentDay = 1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    FResourceStockpile HumanResources;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    FResourceStockpile EnemyResources;

    // Metadata for save select screen
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    FDateTime LastSavedTime;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    FText HumanSummary;   // e.g. "12 Soldiers, 4 Facilities"

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    int32 HumanSoldierCount = 0;
};