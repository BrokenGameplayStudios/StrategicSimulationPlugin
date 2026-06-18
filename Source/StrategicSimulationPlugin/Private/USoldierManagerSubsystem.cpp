#include "USoldierManagerSubsystem.h"
#include "UStrategyVehicle.h"
#include "StrategicSiteDefinition.h"
#include "UStrategyCampaignSubsystem.h"
#include "UStrategyBase.h"
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

// === NEW FUNCTION: USoldierManagerSubsystem::GetCommander ===
UStrategySoldier* USoldierManagerSubsystem::GetCommander(EFactionType Faction) const
{
    const TArray<UStrategySoldier*>& Roster = GetRoster(Faction);

    for (UStrategySoldier* Soldier : Roster)
    {
        if (Soldier && Soldier->ClassDefinition &&
            Soldier->ClassDefinition->ClassName.ToString().Contains("Commander"))
        {
            return Soldier;
        }
    }
    return nullptr;
}

// === POW/KIA FUNCTIONS (updated for Phase 3) ===

const TArray<UStrategySoldier*>& USoldierManagerSubsystem::GetPOWRoster(EFactionType Faction) const
{
    return (Faction == EFactionType::Human) ? HumanPOWRoster : EnemyPOWRoster;
}

const TArray<UStrategySoldier*>& USoldierManagerSubsystem::GetKIARoster(EFactionType Faction) const   // NEW
{
    return (Faction == EFactionType::Human) ? HumanKIARoster : EnemyKIARoster;
}

void USoldierManagerSubsystem::CaptureAsPOW(EFactionType CapturingFaction, UStrategySoldier* Soldier)
{
    if (!Soldier) return;

    if (CapturingFaction == EFactionType::Human)
    {
        EnemyRoster.Remove(Soldier);
        HumanPOWRoster.Add(Soldier);
    }
    else
    {
        HumanRoster.Remove(Soldier);
        EnemyPOWRoster.Add(Soldier);
    }

    Soldier->bIsPOW = true;
    Soldier->StationedBase = nullptr;

    BroadcastSoldierListChanged(CapturingFaction == EFactionType::Human ? EFactionType::Human : EFactionType::Enemy);
    BroadcastSoldierListChanged(CapturingFaction == EFactionType::Human ? EFactionType::Enemy : EFactionType::Human);

    UE_LOG(LogTemp, Display, TEXT("[POW] %s captured enemy soldier '%s' as POW!"),
        *UEnum::GetValueAsString(CapturingFaction), *Soldier->SoldierName);
}

void USoldierManagerSubsystem::MarkAsKIA(EFactionType Faction, UStrategySoldier* Soldier)   // UPDATED
{
    if (!Soldier) return;

    // Remove from active roster
    if (Faction == EFactionType::Human)
        HumanRoster.Remove(Soldier);
    else
        EnemyRoster.Remove(Soldier);

    // Move to KIA roster (bodies stored for autopsy)
    if (Faction == EFactionType::Human)
        HumanKIARoster.Add(Soldier);
    else
        EnemyKIARoster.Add(Soldier);

    Soldier->bIsKIA = true;
    Soldier->bIsMIA = false;
    Soldier->WreckSiteId = FGuid();
    Soldier->StationedBase = nullptr;

    BroadcastSoldierListChanged(Faction);
    UE_LOG(LogTemp, Display, TEXT("[KIA] %s soldier '%s' marked KIA and added to recovery roster"),
        *UEnum::GetValueAsString(Faction), *Soldier->SoldierName);
}

void USoldierManagerSubsystem::ReleasePOW(UStrategySoldier* POW)
{
    if (!POW || !POW->bIsPOW) return;
    UE_LOG(LogTemp, Display, TEXT("[POW] POW '%s' released (placeholder)"), *POW->SoldierName);
}

void USoldierManagerSubsystem::MarkAsMIA(UStrategySoldier* Soldier, UStrategySiteDefinition* WreckSite, EFactionType OwnerFaction)
{
    if (!Soldier || !WreckSite)
    {
        return;
    }

    const EFactionType Faction = OwnerFaction;
    if (Faction == EFactionType::Human)
    {
        HumanRoster.Remove(Soldier);
    }
    else if (Faction == EFactionType::Enemy)
    {
        EnemyRoster.Remove(Soldier);
    }

    Soldier->bIsMIA = true;
    Soldier->bIsKIA = false;
    Soldier->bIsPOW = false;
    Soldier->WreckSiteId = WreckSite->SiteId;
    Soldier->CurrentMission = nullptr;
    Soldier->StationedBase = nullptr;
    Soldier->Status = ESoldierStatus::Wounded;

    WreckSite->MIASoldiers.AddUnique(Soldier);

    if (Faction == EFactionType::Human || Faction == EFactionType::Enemy)
    {
        BroadcastSoldierListChanged(Faction);
    }

    UE_LOG(LogTemp, Display, TEXT("[MIA] %s soldier '%s' missing at wreck '%s'"),
        *UEnum::GetValueAsString(Faction), *Soldier->SoldierName, *WreckSite->SiteName);
}

