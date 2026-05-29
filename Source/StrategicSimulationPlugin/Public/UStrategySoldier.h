#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "StrategicSimulationTypes.h"
#include "USoldierClassDefinition.h"
#include "UItemDefinition.h"
#include "UStrategySoldier.generated.h"

class UStrategyBase;
class UStrategyFacility;

UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UStrategySoldier : public UObject
{
    GENERATED_BODY()

public:
    UStrategySoldier();

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
    ESoldierStatus Status = ESoldierStatus::Healthy;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Status")
    bool bIsWounded = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Status")
    int32 DaysUntilRecovered = 0;

    /** Permanent reserved barracks slot — belongs to this soldier until death (parallel to HomeHanger) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Location")
    UStrategyFacility* HomeBarracks = nullptr;

    /** Which base this soldier is currently stationed at */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Location")
    UStrategyBase* StationedBase = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Loadout")
    TArray<TSoftObjectPtr<UItemDefinition>> CurrentLoadout;

    UFUNCTION(BlueprintCallable, Category = "Soldier|Damage")
    void ApplyDamage(int32 DamageAmount);

    UFUNCTION(BlueprintCallable, Category = "Soldier|Healing")
    bool NeedsHealing() const;

    UFUNCTION(BlueprintCallable, Category = "Soldier|Healing")
    void Heal(int32 Amount);

    UFUNCTION(BlueprintCallable, Category = "Soldier|Healing")
    void UpdateStatusFromHealth();

    UFUNCTION(BlueprintCallable, Category = "Debug")
    void PrintInfo() const;
};