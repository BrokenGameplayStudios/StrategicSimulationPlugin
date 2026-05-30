#include "USoldierManagerSubsystem.h"
#include "UStrategyEventDispatcher.h"
#include "UBaseManagerSubsystem.h"
#include "UStrategyBase.h"
#include "USoldierClassDatabase.h"
#include "UStrategyCampaignSubsystem.h"
#include "Engine/Engine.h"

void USoldierManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    USoldierClassDefinition* DefaultClass = NewObject<USoldierClassDefinition>();
    DefaultClass->ClassName = FText::FromString("Rookie");

    UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();

    // Human initial base (if any)
    UStrategyBase* HumanBase = nullptr;
    if (BaseMgr)
    {
        const TArray<UStrategyBase*>& Bases = BaseMgr->GetBases(EFactionType::Human);
        if (!Bases.IsEmpty())
            HumanBase = Bases[0];
    }
    RecruitSoldier(EFactionType::Human, DefaultClass, HumanBase);

    // Enemy initial base (if any)
    UStrategyBase* EnemyBase = nullptr;
    if (BaseMgr)
    {
        const TArray<UStrategyBase*>& Bases = BaseMgr->GetBases(EFactionType::Enemy);
        if (!Bases.IsEmpty())
            EnemyBase = Bases[0];
    }
    RecruitSoldier(EFactionType::Enemy, DefaultClass, EnemyBase);

    UE_LOG(LogTemp, Display, TEXT("USoldierManagerSubsystem initialized — rosters created for both factions"));
}

UStrategySoldier* USoldierManagerSubsystem::RecruitSoldier(EFactionType Faction, USoldierClassDefinition* ClassDef, UStrategyBase* TargetBase)
{
    if (!ClassDef) return nullptr;

    // Fallback: pick first available base for this faction if none provided
    if (!TargetBase)
    {
        UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
        if (BaseMgr)
        {
            const TArray<UStrategyBase*>& Bases = BaseMgr->GetBases(Faction);
            if (!Bases.IsEmpty())
            {
                TargetBase = Bases[0];
            }
        }
    }

    // === PER-BASE CAPACITY CHECK (strictly per-base now) ===
    if (TargetBase)
    {
        int32 CurrentCapacity = TargetBase->GetTotalCapacityForType(EFacilityType::LivingQuarters);
        int32 CurrentSoldiers = GetNumSoldiersStationedAt(TargetBase, Faction);

        if (CurrentCapacity <= 0)
        {
            UE_LOG(LogTemp, Warning, TEXT("[RECRUIT] %s base '%s' has no operational living quarters (0/%d) — cannot recruit"),
                *UEnum::GetValueAsString(Faction), *TargetBase->BaseName.ToString(), CurrentCapacity);
            return nullptr;
        }

        if (CurrentSoldiers >= CurrentCapacity)
        {
            UE_LOG(LogTemp, Warning, TEXT("[RECRUIT] %s base '%s' barracks full (%d/%d) — cannot recruit"),
                *UEnum::GetValueAsString(Faction), *TargetBase->BaseName.ToString(), CurrentSoldiers, CurrentCapacity);
            return nullptr;
        }
    }
    // (if no base yet, we still allow creation for early-game init)

    // === SOLDIER CREATION (original logic — unchanged) ===
    UStrategySoldier* NewSoldier = NewObject<UStrategySoldier>();
    NewSoldier->SoldierName = FString::Printf(TEXT("%s %s"),
        Faction == EFactionType::Human ? TEXT("Sgt.") : TEXT("Overlord"),
        *ClassDef->ClassName.ToString());
    NewSoldier->ClassDefinition = ClassDef;
    NewSoldier->CurrentStats = ClassDef->BaseStats;
    NewSoldier->XP = ClassDef->StartingXP;

    for (const TSoftObjectPtr<UItemDefinition>& Gear : ClassDef->StartingGear)
    {
        if (UItemDefinition* Item = Gear.Get())
        {
            NewSoldier->CurrentLoadout.Add(Item);
        }
    }

    // NEW: Assign to the target base for location tracking
    NewSoldier->StationedBase = TargetBase;

    if (Faction == EFactionType::Human)
        HumanRoster.Add(NewSoldier);
    else
        EnemyRoster.Add(NewSoldier);

    OnSoldierListChanged.Broadcast(Faction);

    if (UStrategyEventDispatcher* EventDisp = GetGameInstance()->GetSubsystem<UStrategyEventDispatcher>())
        EventDisp->OnSoldierRecruited.Broadcast(Faction, NewSoldier);

    FString BaseNameStr = TargetBase ? TargetBase->BaseName.ToString() : TEXT("No Base");
    UE_LOG(LogTemp, Display, TEXT("Recruited %s for %s at base '%s' with %d starting items"),
        *NewSoldier->SoldierName, *UEnum::GetValueAsString(Faction), *BaseNameStr, NewSoldier->CurrentLoadout.Num());

    return NewSoldier;
}

