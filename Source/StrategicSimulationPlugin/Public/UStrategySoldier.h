#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "StrategicSimulationTypes.h"
#include "USoldierClassDefinition.h"
#include "UItemDefinition.h"
#include "UStrategySoldier.generated.h"

UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UStrategySoldier : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Soldier")
    FString SoldierName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Soldier")
    USoldierClassDefinition* ClassDefinition;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Stats")
    FSoldierStats CurrentStats;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Progress")
    int32 XP = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Progress")
    int32 Rank = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Status")
    bool bIsWounded = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Status")
    int32 DaysUntilRecovered = 0;

    // Soldier carries Items (weapons, armor, grenades, etc.)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Loadout")
    TArray<TSoftObjectPtr<UItemDefinition>> CurrentLoadout;

    UFUNCTION(BlueprintCallable, Category = "Debug")
    void PrintInfo() const;
};