void USoldierManagerSubsystem::ProcessCrewOnVehicleDestruction(UStrategyVehicle* Vehicle, UStrategySiteDefinition* WreckSite)
{
    if (!Vehicle || !WreckSite)
    {
        return;
    }

    float CrashDeathChance = 0.25f;
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UStrategyCampaignSubsystem* Campaign = GI->GetSubsystem<UStrategyCampaignSubsystem>())
        {
            CrashDeathChance = Campaign->VehicleCrashDeathChance;
        }
    }

    EFactionType OwnerFaction = EFactionType::Neutral;
    if (Vehicle->HomeBase)
    {
        OwnerFaction = Vehicle->HomeBase->OwningFaction;
    }

    TArray<UStrategySoldier*> Passengers = Vehicle->CurrentPassengers;
    for (UStrategySoldier* Soldier : Passengers)
    {
        if (!Soldier)
        {
            continue;
        }

        Soldier->CurrentMission = nullptr;

        if (FMath::FRand() < CrashDeathChance)
        {
            if (OwnerFaction != EFactionType::Neutral)
            {
                MarkAsKIA(OwnerFaction, Soldier);
            }
            WreckSite->KIACrashCount++;
            UE_LOG(LogTemp, Display, TEXT("[SALVAGE] %s died in vehicle destruction at wreck '%s'"),
                *Soldier->SoldierName, *WreckSite->SiteName);
        }
        else
        {
            MarkAsMIA(Soldier, WreckSite, OwnerFaction);
        }
    }

    UE_LOG(LogTemp, Display, TEXT("[SALVAGE] Crew processed at '%s' — KIA crash: %d | MIA: %d"),
        *WreckSite->SiteName, WreckSite->KIACrashCount, WreckSite->MIASoldiers.Num());
}

int32 USoldierManagerSubsystem::RescueMIAsFromWreck(EFactionType RescuingFaction, UStrategySiteDefinition* WreckSite, UStrategyBase* ReturnBase)
{
    if (!WreckSite || !ReturnBase || ReturnBase->OwningFaction != RescuingFaction)
    {
        return 0;
    }

    int32 Rescued = 0;
    TArray<UStrategySoldier*> ToRescue = WreckSite->MIASoldiers;

    for (UStrategySoldier* Soldier : ToRescue)
    {
        if (!Soldier || !Soldier->bIsMIA)
        {
            continue;
        }

        if (WreckSite->WreckOwnerFaction != RescuingFaction)
        {
            continue;
        }

        Soldier->bIsMIA = false;
        Soldier->WreckSiteId = FGuid();
        Soldier->StationedBase = ReturnBase;
        Soldier->Status = ESoldierStatus::Wounded;
        Soldier->bIsWounded = true;
        Soldier->DaysUntilRecovered = 3;

        if (RescuingFaction == EFactionType::Human)
        {
            HumanRoster.AddUnique(Soldier);
        }
        else
        {
            EnemyRoster.AddUnique(Soldier);
        }

        WreckSite->MIASoldiers.Remove(Soldier);
        Rescued++;

        UE_LOG(LogTemp, Display, TEXT("[MIA] Rescued '%s' from wreck '%s' → base '%s'"),
            *Soldier->SoldierName, *WreckSite->SiteName, *ReturnBase->BaseName.ToString());
    }

    if (Rescued > 0)
    {
        BroadcastSoldierListChanged(RescuingFaction);
    }

    return Rescued;
}

int32 USoldierManagerSubsystem::ProcessMIAsOnOpposingSalvage(EFactionType SalvagingFaction, UStrategySiteDefinition* WreckSite)
{
    if (!WreckSite || SalvagingFaction == WreckSite->WreckOwnerFaction)
    {
        return 0;
    }

    float POWChance = 0.40f;
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UStrategyCampaignSubsystem* Campaign = GI->GetSubsystem<UStrategyCampaignSubsystem>())
        {
            POWChance = Campaign->OpposingSalvageMIAPOWChance;
        }
    }

    int32 Captured = 0;
    TArray<UStrategySoldier*> RemainingMIA = WreckSite->MIASoldiers;

    for (UStrategySoldier* Soldier : RemainingMIA)
    {
        if (!Soldier || !Soldier->bIsMIA)
        {
            continue;
        }

        EFactionType SoldierFaction = WreckSite->WreckOwnerFaction;
        if (SoldierFaction == SalvagingFaction)
        {
            continue;
        }

        if (FMath::FRand() < POWChance)
        {
            Soldier->bIsMIA = false;
            Soldier->WreckSiteId = FGuid();
            WreckSite->MIASoldiers.Remove(Soldier);
            CaptureAsPOW(SalvagingFaction, Soldier);
            Captured++;
        }
    }

    UE_LOG(LogTemp, Display, TEXT("[SALVAGE] Opposing salvage at '%s' — %d MIA captured as POW by %s"),
        *WreckSite->SiteName, Captured, *UEnum::GetValueAsString(SalvagingFaction));

    return Captured;
}