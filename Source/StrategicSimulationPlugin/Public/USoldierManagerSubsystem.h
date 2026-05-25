#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "StrategicSimulationTypes.h"
#include "UStrategySoldier.h"
#include "USoldierClassDefinition.h"
#include "Delegates/DelegateCombinations.h"
#include "USoldierManagerSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSoldierListChanged, EFactionType, Faction);

UCLASS()
class STRATEGICSIMULATIONPLUGIN_API USoldierManagerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    UFUNCTION(BlueprintCallable, Category = "Soldiers")
    UStrategySoldier* RecruitSoldier(EFactionType Faction, USoldierClassDefinition* ClassDef);

    UFUNCTION(BlueprintCallable, Category = "Soldiers")
    void DismissSoldier(UStrategySoldier* Soldier);

    UFUNCTION(BlueprintCallable, Category = "Soldiers")
    TArray<UStrategySoldier*> GetRoster(EFactionType Faction) const;

    UFUNCTION(BlueprintCallable, Category = "Debug")
    void Debug_PrintTeamRoster(EFactionType Faction) const;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnSoldierListChanged OnSoldierListChanged;

private:
    UPROPERTY(VisibleAnywhere, Transient, Category = "Soldiers")
    TArray<UStrategySoldier*> HumanRoster;

    UPROPERTY(VisibleAnywhere, Transient, Category = "Soldiers")
    TArray<UStrategySoldier*> EnemyRoster;
};