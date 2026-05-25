#include "USoldierManagerSubsystem.h"
#include "UStrategyEventDispatcher.h"
#include "Engine/Engine.h"

void USoldierManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    USoldierClassDefinition* DefaultClass = NewObject<USoldierClassDefinition>();
    DefaultClass->ClassName = FText::FromString("Rookie");

    RecruitSoldier(EFactionType::Human, DefaultClass);
    RecruitSoldier(EFactionType::Enemy, DefaultClass);

    UE_LOG(LogTemp, Display, TEXT("USoldierManagerSubsystem initialized — rosters created for both factions"));
}

UStrategySoldier* USoldierManagerSubsystem::RecruitSoldier(EFactionType Faction, USoldierClassDefinition* ClassDef)
{
    if (!ClassDef) return nullptr;

    UStrategySoldier* NewSoldier = NewObject<UStrategySoldier>();
    NewSoldier->SoldierName = FString::Printf(TEXT("%s %s"),
        Faction == EFactionType::Human ? TEXT("Sgt.") : TEXT("Overlord"),
        *ClassDef->ClassName.ToString());
    NewSoldier->ClassDefinition = ClassDef;
    NewSoldier->CurrentStats = ClassDef->BaseStats;
    NewSoldier->XP = ClassDef->StartingXP;

    // Copy starting gear from the class definition
    for (const TSoftObjectPtr<UItemDefinition>& Gear : ClassDef->StartingGear)
    {
        if (UItemDefinition* Item = Gear.Get())
        {
            NewSoldier->CurrentLoadout.Add(Item);
        }
    }

    if (Faction == EFactionType::Human)
        HumanRoster.Add(NewSoldier);
    else
        EnemyRoster.Add(NewSoldier);

    OnSoldierListChanged.Broadcast(Faction);

    if (UStrategyEventDispatcher* EventDisp = GetGameInstance()->GetSubsystem<UStrategyEventDispatcher>())
        EventDisp->OnSoldierRecruited.Broadcast(Faction, NewSoldier);

    UE_LOG(LogTemp, Display, TEXT("Recruited %s for %s with %d starting items"),
        *NewSoldier->SoldierName, *UEnum::GetValueAsString(Faction), NewSoldier->CurrentLoadout.Num());

    return NewSoldier;
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