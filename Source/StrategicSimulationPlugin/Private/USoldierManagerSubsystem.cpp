#include "USoldierManagerSubsystem.h"
#include "UStrategyEventDispatcher.h"
#include "UBaseManagerSubsystem.h"
#include "UStrategyBase.h"
#include "USoldierClassDatabase.h"
#include "UStrategyCampaignSubsystem.h"
#include "UStrategySoldier.h"          // ← added for safety
#include "USoldierClassDefinition.h"   // ← added for safety
#include "Engine/Engine.h"

void USoldierManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    USoldierClassDefinition* DefaultClass = NewObject<USoldierClassDefinition>();
    DefaultClass->ClassName = FText::FromString("Rookie");

    UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();

    UStrategyBase* HumanBase = nullptr;
    if (BaseMgr && !BaseMgr->GetBases(EFactionType::Human).IsEmpty())
        HumanBase = BaseMgr->GetBases(EFactionType::Human)[0];
    RecruitSoldier(EFactionType::Human, DefaultClass, HumanBase);

    UStrategyBase* EnemyBase = nullptr;
    if (BaseMgr && !BaseMgr->GetBases(EFactionType::Enemy).IsEmpty())
        EnemyBase = BaseMgr->GetBases(EFactionType::Enemy)[0];
    RecruitSoldier(EFactionType::Enemy, DefaultClass, EnemyBase);

    UE_LOG(LogTemp, Display, TEXT("USoldierManagerSubsystem initialized — rosters created for both factions"));
}

UStrategySoldier* USoldierManagerSubsystem::RecruitSoldier(EFactionType Faction, USoldierClassDefinition* ClassDef, UStrategyBase* TargetBase)
{
    if (!ClassDef) return nullptr;

    if (!TargetBase)
    {
        UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
        if (BaseMgr)
        {
            const TArray<UStrategyBase*>& Bases = BaseMgr->GetBases(Faction);
            if (!Bases.IsEmpty()) TargetBase = Bases[0];
        }
    }

    // === RESTORED: Cannot recruit to dead / no-power base + capacity checks (kept exactly as you wrote) ===
    if (TargetBase)
    {
        if (!TargetBase->IsOperational() || TargetBase->GetNetPower() < 0)
        {
            UE_LOG(LogTemp, Verbose, TEXT("[RECRUIT] %s cannot recruit — base '%s' is not operational or has no power (%d)"),
                *UEnum::GetValueAsString(Faction), *TargetBase->BaseName.ToString(), TargetBase->GetNetPower());
            return nullptr;
        }

        int32 CurrentCapacity = TargetBase->GetTotalCapacityForType(EFacilityType::LivingQuarters);
        int32 CurrentSoldiers = GetNumSoldiersStationedAt(TargetBase, Faction);

        if (CurrentCapacity <= 0)
        {
            UE_LOG(LogTemp, Verbose, TEXT("[RECRUIT] %s base '%s' has no operational living quarters (0/%d)"),
                *UEnum::GetValueAsString(Faction), *TargetBase->BaseName.ToString(), CurrentCapacity);
            return nullptr;
        }

        if (CurrentSoldiers >= CurrentCapacity)
        {
            UE_LOG(LogTemp, Verbose, TEXT("[RECRUIT] %s base '%s' barracks full (%d/%d)"),
                *UEnum::GetValueAsString(Faction), *TargetBase->BaseName.ToString(), CurrentSoldiers, CurrentCapacity);
            return nullptr;
        }
    }

    UStrategySoldier* NewSoldier = NewObject<UStrategySoldier>(this); // outer = subsystem for proper lifetime
    NewSoldier->SoldierName = FString::Printf(TEXT("%s %s"),
        Faction == EFactionType::Human ? TEXT("Sgt.") : TEXT("Overlord"),
        *ClassDef->ClassName.ToString());
    NewSoldier->ClassDefinition = ClassDef;
    NewSoldier->CurrentStats = ClassDef->BaseStats;
    NewSoldier->XP = ClassDef->StartingXP;
    NewSoldier->StationedBase = TargetBase;

    for (const TSoftObjectPtr<UItemDefinition>& Gear : ClassDef->StartingGear)
        if (UItemDefinition* Item = Gear.Get()) NewSoldier->CurrentLoadout.Add(Item);

    if (Faction == EFactionType::Human)
        HumanRoster.Add(NewSoldier);
    else
        EnemyRoster.Add(NewSoldier);

    BroadcastSoldierListChanged(Faction);

    if (UStrategyEventDispatcher* EventDisp = GetGameInstance()->GetSubsystem<UStrategyEventDispatcher>())
        EventDisp->OnSoldierRecruited.Broadcast(Faction, NewSoldier);

    FString BaseNameStr = TargetBase ? TargetBase->BaseName.ToString() : TEXT("No Base");
    UE_LOG(LogTemp, Display, TEXT("Recruited %s for %s at base '%s' with %d starting items"),
        *NewSoldier->SoldierName, *UEnum::GetValueAsString(Faction), *BaseNameStr, NewSoldier->CurrentLoadout.Num());

    return NewSoldier;
}

