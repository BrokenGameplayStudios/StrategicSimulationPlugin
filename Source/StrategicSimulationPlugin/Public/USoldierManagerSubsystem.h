#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "StrategicSimulationTypes.h"
#include "UStrategySoldier.h"
#include "USoldierClassDefinition.h"
#include "Delegates/DelegateCombinations.h"
#include "USoldierManagerSubsystem.generated.h"

// Forward declaration — fixes circular dependency with UStrategyBase / CampaignSubsystem
class UStrategyBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSoldierListChanged, EFactionType, Faction);

UCLASS()
class STRATEGICSIMULATIONPLUGIN_API USoldierManagerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /**
     * Recruit a soldier for the given faction and class definition to a SPECIFIC base.
     * TargetBase is now required for per-base barracks capacity checks.
     * Falls back gracefully if nullptr (uses first available base for that faction).
     */
    UFUNCTION(BlueprintCallable, Category = "Soldiers")
    UStrategySoldier* RecruitSoldier(EFactionType Faction, USoldierClassDefinition* ClassDef, UStrategyBase* TargetBase = nullptr);

    UFUNCTION(BlueprintCallable, Category = "Soldiers")
    void DismissSoldier(UStrategySoldier* Soldier);

    UFUNCTION(BlueprintCallable, Category = "Soldiers")
    TArray<UStrategySoldier*> GetRoster(EFactionType Faction) const;

    UFUNCTION(BlueprintCallable, Category = "Debug")
    void Debug_PrintTeamRoster(EFactionType Faction) const;

    /** Returns the number of soldiers currently stationed at a specific base (per-base capacity helper) */
    UFUNCTION(BlueprintCallable, Category = "Soldiers")
    int32 GetNumSoldiersStationedAt(UStrategyBase* Base, EFactionType Faction) const;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnSoldierListChanged OnSoldierListChanged;

private:
    UPROPERTY(VisibleAnywhere, Transient, Category = "Soldiers")
    TArray<UStrategySoldier*> HumanRoster;

    UPROPERTY(VisibleAnywhere, Transient, Category = "Soldiers")
    TArray<UStrategySoldier*> EnemyRoster;
};