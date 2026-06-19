#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "StrategicSimulationTypes.h"
#include "UStrategyEventDispatcher.h"
#include "USoldierManagerSubsystem.generated.h"

/** Game-instance subsystem managing active rosters, POW/KIA/MIA soldiers, and recruitment. */
UCLASS()
class STRATEGICSIMULATIONPLUGIN_API USoldierManagerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /** Registers the subsystem; rosters start empty until recruitment or training completes. */
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /** Creates a soldier from ClassDef, assigns TargetBase (or first faction base), and broadcasts recruitment. */
    UFUNCTION(BlueprintCallable, Category = "Soldier")
    UStrategySoldier* RecruitSoldier(EFactionType Faction, USoldierClassDefinition* ClassDef, UStrategyBase* TargetBase);
        
    /** Resolves SoldierClassAsset to a class definition and recruits via RecruitSoldier. */
    UFUNCTION(BlueprintCallable, Category = "Soldier")
    void FinishSoldierTraining(UStrategyBase* Base, UObject* SoldierClassAsset, EFactionType Faction);

    /** Returns the active (non-POW, non-KIA) roster for Faction. */
    UFUNCTION(BlueprintCallable, Category = "Soldier")
    const TArray<UStrategySoldier*>& GetRoster(EFactionType Faction) const;

    /** Counts roster soldiers whose StationedBase equals Base. */
    UFUNCTION(BlueprintCallable, Category = "Soldier")
    int32 GetNumSoldiersStationedAt(UStrategyBase* Base, EFactionType Faction) const;

    /** True when the soldier can be assigned to a vehicle mission from a base. */
    UFUNCTION(BlueprintPure, Category = "Soldier|Mission")
    static bool IsSoldierEligibleForMission(const UStrategySoldier* Soldier);

    /** Stationed at Base, not on mission, and not KIA/MIA/POW. */
    UFUNCTION(BlueprintCallable, Category = "Soldier|Mission")
    TArray<UStrategySoldier*> GatherMissionReadySoldiersAtBase(UStrategyBase* Base, EFactionType Faction) const;

    /** Broadcasts OnSoldierListChanged with the current roster for Faction. */
    UFUNCTION(BlueprintCallable, Category = "Soldier")
    void BroadcastSoldierListChanged(EFactionType Faction);

    /** Returns captured enemy soldiers held by Faction. */
    UFUNCTION(BlueprintCallable, Category = "POW/KIA")
    const TArray<UStrategySoldier*>& GetPOWRoster(EFactionType Faction) const;

    /** Returns KIA bodies awaiting autopsy for Faction. */
    UFUNCTION(BlueprintCallable, Category = "POW/KIA")
    const TArray<UStrategySoldier*>& GetKIARoster(EFactionType Faction) const;

    /** Moves Soldier from enemy roster to CapturingFaction's POW roster and clears base assignment. */
    UFUNCTION(BlueprintCallable, Category = "POW/KIA")
    void CaptureAsPOW(EFactionType CapturingFaction, UStrategySoldier* Soldier);

    /** Removes Soldier from active roster, adds to KIA roster, and clears mission/base state. */
    UFUNCTION(BlueprintCallable, Category = "POW/KIA")
    void MarkAsKIA(EFactionType Faction, UStrategySoldier* Soldier);

    /** Rolls crash deaths; survivors become MIA at the wreck site. */
    UFUNCTION(BlueprintCallable, Category = "POW/KIA|Salvage")
    void ProcessCrewOnVehicleDestruction(class UStrategyVehicle* Vehicle, class UStrategySiteDefinition* WreckSite);

    /** Removes Soldier from roster, tags as MIA at WreckSite, and registers on the site. */
    UFUNCTION(BlueprintCallable, Category = "POW/KIA|Salvage")
    void MarkAsMIA(UStrategySoldier* Soldier, class UStrategySiteDefinition* WreckSite, EFactionType OwnerFaction);

    /** Owning faction salvage: return MIA soldiers to the given base roster. */
    UFUNCTION(BlueprintCallable, Category = "POW/KIA|Salvage")
    int32 RescueMIAsFromWreck(EFactionType RescuingFaction, class UStrategySiteDefinition* WreckSite, class UStrategyBase* ReturnBase);

    /** Opposing faction salvage (AI dice): MIA may become POW. Player salvage uses contested combat (PR-6b). */
    UFUNCTION(BlueprintCallable, Category = "POW/KIA|Salvage")
    int32 ProcessMIAsOnOpposingSalvage(EFactionType SalvagingFaction, class UStrategySiteDefinition* WreckSite);

private:
    UPROPERTY(VisibleAnywhere, Transient) TArray<UStrategySoldier*> HumanRoster;
    UPROPERTY(VisibleAnywhere, Transient) TArray<UStrategySoldier*> EnemyRoster;

    UPROPERTY(VisibleAnywhere, Transient) TArray<UStrategySoldier*> HumanPOWRoster;
    UPROPERTY(VisibleAnywhere, Transient) TArray<UStrategySoldier*> EnemyPOWRoster;

    UPROPERTY(VisibleAnywhere, Transient) TArray<UStrategySoldier*> HumanKIARoster;
    UPROPERTY(VisibleAnywhere, Transient) TArray<UStrategySoldier*> EnemyKIARoster;
};