// === NEW HELPER (add this at the bottom of the file) ===
int32 USoldierManagerSubsystem::GetNumSoldiersStationedAt(UStrategyBase* Base, EFactionType Faction) const
{
    if (!Base) return 0;

    const TArray<UStrategySoldier*>& Roster = (Faction == EFactionType::Human) ? HumanRoster : EnemyRoster;
    int32 Count = 0;
    for (UStrategySoldier* Soldier : Roster)
    {
        if (Soldier && Soldier->StationedBase == Base)
        {
            Count++;
        }
    }
    return Count;
}
void USoldierManagerSubsystem::DismissSoldier(UStrategySoldier* Soldier)
{
    if (!Soldier) return;
    HumanRoster.Remove(Soldier);
    EnemyRoster.Remove(Soldier);

    OnSoldierListChanged.Broadcast(EFactionType::Human);
    OnSoldierListChanged.Broadcast(EFactionType::Enemy);
}

TArray<UStrategySoldier*> USoldierManagerSubsystem::GetRoster(EFactionType Faction) const
{
    return (Faction == EFactionType::Human) ? HumanRoster : EnemyRoster;
}

void USoldierManagerSubsystem::Debug_PrintTeamRoster(EFactionType Faction) const
{
    UE_LOG(LogTemp, Display, TEXT("=== %s TEAM ROSTER ==="), *UEnum::GetValueAsString(Faction));
    const TArray<UStrategySoldier*>& Roster = (Faction == EFactionType::Human) ? HumanRoster : EnemyRoster;
    for (UStrategySoldier* Soldier : Roster)
    {
        if (Soldier) Soldier->PrintInfo();
    }
    UE_LOG(LogTemp, Display, TEXT("=== END %s ROSTER ===\n"), *UEnum::GetValueAsString(Faction));
}

void USoldierManagerSubsystem::FinishSoldierTraining(UStrategyBase* Base, UObject* SoldierClassAsset)
{
    if (!Base) return;

    USoldierClassDefinition* ClassDef = Cast<USoldierClassDefinition>(SoldierClassAsset);
    if (!ClassDef)
    {
        if (UStrategyCampaignSubsystem* Campaign = Base->GetWorld()->GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>())
        {
            if (USoldierClassDatabase* DB = Campaign->SoldierClassDatabaseAsset.Get())
                if (DB->AvailableSoldierClasses.Num() > 0)
                    ClassDef = DB->AvailableSoldierClasses[0].Get();
        }
    }

    UStrategySoldier* NewSoldier = NewObject<UStrategySoldier>();
    NewSoldier->ClassDefinition = ClassDef;
    NewSoldier->CurrentStats.Health = 10;          // Correct field from GitHub
    NewSoldier->HomeBarracks = Base->Facilities.Num() > 0 ? Base->Facilities[0] : nullptr;

    EnemyRoster.Add(NewSoldier);

    UWorld* World = Base->GetWorld();
    if (World)
    {
        if (UStrategyEventDispatcher* Disp = World->GetGameInstance()->GetSubsystem<UStrategyEventDispatcher>())
        {
            Disp->OnSoldierRecruited.Broadcast(EFactionType::Enemy, NewSoldier);   // Matches your current GitHub dispatcher
        }
    }

    UE_LOG(LogTemp, Display, TEXT("[SOLDIER] ✅ New soldier added to roster and UI notified"));
}