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

    // === NEW: POW/KIA SYSTEM (Phase 1) ===
    UFUNCTION(BlueprintCallable, Category = "POW/KIA")
    const TArray<UStrategySoldier*>& GetPOWRoster(EFactionType Faction) const;

    UFUNCTION(BlueprintCallable, Category = "POW/KIA")
    void CaptureAsPOW(EFactionType CapturingFaction, UStrategySoldier* Soldier);   // moves soldier to POW roster + frees slot

    UFUNCTION(BlueprintCallable, Category = "POW/KIA")
    void MarkAsKIA(EFactionType Faction, UStrategySoldier* Soldier);               // removes permanently

    UFUNCTION(BlueprintCallable, Category = "POW/KIA")
    void ReleasePOW(UStrategySoldier* POW);  // future-proof (trade/recruit later)

private:

    // Exsisting Rosters
    UPROPERTY(VisibleAnywhere, Transient) TArray<UStrategySoldier*> HumanRoster;
    UPROPERTY(VisibleAnywhere, Transient) TArray<UStrategySoldier*> EnemyRoster;

    // NEW POW rosters (separate, per faction)
    UPROPERTY(VisibleAnywhere, Transient) TArray<UStrategySoldier*> HumanPOWRoster;
    UPROPERTY(VisibleAnywhere, Transient) TArray<UStrategySoldier*> EnemyPOWRoster;
};