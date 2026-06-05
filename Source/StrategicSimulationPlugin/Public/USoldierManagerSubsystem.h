#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "StrategicSimulationTypes.h"
#include "UStrategyEventDispatcher.h"
#include "USoldierManagerSubsystem.generated.h"

UCLASS()
class STRATEGICSIMULATIONPLUGIN_API USoldierManagerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    UFUNCTION(BlueprintCallable, Category = "Soldier")
    UStrategySoldier* RecruitSoldier(EFactionType Faction, USoldierClassDefinition* ClassDef, UStrategyBase* TargetBase);
        
    UFUNCTION(BlueprintCallable, Category = "Soldier")
    UStrategySoldier* GetCommander(EFactionType Faction) const;

    UFUNCTION(BlueprintCallable, Category = "Soldier")
    void FinishSoldierTraining(UStrategyBase* Base, UObject* SoldierClassAsset, EFactionType Faction);

    UFUNCTION(BlueprintCallable, Category = "Soldier")
    void DismissSoldier(UStrategySoldier* Soldier);

    UFUNCTION(BlueprintCallable, Category = "Soldier")
    const TArray<UStrategySoldier*>& GetRoster(EFactionType Faction) const;

    UFUNCTION(BlueprintCallable, Category = "Soldier")
    int32 GetNumSoldiersStationedAt(UStrategyBase* Base, EFactionType Faction) const;

    UFUNCTION(BlueprintCallable, Category = "Soldier")
    void BroadcastSoldierListChanged(EFactionType Faction);

    UFUNCTION(BlueprintCallable, Category = "Debug")
    void Debug_PrintTeamRoster(EFactionType Faction) const;

private:
    UPROPERTY(VisibleAnywhere, Transient)
    TArray<UStrategySoldier*> HumanRoster;

    UPROPERTY(VisibleAnywhere, Transient)
    TArray<UStrategySoldier*> EnemyRoster;
};