void USoldierManagerSubsystem::FinishSoldierTraining(UStrategyBase* Base, UObject* SoldierClassAsset, EFactionType Faction)
{
    if (!SoldierClassAsset) return;

    USoldierClassDefinition* ClassDef = Cast<USoldierClassDefinition>(SoldierClassAsset);
    if (!ClassDef)
    {
        if (UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>())
        {
            if (USoldierClassDatabase* DB = Campaign->SoldierClassDatabaseAsset.Get())
                if (DB->AvailableSoldierClasses.Num() > 0)
                    ClassDef = DB->AvailableSoldierClasses[0].Get();
        }
    }

    if (!ClassDef)
    {
        UE_LOG(LogTemp, Warning, TEXT("[FINISH TRAINING] No valid SoldierClassDefinition found — aborting"));
        return;
    }

    UStrategySoldier* NewSoldier = RecruitSoldier(Faction, ClassDef, Base);   // ← NOW USES CORRECT FACTION

    if (NewSoldier)
    {
        if (UStrategyEventDispatcher* Disp = GetGameInstance()->GetSubsystem<UStrategyEventDispatcher>())
        {
            Disp->OnSoldierListChanged.Broadcast(Faction, (Faction == EFactionType::Human) ? HumanRoster : EnemyRoster);
            Disp->OnSoldierRecruited.Broadcast(Faction, NewSoldier);
            Disp->OnSoldierLoadoutChanged.Broadcast(Faction, NewSoldier);
        }

        UE_LOG(LogTemp, Display, TEXT("[SOLDIER] %s soldier trained and added to roster"), *UEnum::GetValueAsString(Faction));
    }
}

void USoldierManagerSubsystem::DismissSoldier(UStrategySoldier* Soldier)
{
    if (!Soldier) return;
    HumanRoster.Remove(Soldier);
    EnemyRoster.Remove(Soldier);
    BroadcastSoldierListChanged(EFactionType::Human);
    BroadcastSoldierListChanged(EFactionType::Enemy);
}

int32 USoldierManagerSubsystem::GetNumSoldiersStationedAt(UStrategyBase* Base, EFactionType Faction) const
{
    if (!Base) return 0;
    const TArray<UStrategySoldier*>& Roster = (Faction == EFactionType::Human) ? HumanRoster : EnemyRoster;
    int32 Count = 0;
    for (UStrategySoldier* S : Roster)
        if (S && S->StationedBase == Base) Count++;
    return Count;
}

const TArray<UStrategySoldier*>& USoldierManagerSubsystem::GetRoster(EFactionType Faction) const
{
    return (Faction == EFactionType::Human) ? HumanRoster : EnemyRoster;
}

void USoldierManagerSubsystem::Debug_PrintTeamRoster(EFactionType Faction) const
{
    UE_LOG(LogTemp, Display, TEXT("=== %s TEAM ROSTER ==="), *UEnum::GetValueAsString(Faction));
    const TArray<UStrategySoldier*>& Roster = (Faction == EFactionType::Human) ? HumanRoster : EnemyRoster;
    for (UStrategySoldier* S : Roster)
        if (S) S->PrintInfo();
    UE_LOG(LogTemp, Display, TEXT("=== END %s ROSTER ===\n"), *UEnum::GetValueAsString(Faction));
}

void USoldierManagerSubsystem::BroadcastSoldierListChanged(EFactionType Faction)
{
    if (UStrategyEventDispatcher* Disp = GetGameInstance()->GetSubsystem<UStrategyEventDispatcher>())
    {
        Disp->OnSoldierListChanged.Broadcast(Faction, Faction == EFactionType::Enemy ? EnemyRoster : HumanRoster);
    }
}