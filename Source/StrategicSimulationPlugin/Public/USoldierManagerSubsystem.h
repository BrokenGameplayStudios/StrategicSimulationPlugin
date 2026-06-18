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

    // === POW / KIA (Phase 1 + 3) ===
    UFUNCTION(BlueprintCallable, Category = "POW/KIA")
    const TArray<UStrategySoldier*>& GetPOWRoster(EFactionType Faction) const;

    UFUNCTION(BlueprintCallable, Category = "POW/KIA")
    const TArray<UStrategySoldier*>& GetKIARoster(EFactionType Faction) const;   // NEW

    UFUNCTION(BlueprintCallable, Category = "POW/KIA")
    void CaptureAsPOW(EFactionType CapturingFaction, UStrategySoldier* Soldier);

    UFUNCTION(BlueprintCallable, Category = "POW/KIA")
    void MarkAsKIA(EFactionType Faction, UStrategySoldier* Soldier);   // now moves to KIA roster

    UFUNCTION(BlueprintCallable, Category = "POW/KIA")
    void ReleasePOW(UStrategySoldier* POW);

    /** Rolls crash deaths; survivors become MIA at the wreck site. */
    UFUNCTION(BlueprintCallable, Category = "POW/KIA|Salvage")
    void ProcessCrewOnVehicleDestruction(class UStrategyVehicle* Vehicle, class UStrategySiteDefinition* WreckSite);

    UFUNCTION(BlueprintCallable, Category = "POW/KIA|Salvage")
    void MarkAsMIA(UStrategySoldier* Soldier, class UStrategySiteDefinition* WreckSite, EFactionType OwnerFaction);

    /** Owning faction salvage: return MIA soldiers to the given base roster. */
    UFUNCTION(BlueprintCallable, Category = "POW/KIA|Salvage")
    int32 RescueMIAsFromWreck(EFactionType RescuingFaction, class UStrategySiteDefinition* WreckSite, class UStrategyBase* ReturnBase);

    /** Opposing faction salvage (AI dice): MIA may become POW. Player salvage uses contested combat (PR-6b). */
    UFUNCTION(BlueprintCallable, Category = "POW/KIA|Salvage")
    int32 ProcessMIAsOnOpposingSalvage(EFactionType SalvagingFaction, class UStrategySiteDefinition* WreckSite);

private:

    // Exsisting Rosters
    UPROPERTY(VisibleAnywhere, Transient) TArray<UStrategySoldier*> HumanRoster;
    UPROPERTY(VisibleAnywhere, Transient) TArray<UStrategySoldier*> EnemyRoster;

    // NEW POW rosters (separate, per faction)
    UPROPERTY(VisibleAnywhere, Transient) TArray<UStrategySoldier*> HumanPOWRoster;
    UPROPERTY(VisibleAnywhere, Transient) TArray<UStrategySoldier*> EnemyPOWRoster;

    // NEW: KIA bodies (stored until autopsied)
    UPROPERTY(VisibleAnywhere, Transient) TArray<UStrategySoldier*> HumanKIARoster;
    UPROPERTY(VisibleAnywhere, Transient) TArray<UStrategySoldier*> EnemyKIARoster;
};