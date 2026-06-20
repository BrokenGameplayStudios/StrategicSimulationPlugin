#include "UBaseManagerSubsystem.h"
#include "UStrategyVehicle.h"
#include "UVehicleDefinition.h"
#include "UStrategyEventDispatcher.h"
#include "UFactionIntelSubsystem.h"
#include "UMissionManagerSubsystem.h"
#include "Engine/Engine.h"
#include "UFacilityDatabase.h"
#include "UResourceManagerSubsystem.h"
#include "UStrategyCampaignSubsystem.h"
#include "UStrategyBase.h"
#include "UStrategyFacility.h"
#include "USoldierManagerSubsystem.h"
#include "UMissionManagerSubsystem.h"
#include "UTimeManagerSubsystem.h"
#include "UStrategySaveGame.h"
#include "UStrategicSimulationDisplayHelpers.h"
#include "UExplorationSubsystem.h"

namespace SiteGenerationHelpers
{
    /** Assigns randomized mineable resource pools and site type to a new site. */
    void PopulateMineableResources(UStrategySiteDefinition* Site)
    {
        if (!Site)
        {
            return;
        }

        const float Richness = FMath::FRandRange(0.75f, 1.35f);
        const bool bResourceRich = FMath::FRand() < 0.25f;

        if (bResourceRich)
        {
            Site->SiteType = EStrategySiteType::ResourceNode;
            Site->MaxResources.Metals = FMath::RoundToInt(FMath::RandRange(500, 900) * Richness);
            Site->MaxResources.Chemicals = FMath::RoundToInt(FMath::RandRange(250, 500) * Richness);
            Site->MaxResources.Biologicals = FMath::RoundToInt(FMath::RandRange(80, 220) * Richness);
            Site->MaxResources.Money = FMath::RoundToInt(FMath::RandRange(150, 350) * Richness);
            Site->MaxResources.ExoticMaterial = FMath::RoundToInt(FMath::RandRange(20, 80) * Richness);
        }
        else
        {
            Site->SiteType = EStrategySiteType::PotentialBase;
            Site->MaxResources.Metals = FMath::RoundToInt(FMath::RandRange(200, 450) * Richness);
            Site->MaxResources.Chemicals = FMath::RoundToInt(FMath::RandRange(100, 280) * Richness);
            Site->MaxResources.Biologicals = FMath::RoundToInt(FMath::RandRange(40, 160) * Richness);
            Site->MaxResources.Money = FMath::RoundToInt(FMath::RandRange(120, 320) * Richness);
            Site->MaxResources.ExoticMaterial = FMath::RoundToInt(FMath::RandRange(0, 40) * Richness);
        }

        Site->CurrentResources = Site->MaxResources;
    }
}

/** Binds to UTimeManagerSubsystem::OnDayPassed and logs initialization. */
void UBaseManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Collection.InitializeDependency<UTimeManagerSubsystem>();
    Super::Initialize(Collection);

    if (UTimeManagerSubsystem* TimeMgr = GetGameInstance()->GetSubsystem<UTimeManagerSubsystem>())
    {
        TimeMgr->OnDayPassed.AddDynamic(this, &UBaseManagerSubsystem::OnDayPassed);
    }

    UE_LOG(LogTemp, Display, TEXT("UBaseManagerSubsystem initialized — multiple-base systems online"));
}

/**
 * Creates a base, links it to an optional site, and spawns an initial Command Center.
 * First base per faction also recruits the Commander from index 0 of the soldier class database.
 */
UStrategyBase* UBaseManagerSubsystem::BuildNewBase(EFactionType Faction, FText BaseName, FVector2D MapLocation, UStrategySiteDefinition* Site)
{
    UStrategyBase* NewBase = NewObject<UStrategyBase>(this);
    NewBase->OwningFaction = Faction;   
    NewBase->BaseName = BaseName.IsEmpty() ? FText::FromString("New Base") : BaseName;
    NewBase->MapLocation = MapLocation;
    NewBase->BuiltOnSite = Site;  

    if (Site)
    {
        Site->bHasBeenUsed = true;
        if (UExplorationSubsystem* Exploration = GetGameInstance()->GetSubsystem<UExplorationSubsystem>())
        {
            Exploration->MarkSiteSurveyed(Faction, Site);
        }
    }

    if (Faction == EFactionType::Human)
        HumanBases.Add(NewBase);
    else
        EnemyBases.Add(NewBase);

    OnBaseListChanged.Broadcast(Faction);
    OnFacilityListChanged.Broadcast(Faction);

    UE_LOG(LogTemp, Display, TEXT("Built new base '%s' for %s at (%.0f, %.0f)"),
        *NewBase->BaseName.ToString(), *UEnum::GetValueAsString(Faction), MapLocation.X, MapLocation.Y);

    // === SPECIAL CASE: Initial Command Center + Commander ===
    bool bIsInitialBase = (Faction == EFactionType::Enemy && EnemyBases.Num() == 1) ||
        (Faction == EFactionType::Human && HumanBases.Num() == 1);

    if (bIsInitialBase)
    {
        // 1. Create the Command Center facility
        UStrategyFacility* CommandFacility = NewObject<UStrategyFacility>(this);

        if (UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>())
        {
            if (UFacilityDatabase* FacilityDB = Campaign->FacilityDatabaseAsset.Get())
            {
                for (const TSoftObjectPtr<UFacilityDefinition>& SoftDef : FacilityDB->AvailableFacilities)
                {
                    if (UFacilityDefinition* Def = SoftDef.Get())
                    {
                        if (Def->FacilityType == EFacilityType::Command)
                        {
                            CommandFacility->FacilityDefinition = Def;
                            break;
                        }
                    }
                }
            }
        }

        CommandFacility->BuildProgressDays = 0;
        CommandFacility->bIsOperational = true;
        CommandFacility->CurrentPowerDraw = 0;
        NewBase->AddFacility(CommandFacility);
        NewBase->UpdatePowerFromFacilities();

        UE_LOG(LogTemp, Display, TEXT("[FACILITY] Initial Command Center is NOW OPERATIONAL in base '%s'"),
            *NewBase->BaseName.ToString());

        // 2. Spawn the Commander (first class in SoldierClassDatabase = DA_Sol_Commander)
        UStrategyCampaignSubsystem* CampaignSub = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();
        USoldierManagerSubsystem* SoldierMgr = GetGameInstance()->GetSubsystem<USoldierManagerSubsystem>();

        if (CampaignSub && SoldierMgr && CampaignSub->SoldierClassDatabaseAsset.IsValid())
        {
            if (USoldierClassDatabase* DB = CampaignSub->SoldierClassDatabaseAsset.Get())
            {
                if (!DB->AvailableSoldierClasses.IsEmpty())
                {
                    USoldierClassDefinition* CommanderClass = DB->AvailableSoldierClasses[0].Get(); // index 0 = Commander

                    if (CommanderClass)
                    {
                        UStrategySoldier* Commander = SoldierMgr->RecruitSoldier(Faction, CommanderClass, NewBase);

                        if (Commander)
                        {
                            Commander->SoldierName = (Faction == EFactionType::Human)
                                ? FString("Sgt. Commander")
                                : FString("Overlord Commander");

                            UE_LOG(LogTemp, Display, TEXT("[COMMANDER] %s spawned Commander (%s) in initial base '%s'"),
                                *UEnum::GetValueAsString(Faction), *CommanderClass->ClassName.ToString(), *NewBase->BaseName.ToString());
                        }
                    }
                }
            }
        }

        if (UStrategyEventDispatcher* EventDisp = GetGameInstance()->GetSubsystem<UStrategyEventDispatcher>())
            EventDisp->OnFacilityCompleted.Broadcast(Faction, CommandFacility);

        UE_LOG(LogTemp, Display, TEXT("[AI] Initial 'Command Center' base + Commander created successfully"));
        return NewBase;
    }

    // === NORMAL PAID EXPANSION (cost deducted once inside BuildFacility) ===
    UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();

    UFacilityDefinition* CommandDef = nullptr;
    if (Campaign)
    {
        if (UFacilityDatabase* FacilityDB = Campaign->FacilityDatabaseAsset.Get())
        {
            for (const TSoftObjectPtr<UFacilityDefinition>& SoftDef : FacilityDB->AvailableFacilities)
            {
                if (UFacilityDefinition* Def = SoftDef.Get())
                {
                    if (Def->FacilityType == EFacilityType::Command)
                    {
                        CommandDef = Def;
                        break;
                    }
                }
            }
        }
    }

    auto AbortExpansionBase = [&](const TCHAR* Reason)
    {
        UE_LOG(LogTemp, Warning, TEXT("%s"), Reason);
        GetMutableBases(Faction).Remove(NewBase);
        if (Site)
        {
            Site->bHasBeenUsed = false;
        }
        OnBaseListChanged.Broadcast(Faction);
        OnFacilityListChanged.Broadcast(Faction);
    };

    if (!CommandDef)
    {
        AbortExpansionBase(TEXT("[BASE] No Command Center definition — ABORTING new base"));
        return nullptr;
    }

    UStrategyFacility* CommandFacility = BuildFacility(Faction, CommandDef, NewBase);

    if (!CommandFacility)
    {
        const FString AbortMsg = FString::Printf(
            TEXT("[BASE] Command Center placement failed for '%s' — ABORTING new base (site reopened)"),
            *NewBase->BaseName.ToString());
        AbortExpansionBase(*AbortMsg);
        return nullptr;
    }

    const int32 BuildDays = FMath::Max(1, CommandDef->BuildTimeDays);
    CommandFacility->bIsOperational = false;
    CommandFacility->BuildProgressDays = BuildDays;
    NewBase->UpdatePowerFromFacilities();

    UE_LOG(LogTemp, Display, TEXT("[FACILITY] Command Center construction started in new base '%s' (%d days)"),
        *NewBase->BaseName.ToString(), BuildDays);

    UE_LOG(LogTemp, Display, TEXT("[AI] Expanded to new base '%s'"), *NewBase->BaseName.ToString());
    return NewBase;
}

/** Returns the faction's base list (delegates to GetBasesInternal). */
const TArray<UStrategyBase*>& UBaseManagerSubsystem::GetBases(EFactionType Faction) const
{
    return GetBasesInternal(Faction);
}

/** Returns the const internal base array for Human or Enemy. */
const TArray<UStrategyBase*>& UBaseManagerSubsystem::GetBasesInternal(EFactionType Faction) const
{
    return (Faction == EFactionType::Human) ? HumanBases : EnemyBases;
}

/** Returns the mutable internal base array for Human or Enemy. */
TArray<UStrategyBase*>& UBaseManagerSubsystem::GetMutableBases(EFactionType Faction)
{
    return (Faction == EFactionType::Human) ? HumanBases : EnemyBases;
}

/**
 * Validates prerequisites and resources, deducts cost, and queues facility construction
 * at the chosen or specified base.
 */
UStrategyFacility* UBaseManagerSubsystem::BuildFacility(EFactionType Faction, UFacilityDefinition* FacilityDef, UStrategyBase* TargetBase)
{
    if (!FacilityDef)
    {
        UE_LOG(LogTemp, Warning, TEXT("[BUILD] FacilityDef is null!"));
        return nullptr;
    }

    UE_LOG(LogTemp, Display, TEXT("[BUILD DEBUG] === START BuildFacility for %s (MaxBuilt=%d) ==="),
        *FacilityDef->FacilityName.ToString(), FacilityDef->MaxBuilt);

    UResourceManagerSubsystem* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>();

    // ===============================
    // STEP 1: Choose the target base 
    // ===============================
    TArray<UStrategyBase*>& Bases = GetMutableBases(Faction);

    if (Bases.IsEmpty())
    {
        // We should not reach here during normal expansion.
        // Log a warning instead of silently creating a base at the map center.
        UE_LOG(LogTemp, Warning, TEXT("[BaseManager] Tried to build facility with no bases. Expansion should go through StartBaseExpansion()."));
        return nullptr;
    }

    UStrategyBase* ChosenBase = TargetBase;

    if (!ChosenBase)
    {
        for (UStrategyBase* Base : Bases)
        {
            if (Base && !Base->HasOperationalCommandCenter())
            {
                ChosenBase = Base;
                break;
            }
        }
        if (!ChosenBase && !Bases.IsEmpty())
            ChosenBase = Bases[0];
    }

    if (!ChosenBase)
    {
        UE_LOG(LogTemp, Error, TEXT("[BUILD] No valid base found to build %s!"), *FacilityDef->FacilityName.ToString());
        return nullptr;
    }

    UE_LOG(LogTemp, Display, TEXT("[BUILD DEBUG] Chosen base '%s' for %s"),
        *ChosenBase->BaseName.ToString(), *FacilityDef->FacilityName.ToString());

    // ===================================================================
    // STEP 2: PREREQUISITE CHECK — BEFORE ANY RESOURCE DEDUCTION
    // ===================================================================
    if (FacilityDef->FacilityType != EFacilityType::Command)
    {
        if (!ChosenBase->HasOperationalCommandCenter())
        {
            UE_LOG(LogTemp, Warning, TEXT("[BUILD] Cannot build %s — Command Center must be operational first in base '%s'!"),
                *FacilityDef->FacilityName.ToString(), *ChosenBase->BaseName.ToString());
            return nullptr;   // ← EXIT HERE. No money is touched.
        }
    }

    UE_LOG(LogTemp, Display, TEXT("[BUILD DEBUG] Command Center check PASSED"));

    // ===================================
    // STEP 3: RESOURCE CHECK & DEDUCTION 
    // ===================================
    if (ResourceMgr)
    {
        if (!ResourceMgr->CanAfford(Faction, FacilityDef->BuildCost))
        {
            UE_LOG(LogTemp, Warning, TEXT("[BUILD] Cannot afford %s (%d Money, %d Metals, %d Biologicals, %d Chemicals needed)"),
                *FacilityDef->FacilityName.ToString(), FacilityDef->BuildCost.Money,
                FacilityDef->BuildCost.Metals, FacilityDef->BuildCost.Biologicals, FacilityDef->BuildCost.Chemicals);
            return nullptr;
        }

        // Resources OK
        UE_LOG(LogTemp, Display, TEXT("[BUILD DEBUG] Resources check PASSED for %s"), *FacilityDef->FacilityName.ToString());

        FResourceStockpile NegativeCost = FacilityDef->BuildCost;
        NegativeCost.Money = -NegativeCost.Money;
        NegativeCost.Metals = -NegativeCost.Metals;
        NegativeCost.Biologicals = -NegativeCost.Biologicals;
        NegativeCost.Chemicals = -NegativeCost.Chemicals;
        ResourceMgr->AddResources(Faction, NegativeCost);
    }

    // ============================
    // STEP 4: Create the facility
    // ============================
    UStrategyFacility* NewFacility = NewObject<UStrategyFacility>(this);
    NewFacility->FacilityDefinition = FacilityDef;
    NewFacility->BuildProgressDays = FacilityDef->BuildTimeDays;
    NewFacility->bIsOperational = false;
    NewFacility->CurrentPowerDraw = FacilityDef->PowerDraw;
    NewFacility->OwningBase = ChosenBase;

    ChosenBase->AddFacility(NewFacility);

    UE_LOG(LogTemp, Display, TEXT("[BUILD DEBUG] Facility object created and added to base"));

    if (FacilityDef->BuildTimeDays <= 0)
    {
        NewFacility->bIsOperational = true;
        NewFacility->BuildProgressDays = 0;
        UE_LOG(LogTemp, Display, TEXT("[BUILD DEBUG] Instant operational facility (0 build days)"));
    }
    else if (FacilityDef->FacilityType != EFacilityType::Command)
    {
        NewFacility->StartConstruction(FacilityDef);
        UE_LOG(LogTemp, Display, TEXT("[BUILD DEBUG] StartConstruction called (BuildTime = %d days)"), FacilityDef->BuildTimeDays);
    }
    else
    {
        UE_LOG(LogTemp, Display, TEXT("[BUILD DEBUG] Command Center using day countdown only (%d days remaining)"),
            NewFacility->BuildProgressDays);
    }

    OnFacilityListChanged.Broadcast(Faction);

    UE_LOG(LogTemp, Display, TEXT("✅ [BUILD] Started construction of %s in base '%s' (%d days)"),
        *FacilityDef->FacilityName.ToString(), *ChosenBase->BaseName.ToString(), NewFacility->BuildProgressDays);

    return NewFacility;
}

/** Sums PowerProvided from all operational facilities across faction bases. */
int32 UBaseManagerSubsystem::GetTotalPowerProvided(EFactionType Faction) const
{
    int32 Total = 0;
    for (UStrategyBase* Base : GetBasesInternal(Faction))
    {
        if (Base)
        {
            for (UStrategyFacility* Fac : Base->Facilities)
            {
                if (Fac && Fac->bIsOperational && Fac->FacilityDefinition)
                    Total += Fac->FacilityDefinition->PowerProvided;
            }
        }
    }
    return Total;
}

/** Sums CurrentPowerDraw from all operational facilities across faction bases. */
int32 UBaseManagerSubsystem::GetTotalPowerDrawn(EFactionType Faction) const
{
    int32 Total = 0;
    for (UStrategyBase* Base : GetBasesInternal(Faction))
    {
        if (Base)
        {
            for (UStrategyFacility* Fac : Base->Facilities)
            {
                if (Fac && Fac->bIsOperational)
                    Total += Fac->CurrentPowerDraw;
            }
        }
    }
    return Total;
}

/** Returns GetTotalPowerProvided minus GetTotalPowerDrawn for the faction. */
int32 UBaseManagerSubsystem::GetNetPower(EFactionType Faction) const
{
    return GetTotalPowerProvided(Faction) - GetTotalPowerDrawn(Faction);
}

/** Sums living-quarters capacity across all faction bases. */
int32 UBaseManagerSubsystem::GetTotalBarracksCapacity(EFactionType Faction) const
{
    int32 Total = 0;
    for (UStrategyBase* Base : GetBasesInternal(Faction))
    {
        if (Base)
            Total += Base->GetTotalCapacityForType(EFacilityType::LivingQuarters);
    }
    return Total;
}

/** True if any faction base contains a facility of the given type. */
bool UBaseManagerSubsystem::HasFacilityOfType(EFactionType Faction, EFacilityType FacilityType) const
{
    for (UStrategyBase* Base : GetBasesInternal(Faction))
    {
        if (Base && Base->HasFacilityOfType(FacilityType))
            return true;
    }
    return false;
}

/** Counts facilities of the given type across all faction bases. */
int32 UBaseManagerSubsystem::GetCurrentCountOfType(EFactionType Faction, EFacilityType FacilityType) const
{
    int32 Count = 0;
    for (UStrategyBase* Base : GetBasesInternal(Faction))
    {
        if (Base)
            Count += Base->GetCountOfType(FacilityType);
    }
    return Count;
}

/** Decrements build timers and marks facilities operational when complete. */
void UBaseManagerSubsystem::AdvanceFacilityConstruction(EFactionType Faction)
{
    for (UStrategyBase* Base : GetBasesInternal(Faction))
    {
        if (!Base) continue;

        bool bAnyCompletedThisTick = false;

        for (UStrategyFacility* Fac : Base->Facilities)
        {
            if (!Fac || Fac->bIsOperational || !Fac->FacilityDefinition) continue;

            if (Fac->FacilityDefinition->FacilityType != EFacilityType::Command
                && Fac->ActiveProductionJobs.Num() > 0)
            {
                continue;
            }

            if (Fac->FacilityDefinition->FacilityType == EFacilityType::Command
                && Base->BuiltOnSite && !Fac->bIsOperational)
            {
                UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>();
                if (!MissionMgr || !MissionMgr->IsExpansionBaseGuarded(Base))
                {
                    CancelExpansionConstruction(Base, Base->BuiltOnSite);
                    continue;
                }
            }

            Fac->BuildProgressDays--;

            if (Fac->FacilityDefinition->FacilityType == EFacilityType::Command && Fac->BuildProgressDays > 0)
            {
                UE_LOG(LogTemp, Display, TEXT("[CC BUILD] %s '%s' — %d day(s) remaining"),
                    *UEnum::GetValueAsString(Faction), *Base->BaseName.ToString(), Fac->BuildProgressDays);
            }

            if (Fac->BuildProgressDays <= 0)
            {
                bAnyCompletedThisTick = true;

                Fac->bIsOperational = true;
                Fac->BuildProgressDays = 0;

                int32 PowerImpact = Fac->FacilityDefinition->PowerProvided - Fac->FacilityDefinition->PowerDraw;

                UE_LOG(LogTemp, Display, TEXT("[FACILITY] %s completed in base '%s' for %s (Power impact: %+d) — NOW OPERATIONAL"),
                    *Fac->FacilityDefinition->FacilityName.ToString(),
                    *Base->BaseName.ToString(),
                    *UEnum::GetValueAsString(Faction),
                    PowerImpact);

                if (UStrategyEventDispatcher* EventDisp = GetGameInstance()->GetSubsystem<UStrategyEventDispatcher>())
                    EventDisp->OnFacilityCompleted.Broadcast(Faction, Fac);
            }
        }

        if (bAnyCompletedThisTick)
        {
            Base->UpdatePowerFromFacilities();
        }
    }
}

/** Flattens all facilities from every base into a single transient array. */
const TArray<UStrategyFacility*>& UBaseManagerSubsystem::GetFacilities(EFactionType Faction) const
{
    static TArray<UStrategyFacility*> FlatList;
    FlatList.Empty();
    for (UStrategyBase* Base : GetBasesInternal(Faction))
    {
        if (Base)
            FlatList.Append(Base->Facilities);
    }
    return FlatList;
}

/** Daily tick: construction, repairs, extraction, and salvage expiry for both factions. */
void UBaseManagerSubsystem::OnDayPassed(int32 NewDay)
{
    AdvanceFacilityConstruction(EFactionType::Human);
    AdvanceFacilityConstruction(EFactionType::Enemy);
    AdvanceAllConstruction();           // NEW: Processes the construction queue every day
    SimulateDailyRepairs(EFactionType::Human);
    SimulateDailyRepairs(EFactionType::Enemy);
    // === NEW: Daily resource extraction from sites ===
    ProcessDailyResourceExtraction(EFactionType::Human);
    ProcessDailyResourceExtraction(EFactionType::Enemy);
    ProcessSalvageSiteExpiry(NewDay);
}

/** Returns days until salvage expiry, or 0 if the site is not an active wreck. */
int32 UBaseManagerSubsystem::GetSalvageDaysRemaining(const UStrategySiteDefinition* Site) const
{
    if (!Site || Site->SiteType != EStrategySiteType::SalvageSite || Site->SalvageState != ESalvageSiteState::Active)
    {
        return 0;
    }

    int32 CurrentDay = Site->CreatedOnSimulationDay;
    if (UTimeManagerSubsystem* TimeMgr = GetGameInstance()->GetSubsystem<UTimeManagerSubsystem>())
    {
        CurrentDay = TimeMgr->GetTotalSimulationDays();
    }

    return FMath::Max(0, Site->SalvageExpiresOnDay - CurrentDay);
}

/** Removes a salvage site, resolves MIAs, and broadcasts OnSalvageSiteRemoved. */
void UBaseManagerSubsystem::RemoveSalvageSite(UStrategySiteDefinition* Site, bool bExpired,
    EFactionType LastSalvagingFaction)
{
    if (!Site || Site->SiteType != EStrategySiteType::SalvageSite)
    {
        return;
    }

    const FGuid RemovedSiteId = Site->SiteId;

    if (USoldierManagerSubsystem* SoldierMgr = GetGameInstance()->GetSubsystem<USoldierManagerSubsystem>())
    {
        for (UStrategySoldier* MIA : Site->MIASoldiers)
        {
            if (MIA && MIA->bIsMIA)
            {
                MIA->bIsMIA = false;
                MIA->WreckSiteId = FGuid();
                SoldierMgr->MarkAsKIA(Site->WreckOwnerFaction, MIA);
                UE_LOG(LogTemp, Display, TEXT("[SALVAGE] MIA '%s' presumed lost — wreck %s"),
                    *MIA->SoldierName, bExpired ? TEXT("expired") : TEXT("removed"));
            }
        }
    }

    Site->MIASoldiers.Empty();
    Site->SalvageState = ESalvageSiteState::Removed;
    Site->bHasBeenUsed = true;
    DiscoveredSitesHuman.Remove(Site);
    DiscoveredSitesEnemy.Remove(Site);
    AllPotentialSites.Remove(Site);

    UE_LOG(LogTemp, Display, TEXT("[SALVAGE] Wreck '%s' removed from map (%s)"),
        *Site->SiteName, bExpired ? TEXT("expired") : TEXT("depleted"));

    if (UStrategyEventDispatcher* EventDisp = GetGameInstance()->GetSubsystem<UStrategyEventDispatcher>())
    {
        EventDisp->OnSalvageSiteRemoved.Broadcast(RemovedSiteId, LastSalvagingFaction);
    }
}

/** Collects and removes salvage sites that have passed their expiry day. */
void UBaseManagerSubsystem::ProcessSalvageSiteExpiry(int32 CurrentSimulationDay)
{
    TArray<UStrategySiteDefinition*> ExpiredSites;

    for (UStrategySiteDefinition* Site : AllPotentialSites)
    {
        if (!Site || Site->SiteType != EStrategySiteType::SalvageSite)
        {
            continue;
        }

        if (Site->SalvageState != ESalvageSiteState::Active)
        {
            continue;
        }

        if (CurrentSimulationDay >= Site->SalvageExpiresOnDay)
        {
            ExpiredSites.Add(Site);
        }
    }

    for (UStrategySiteDefinition* Site : ExpiredSites)
    {
        RemoveSalvageSite(Site, true);
    }
}

/** Serializes every site in AllPotentialSites including discovery flags. */
TArray<FStrategySiteSaveData> UBaseManagerSubsystem::SerializeAllSites() const
{
    TArray<FStrategySiteSaveData> Result;
    Result.Reserve(AllPotentialSites.Num());

    for (const UStrategySiteDefinition* Site : AllPotentialSites)
    {
        if (!Site)
        {
            continue;
        }

        FStrategySiteSaveData Data;
        Data.SiteId = Site->SiteId;
        Data.Location = Site->Location;
        Data.SiteType = Site->SiteType;
        Data.WreckOwnerFaction = Site->WreckOwnerFaction;
        Data.SiteName = Site->SiteName;
        Data.MaxResources = Site->MaxResources;
        Data.CurrentResources = Site->CurrentResources;
        Data.bHasBeenUsed = Site->bHasBeenUsed;
        Data.SalvageState = Site->SalvageState;
        Data.CreatedOnSimulationDay = Site->CreatedOnSimulationDay;
        Data.SalvageExpiresOnDay = Site->SalvageExpiresOnDay;
        Data.KIACrashCount = Site->KIACrashCount;
        Data.KnownFactions = Site->KnownFactions;
        Data.bDiscoveredByHuman = DiscoveredSitesHuman.Contains(Site);
        Data.bDiscoveredByEnemy = DiscoveredSitesEnemy.Contains(Site);

        const FSoftObjectPath VehiclePath = Site->SourceVehicleDefinition.ToSoftObjectPath();
        if (VehiclePath.IsValid())
        {
            Data.SourceVehicleDefinitionPath = VehiclePath;
        }

        Result.Add(Data);
    }

    return Result;
}

/** Rebuilds site objects and faction discovery lists from save data. */
void UBaseManagerSubsystem::DeserializeAllSites(const TArray<FStrategySiteSaveData>& SavedSites)
{
    AllPotentialSites.Empty();
    DiscoveredSitesHuman.Empty();
    DiscoveredSitesEnemy.Empty();

    for (const FStrategySiteSaveData& Data : SavedSites)
    {
        if (Data.SalvageState == ESalvageSiteState::Removed)
        {
            continue;
        }

        UStrategySiteDefinition* Site = NewObject<UStrategySiteDefinition>(this);
        Site->SiteId = Data.SiteId;
        Site->Location = Data.Location;
        Site->SiteType = Data.SiteType;
        Site->WreckOwnerFaction = Data.WreckOwnerFaction;
        Site->SiteName = Data.SiteName;
        Site->MaxResources = Data.MaxResources;
        Site->CurrentResources = Data.CurrentResources;
        Site->bHasBeenUsed = Data.bHasBeenUsed;
        Site->SalvageState = Data.SalvageState;
        Site->CreatedOnSimulationDay = Data.CreatedOnSimulationDay;
        Site->SalvageExpiresOnDay = Data.SalvageExpiresOnDay;
        Site->KIACrashCount = Data.KIACrashCount;
        Site->KnownFactions = Data.KnownFactions;
        Site->MIASoldiers.Empty();

        if (Data.SourceVehicleDefinitionPath.IsValid())
        {
            Site->SourceVehicleDefinition = TSoftObjectPtr<UVehicleDefinition>(Data.SourceVehicleDefinitionPath);
        }

        AllPotentialSites.Add(Site);

        if (Data.bDiscoveredByHuman)
        {
            DiscoveredSitesHuman.Add(Site);
        }
        if (Data.bDiscoveredByEnemy)
        {
            DiscoveredSitesEnemy.Add(Site);
        }
    }

    UE_LOG(LogTemp, Display, TEXT("[SAVE] Deserialized %d site(s) (%d human discoveries, %d enemy discoveries)"),
        AllPotentialSites.Num(), DiscoveredSitesHuman.Num(), DiscoveredSitesEnemy.Num());
}

/** Checks faction base cap and hangar-gated expansion limit. */
bool UBaseManagerSubsystem::CanBuildNewBase(EFactionType Faction) const
{
    const TArray<UStrategyBase*>& Bases = GetBasesInternal(Faction);

    int32 MaxFactionBases = 10;
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UStrategyCampaignSubsystem* Campaign = GI->GetSubsystem<UStrategyCampaignSubsystem>())
        {
            MaxFactionBases = Campaign->MaxAIBases;
        }
    }

    if (Bases.Num() >= MaxFactionBases)
    {
        return false;
    }

    int32 OperationalHangers = GetNumberOfOperationalHangers(Faction);
    int32 MaxAllowedBases = 1 + OperationalHangers;

    return Bases.Num() < MaxAllowedBases;
}

/** Counts bases that have at least one operational hangar. */
int32 UBaseManagerSubsystem::GetNumberOfOperationalHangers(EFactionType Faction) const
{
    int32 Count = 0;
    for (UStrategyBase* Base : GetBasesInternal(Faction))
    {
        if (Base && Base->HasOperationalFacilityOfType(EFacilityType::Hanger))
            Count++;
    }
    return Count;
}

/** Sums hangar Capacity across all operational hangars for the faction. */
int32 UBaseManagerSubsystem::GetTotalAvailableHangerSlots(EFactionType Faction) const
{
    int32 TotalSlots = 0;
    for (UStrategyBase* Base : GetBasesInternal(Faction))
    {
        if (Base)
        {
            TotalSlots += Base->GetTotalCapacityForType(EFacilityType::Hanger);
        }
    }
    return TotalSlots;
}

/** Destroys all base objects and clears HumanBases and EnemyBases. */
void UBaseManagerSubsystem::ResetAllBases()
{
    for (UStrategyBase* Base : HumanBases)
    {
        if (Base) Base->ConditionalBeginDestroy();
    }
    HumanBases.Empty();

    for (UStrategyBase* Base : EnemyBases)
    {
        if (Base) Base->ConditionalBeginDestroy();
    }
    EnemyBases.Empty();

    OnBaseListChanged.Broadcast(EFactionType::Human);
    OnBaseListChanged.Broadcast(EFactionType::Enemy);

    UE_LOG(LogTemp, Display, TEXT("[RESET] All bases cleared for both factions"));
}

/** Calls SimulateDaily on every facility in every base of the faction. */
void UBaseManagerSubsystem::SimulateDailyRepairs(EFactionType Faction)
{
    UE_LOG(LogTemp, Verbose, TEXT("[DAILY SIM] %s — Medical Bays can heal 0 soldiers | Vehicle Repair Shops can repair 0 vehicles (+25 HP)"),
        *UEnum::GetValueAsString(Faction));

    for (UStrategyBase* Base : GetBasesInternal(Faction))
    {
        if (!Base) continue;

        for (UStrategyFacility* Fac : Base->Facilities)
        {
            if (Fac)
            {
                Fac->SimulateDaily();   // ALWAYS call - this guarantees production jobs advance and slots empty
            }
        }
    }
}

/** Advances production/construction for all facilities in Human and Enemy bases. */
void UBaseManagerSubsystem::AdvanceAllConstruction()
{
    UE_LOG(LogTemp, Verbose, TEXT("[BASE] AdvanceAllConstruction — Processing BOTH factions"));

    // Enemy
    UE_LOG(LogTemp, Display, TEXT("[BASE] Advancing Human bases (%d bases)"), HumanBases.Num());
    for (UStrategyBase* Base : HumanBases)
    {
        for (UStrategyFacility* Fac : Base->Facilities)
        {
            if (Fac)
                Fac->AdvanceConstructionDay();
        }
    }

    // Enemy
    UE_LOG(LogTemp, Display, TEXT("[BASE] Advancing Enemy bases (%d bases)"), EnemyBases.Num());
    for (UStrategyBase* Base : EnemyBases)
    {
        for (UStrategyFacility* Fac : Base->Facilities)
        {
            if (Fac)
                Fac->AdvanceConstructionDay();
        }
    }    
}

/** Logs per-base and global personnel/vehicle/facility totals to the output log. */
void UBaseManagerSubsystem::DebugPrintFullBaseState(EFactionType Faction) const
{
    USoldierManagerSubsystem* SoldierMgr = GetGameInstance()->GetSubsystem<USoldierManagerSubsystem>();
    UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>();
    if (!SoldierMgr)
    {
        UE_LOG(LogTemp, Warning, TEXT("[BASE STATE] Could not get SoldierMgr"));
        return;
    }

    UE_LOG(LogTemp, Display, TEXT("=== BASE STATE FOR %s (%d bases) ==="),
        *UEnum::GetValueAsString(Faction), GetBases(Faction).Num());

    const TArray<UStrategyBase*>& Bases = GetBases(Faction);

    for (UStrategyBase* B : Bases)
    {
        if (!B) continue;

        // Soldiers stationed at this base
        int32 SoldiersStationed = 0;
        for (UStrategySoldier* Soldier : SoldierMgr->GetRoster(Faction))
        {
            if (Soldier && Soldier->StationedBase == B)
                SoldiersStationed++;
        }

        // Soldiers on mission FROM this base
        int32 SoldiersOnMission = 0;
        if (MissionMgr)
        {
            for (UMissionGroup* Mission : MissionMgr->ActiveMissions)
            {
                if (Mission && Mission->OriginBase == B)
                {
                    for (UStrategyVehicle* Vehicle : Mission->VehiclesInFleet)
                    {
                        if (Vehicle)
                            SoldiersOnMission += Vehicle->CurrentPassengers.Num();
                    }
                }
            }
        }

        // Vehicles parked in hangars at this base
        int32 VehiclesStationed = 0;
        for (UStrategyFacility* Fac : B->Facilities)
        {
            if (Fac && Fac->bIsOperational && Fac->FacilityDefinition &&
                Fac->FacilityDefinition->FacilityType == EFacilityType::Hanger)
                VehiclesStationed += Fac->ParkedVehicles.Num();
        }

        // Vehicles on mission FROM this base
        int32 VehiclesOnMission = 0;
        if (MissionMgr)
        {
            for (UMissionGroup* Mission : MissionMgr->ActiveMissions)
            {
                if (Mission && Mission->OriginBase == B)
                    VehiclesOnMission += Mission->VehiclesInFleet.Num();
            }
        }

        UE_LOG(LogTemp, Display, TEXT("Base: %s | Net Power: %d"),
            *B->BaseName.ToString(), B->GetNetPower());

        FString FacilityList;
        for (EFacilityType Type : {
            EFacilityType::Command, EFacilityType::LivingQuarters, EFacilityType::Laboratory,
                EFacilityType::Workshop, EFacilityType::Hanger, EFacilityType::Medical,
                EFacilityType::VehicleRepair, EFacilityType::Containment, EFacilityType::Autopsy
        })
        {
            int32 Count = B->GetTotalBuiltOfType(Type);
            if (Count > 0)
            {
                FacilityList.Append(FString::Printf(TEXT("%d %s, "), Count, *UEnum::GetValueAsString(Type)));
            }
        }
        if (!FacilityList.IsEmpty())
        {
            FacilityList = FacilityList.LeftChop(2);
        }

        UE_LOG(LogTemp, Display, TEXT("  Facilities: %s"), *FacilityList);
        UE_LOG(LogTemp, Display, TEXT("  Soldiers stationed: %d | Vehicles stationed: %d | POW Count: %d | KIA Bodies: %d"),
            SoldiersStationed, VehiclesStationed, B->GetPOWCount(), B->GetKIABodyCount());

        UE_LOG(LogTemp, Display, TEXT("  Soldiers on mission: %d | Vehicles on mission: %d"),
            SoldiersOnMission, VehiclesOnMission);
    }

    // === GLOBAL TOTALS ===
    int32 TotalBarracksCapacity = 0;
    int32 TotalSoldiers = SoldierMgr->GetRoster(Faction).Num();
    int32 TotalPOW = 0;
    int32 TotalKIA = 0;

    for (UStrategyBase* B : Bases)
    {
        if (B)
        {
            TotalBarracksCapacity += B->GetTotalBuiltOfType(EFacilityType::LivingQuarters) * 6;
            TotalPOW += B->GetPOWCount();
            TotalKIA += B->GetKIABodyCount();
        }
    }

    UE_LOG(LogTemp, Display, TEXT("=== GLOBAL TOTALS ==="));
    UE_LOG(LogTemp, Display, TEXT("Total Barracks Capacity: %d | Current Soldiers: %d | Total POW Count: %d | Total KIA Bodies: %d"),
        TotalBarracksCapacity, TotalSoldiers, TotalPOW, TotalKIA);
}

/** Builds a text report of base state suitable for UI display or export. */
FString UBaseManagerSubsystem::GetBaseStateDebugString(EFactionType Faction) const
{
    FString Output = FString::Printf(TEXT("=== BASE STATE FOR %s (%d bases) ===\n"),
        *UEnum::GetValueAsString(Faction), GetBases(Faction).Num());

    USoldierManagerSubsystem* SoldierMgr = GetGameInstance()->GetSubsystem<USoldierManagerSubsystem>();
    UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>();

    const TArray<UStrategyBase*>& Bases = GetBases(Faction);

    for (UStrategyBase* B : Bases)
    {
        if (!B) continue;

        // Soldiers stationed
        int32 SoldiersStationed = 0;
        for (UStrategySoldier* S : SoldierMgr->GetRoster(Faction))
            if (S && S->StationedBase == B) SoldiersStationed++;

        // Vehicles stationed
        int32 VehiclesStationed = 0;
        for (UStrategyFacility* Fac : B->Facilities)
            if (Fac && Fac->bIsOperational && Fac->FacilityDefinition &&
                Fac->FacilityDefinition->FacilityType == EFacilityType::Hanger)
                VehiclesStationed += Fac->ParkedVehicles.Num();

        // Vehicles on mission from this base
        int32 VehiclesOnMission = 0;
        if (MissionMgr)
            for (UMissionGroup* M : MissionMgr->ActiveMissions)
                if (M && M->OriginBase == B)
                    VehiclesOnMission += M->VehiclesInFleet.Num();

        // Soldiers on mission from this base
        int32 SoldiersOnMission = 0;
        if (MissionMgr)
        {
            for (UMissionGroup* M : MissionMgr->ActiveMissions)
            {
                if (M && M->OriginBase == B)
                {
                    for (UStrategyVehicle* V : M->VehiclesInFleet)
                        SoldiersOnMission += V->CurrentPassengers.Num();
                }
            }
        }

        FString FacilityList;
        for (EFacilityType Type : { EFacilityType::Command, EFacilityType::LivingQuarters, EFacilityType::Laboratory,
            EFacilityType::Workshop, EFacilityType::Hanger, EFacilityType::Medical,
            EFacilityType::VehicleRepair, EFacilityType::Containment, EFacilityType::Autopsy })
        {
            int32 Count = B->GetTotalBuiltOfType(Type);
            if (Count > 0)
                FacilityList.Append(FString::Printf(TEXT("%d %s, "), Count, *UEnum::GetValueAsString(Type).Mid(1))); // remove E
        }
        if (!FacilityList.IsEmpty())
            FacilityList = FacilityList.LeftChop(2);

        Output += FString::Printf(TEXT("Base: %s | Net Power: %d\n"), *B->BaseName.ToString(), B->GetNetPower());
        Output += FString::Printf(TEXT("  Facilities: %s\n"), *FacilityList);
        Output += FString::Printf(TEXT("  Soldiers stationed: %d | Vehicles stationed: %d | POW Count: %d | KIA Bodies: %d\n"),
            SoldiersStationed, VehiclesStationed, B->GetPOWCount(), B->GetKIABodyCount());
        Output += FString::Printf(TEXT("  Soldiers on mission: %d | Vehicles on mission: %d\n\n"),
            SoldiersOnMission, VehiclesOnMission);
    }

    // Global totals
    int32 TotalBarracks = 0;
    int32 TotalSoldiers = SoldierMgr ? SoldierMgr->GetRoster(Faction).Num() : 0;
    int32 TotalPOW = 0;
    int32 TotalKIA = 0;

    for (UStrategyBase* B : Bases)
    {
        if (B)
        {
            TotalBarracks += B->GetTotalBuiltOfType(EFacilityType::LivingQuarters) * 6;
            TotalPOW += B->GetPOWCount();
            TotalKIA += B->GetKIABodyCount();
        }
    }

    Output += FString::Printf(TEXT("=== GLOBAL TOTALS ===\n"));
    Output += FString::Printf(TEXT("Total Barracks Capacity: %d | Current Soldiers: %d | Total POW Count: %d | Total KIA Bodies: %d\n"),
        TotalBarracks, TotalSoldiers, TotalPOW, TotalKIA);

    return Output;
}

/** Registers a site for a faction, fires discovery events, and updates intel. */
UStrategySiteDefinition* UBaseManagerSubsystem::AddDiscoveredSite(EFactionType Faction, UStrategySiteDefinition* Site,
    EDiscoveryReason Reason)
{
    if (!Site)
    {
        return nullptr;
    }

    bool bNewDiscovery = false;
    if (Faction == EFactionType::Human)
    {
        const int32 Before = DiscoveredSitesHuman.Num();
        DiscoveredSitesHuman.AddUnique(Site);
        bNewDiscovery = DiscoveredSitesHuman.Num() > Before;
    }
    else if (Faction == EFactionType::Enemy)
    {
        const int32 Before = DiscoveredSitesEnemy.Num();
        DiscoveredSitesEnemy.AddUnique(Site);
        bNewDiscovery = DiscoveredSitesEnemy.Num() > Before;
    }

    if (bNewDiscovery)
    {
        UE_LOG(LogTemp, Display, TEXT("[DISCOVERY] %s discovered %s at (%.0f, %.0f) via %s"),
            *UEnum::GetValueAsString(Faction),
            *StaticEnum<EStrategySiteType>()->GetNameStringByValue(static_cast<int64>(Site->SiteType)),
            Site->Location.X, Site->Location.Y,
            *StaticEnum<EDiscoveryReason>()->GetNameStringByValue(static_cast<int64>(Reason)));

        if (UStrategyEventDispatcher* EventDisp = GetGameInstance()->GetSubsystem<UStrategyEventDispatcher>())
        {
            EventDisp->OnSiteDiscovered.Broadcast(Faction, Site, Reason);
        }
    }

    if (UFactionIntelSubsystem* IntelMgr = GetGameInstance()->GetSubsystem<UFactionIntelSubsystem>())
    {
        float ObservedHours = 0.0f;
        if (UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
        {
            ObservedHours = MissionMgr->GetCurrentGameHours();
        }
        IntelMgr->ObserveSite(Faction, Site, Reason, ObservedHours);
    }

    return Site;
}

/** Adds combat-known factions to discovery lists for a salvage wreck. */
void UBaseManagerSubsystem::RegisterCombatKnownSalvage(UStrategySiteDefinition* Site)
{
    if (!Site || Site->SiteType != EStrategySiteType::SalvageSite)
    {
        return;
    }

    for (const EFactionType Faction : Site->KnownFactions)
    {
        if (Faction == EFactionType::Human || Faction == EFactionType::Enemy)
        {
            AddDiscoveredSite(Faction, Site, EDiscoveryReason::Combat);
        }
    }
}

/**
 * Spawns a timed salvage wreck site from a destroyed vehicle's position and loot.
 * Sets WreckOwnerFaction, KnownFactions (owner + combat opponent), expiry from campaign settings,
 * adds to AllPotentialSites, and calls RegisterCombatKnownSalvage.
 */
UStrategySiteDefinition* UBaseManagerSubsystem::CreateSalvageSite(FVector2D Location, UStrategyVehicle* DestroyedVehicle)
{
    UStrategySiteDefinition* Site = NewObject<UStrategySiteDefinition>(this);
    Site->SiteId = FGuid::NewGuid();
    Site->Location = Location;
    Site->SiteType = EStrategySiteType::SalvageSite;
    Site->SalvageState = ESalvageSiteState::Active;
    Site->bHasBeenUsed = false;

    if (DestroyedVehicle && DestroyedVehicle->HomeBase)
    {
        Site->WreckOwnerFaction = DestroyedVehicle->HomeBase->OwningFaction;
        Site->KnownFactions.AddUnique(Site->WreckOwnerFaction);
    }

    if (DestroyedVehicle && DestroyedVehicle->CurrentTargetVehicle.IsValid())
    {
        if (UStrategyVehicle* Opponent = DestroyedVehicle->CurrentTargetVehicle.Get())
        {
            if (Opponent->HomeBase)
            {
                Site->KnownFactions.AddUnique(Opponent->HomeBase->OwningFaction);
            }
        }
    }

    if (DestroyedVehicle && DestroyedVehicle->VehicleDefinition)
    {
        Site->SourceVehicleDefinition = DestroyedVehicle->VehicleDefinition;

        const FText& VehicleName = DestroyedVehicle->VehicleDefinition->VehicleName;
        Site->SiteName = FString::Printf(TEXT("Wreck: %s"), *VehicleName.ToString());

        const FResourceStockpile& BuildCost = DestroyedVehicle->VehicleDefinition->BuildCost;
        Site->MaxResources.Metals = FMath::Max(100, BuildCost.Metals / 2);
        Site->MaxResources.Chemicals = FMath::Max(50, BuildCost.Chemicals / 2);
        Site->MaxResources.Money = FMath::Max(200, BuildCost.Money / 4);
        Site->MaxResources.ExoticMaterial = BuildCost.ExoticMaterial / 4;
        Site->CurrentResources = Site->MaxResources;
    }
    else
    {
        Site->SiteName = TEXT("Vehicle Wreck");
        Site->MaxResources.Metals = 400;
        Site->MaxResources.Chemicals = 150;
        Site->MaxResources.Money = 500;
        Site->CurrentResources = Site->MaxResources;
    }

    int32 CreatedDay = 0;
    if (UTimeManagerSubsystem* TimeMgr = GetGameInstance()->GetSubsystem<UTimeManagerSubsystem>())
    {
        CreatedDay = TimeMgr->GetTotalSimulationDays();
        Site->CreatedOnSimulationDay = CreatedDay;
    }

    int32 ExpiryDays = 7;
    if (UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>())
    {
        ExpiryDays = FMath::Max(1, Campaign->SalvageWreckExpiryDays);
    }
    Site->SalvageExpiresOnDay = CreatedDay + ExpiryDays;

    for (UStrategySiteDefinition* Existing : AllPotentialSites)
    {
        if (!Existing)
        {
            continue;
        }

        if (FVector2D::Distance(Existing->Location, Location) <= SiteMatchTolerance)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[SALVAGE] WARNING: Wreck within %.0f px of site '%s' (type %s)"),
                SiteMatchTolerance,
                *Existing->SiteName,
                *StaticEnum<EStrategySiteType>()->GetNameStringByValue(static_cast<int64>(Existing->SiteType)));
            break;
        }
    }

    AllPotentialSites.Add(Site);
    RegisterCombatKnownSalvage(Site);

    UE_LOG(LogTemp, Display, TEXT("[SALVAGE] Wreck site created at (%.0f, %.0f) — owner %s — known to %d faction(s)"),
        Location.X, Location.Y,
        *UEnum::GetValueAsString(Site->WreckOwnerFaction),
        Site->KnownFactions.Num());

    if (UStrategyEventDispatcher* EventDisp = GetGameInstance()->GetSubsystem<UStrategyEventDispatcher>())
    {
        EventDisp->OnSalvageSiteCreated.Broadcast(Site->WreckOwnerFaction, Site->KnownFactions, Site);
    }

    return Site;
}

/** True when SiteType is SalvageSite. */
bool UBaseManagerSubsystem::IsSalvageSite(const UStrategySiteDefinition* Site) const
{
    return Site != nullptr && Site->SiteType == EStrategySiteType::SalvageSite;
}

/** True when the site is in KnownFactions or the faction discovery list. */
bool UBaseManagerSubsystem::IsSiteKnownToFaction(EFactionType Faction, const UStrategySiteDefinition* Site) const
{
    if (!Site)
    {
        return false;
    }

    if (Site->KnownFactions.Contains(Faction))
    {
        return true;
    }

    const TArray<UStrategySiteDefinition*>& Discovered =
        (Faction == EFactionType::Human) ? DiscoveredSitesHuman : DiscoveredSitesEnemy;
    return Discovered.Contains(Site);
}

/** Returns the closest site within Tolerance of Location, or nullptr. */
UStrategySiteDefinition* UBaseManagerSubsystem::FindSiteAtLocation(FVector2D Location, float Tolerance) const
{
    UStrategySiteDefinition* BestMatch = nullptr;
    float BestDist = Tolerance;

    for (UStrategySiteDefinition* Site : AllPotentialSites)
    {
        if (!Site)
        {
            continue;
        }

        const float Dist = FVector2D::Distance(Site->Location, Location);
        if (Dist <= BestDist)
        {
            BestDist = Dist;
            BestMatch = Site;
        }
    }

    return BestMatch;
}

/** Validates campaign flags, intel, resources, mission conflicts, and vehicle range. */
bool UBaseManagerSubsystem::CanSalvageSite(EFactionType Faction, const UStrategySiteDefinition* Site,
    const UStrategyVehicle* SalvageVehicle) const
{
    if (UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>())
    {
        if (!Campaign->bSalvageSitesEnabled || !Campaign->bSalvageMissionsEnabled)
        {
            return false;
        }
    }

    if (!IsSalvageSite(Site))
    {
        return false;
    }

    if (Site->SalvageState != ESalvageSiteState::Active)
    {
        return false;
    }

    if (Site->bHasBeenUsed || Site->CurrentResources.IsEmpty())
    {
        return false;
    }

    if (!IsSiteKnownToFaction(Faction, Site))
    {
        return false;
    }

    if (UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
    {
        if (MissionMgr->IsSiteTargetedByActiveMissions(Site))
        {
            return false;
        }
    }

    if (SalvageVehicle)
    {
        if (!SalvageVehicle->VehicleDefinition
            || !UStrategicSimulationDisplayHelpers::IsSalvageCapableVehicleType(SalvageVehicle->VehicleDefinition->VehicleType))
        {
            return false;
        }

        if (SalvageVehicle->HomeBase)
        {
            const float RoundTrip = FVector2D::Distance(SalvageVehicle->HomeBase->MapLocation, Site->Location) * 2.0f;
            if (!SalvageVehicle->HasEnoughRangeForMission(RoundTrip))
            {
                return false;
            }
        }
    }

    return true;
}

/** Procedurally places potential base/resource sites with minimum spacing on the map. */
void UBaseManagerSubsystem::GenerateInitialSites(int32 NumSites, float MinDistanceBetweenSites,
    float LogicalMapWidth, float LogicalMapHeight, float BorderPadding)
{
    AllPotentialSites.Empty();
    if (!GetWorld() || NumSites <= 0) return;

    const int32 MaxAttemptsPerSite = 50;
    int32 SitesPlaced = 0;

    float MinX = BorderPadding;
    float MaxX = LogicalMapWidth - BorderPadding;
    float MinY = BorderPadding;
    float MaxY = LogicalMapHeight - BorderPadding;

    for (int32 i = 0; i < NumSites; ++i)
    {
        bool bPlaced = false;
        for (int32 Attempt = 0; Attempt < MaxAttemptsPerSite; ++Attempt)
        {
            FVector2D NewLoc(FMath::RandRange(MinX, MaxX), FMath::RandRange(MinY, MaxY));

            bool bTooClose = false;
            for (UStrategySiteDefinition* Existing : AllPotentialSites)
            {
                if (FVector2D::Distance(Existing->Location, NewLoc) < MinDistanceBetweenSites)
                {
                    bTooClose = true;
                    break;
                }
            }
            if (bTooClose) continue;

            UStrategySiteDefinition* NewSite = NewObject<UStrategySiteDefinition>(this);
            NewSite->SiteId = FGuid::NewGuid();
            NewSite->Location = NewLoc;
            NewSite->SiteName = FString::Printf(TEXT("Potential Base %d"), SitesPlaced + 1);
            SiteGenerationHelpers::PopulateMineableResources(NewSite);
            if (NewSite->SiteType == EStrategySiteType::ResourceNode)
            {
                NewSite->SiteName = FString::Printf(TEXT("Resource Node %d"), SitesPlaced + 1);
            }

            AllPotentialSites.Add(NewSite);

            // White debug sphere (still useful in world space)
            DrawDebugSphere(GetWorld(), FVector(NewLoc.X, NewLoc.Y, 100.0f), 55.0f, 12, FColor::White, true, -1.0f, 0, 3.0f);

            SitesPlaced++;
            bPlaced = true;
            break;
        }

        if (!bPlaced)
        {
            UE_LOG(LogTemp, Warning, TEXT("[MAP] Could not place site %d after %d attempts"), i + 1, MaxAttemptsPerSite);
        }
    }

    UE_LOG(LogTemp, Display, TEXT("[MAP] Generated %d / %d sites inside %.0f×%.0f map with %.0f px border"),
        AllPotentialSites.Num(), NumSites, LogicalMapWidth, LogicalMapHeight, BorderPadding);
}

/** True when the site is a discovered, unused PotentialBase and faction can expand. */
bool UBaseManagerSubsystem::CanBuildBaseOnSite(EFactionType Faction, UStrategySiteDefinition* Site) const
{
    if (!Site) return false;

    if (Site->SiteType == EStrategySiteType::SalvageSite)
    {
        ensureMsgf(false, TEXT("CanBuildBaseOnSite must not be called on SalvageSite '%s'"), *Site->SiteName);
        return false;
    }

    if (Site->SiteType != EStrategySiteType::PotentialBase)
    {
        return false;
    }

    // Site must be discovered by this faction
    const TArray<UStrategySiteDefinition*>& Discovered = (Faction == EFactionType::Human)
        ? DiscoveredSitesHuman
        : DiscoveredSitesEnemy;

    if (!Discovered.Contains(Site)) return false;

    // Site must not already be used
    if (Site->bHasBeenUsed) return false;

    // Check base limit
    if (!CanBuildNewBase(Faction)) return false;

    if (UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>())
    {
        if (UFacilityDatabase* FacilityDB = Campaign->FacilityDatabaseAsset.Get())
        {
            for (const TSoftObjectPtr<UFacilityDefinition>& SoftDef : FacilityDB->AvailableFacilities)
            {
                if (UFacilityDefinition* Def = SoftDef.Get())
                {
                    if (Def->FacilityType == EFacilityType::Command)
                    {
                        if (UResourceManagerSubsystem* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>())
                        {
                            if (!ResourceMgr->CanAfford(Faction, Def->BuildCost))
                            {
                                return false;
                            }
                        }
                        break;
                    }
                }
            }
        }
    }

    return true;
}

/** True when the base has an operational Command Center facility. */
bool UBaseManagerSubsystem::IsCommandCenterOperational(const UStrategyBase* Base) const
{
    if (!Base)
    {
        return false;
    }

    for (const UStrategyFacility* Fac : Base->Facilities)
    {
        if (Fac && Fac->bIsOperational && Fac->BuildProgressDays <= 0 && Fac->FacilityDefinition
            && Fac->FacilityDefinition->FacilityType == EFacilityType::Command)
        {
            return true;
        }
    }

    return false;
}

/** Days remaining on an in-progress Command Center (0 if none or already operational). */
int32 UBaseManagerSubsystem::GetCommandCenterBuildDaysRemaining(const UStrategyBase* Base) const
{
    if (!Base)
    {
        return 0;
    }

    for (const UStrategyFacility* Fac : Base->Facilities)
    {
        if (Fac && Fac->FacilityDefinition && Fac->FacilityDefinition->FacilityType == EFacilityType::Command
            && !Fac->bIsOperational && Fac->BuildProgressDays > 0)
        {
            return Fac->BuildProgressDays;
        }
    }

    return 0;
}

/** Finds a base under construction at a site (CC not yet operational). */
UStrategyBase* UBaseManagerSubsystem::FindExpansionBaseAtSite(const UStrategySiteDefinition* Site) const
{
    if (!Site)
    {
        return nullptr;
    }

    auto FindInBases = [&](const TArray<UStrategyBase*>& Bases) -> UStrategyBase*
    {
        for (UStrategyBase* Base : Bases)
        {
            if (Base && Base->BuiltOnSite == Site && !IsCommandCenterOperational(Base))
            {
                return Base;
            }
        }
        return nullptr;
    };

    if (UStrategyBase* Base = FindInBases(HumanBases))
    {
        return Base;
    }

    return FindInBases(EnemyBases);
}

/** Atomic site claim: deducts CC cost and starts construction when gates still pass. */
UStrategyBase* UBaseManagerSubsystem::TryClaimExpansionSite(EFactionType Faction, UStrategySiteDefinition* TargetSite,
    UStrategyVehicle* GuardVehicle, FText BaseName)
{
    if (!TargetSite || !GuardVehicle)
    {
        return nullptr;
    }

    if (!CanBuildBaseOnSite(Faction, TargetSite))
    {
        return nullptr;
    }

    UStrategyBase* NewBase = BuildNewBase(Faction, BaseName, TargetSite->Location, TargetSite);
    if (!NewBase)
    {
        return nullptr;
    }

    NewBase->BuiltOnSite = TargetSite;

    UE_LOG(LogTemp, Display, TEXT("[BASE EXPANSION] %s claimed site '%s' with guard %s — Command Center construction started"),
        *UEnum::GetValueAsString(Faction), *TargetSite->SiteName,
        GuardVehicle->VehicleDefinition ? *GuardVehicle->VehicleDefinition->VehicleName.ToString() : TEXT("Vehicle"));

    if (UStrategyEventDispatcher* EventDisp = GetGameInstance()->GetSubsystem<UStrategyEventDispatcher>())
    {
        EventDisp->OnBaseExpansionClaimed.Broadcast(Faction, TargetSite, NewBase);
    }

    return NewBase;
}

/** Removes an in-progress expansion shell and reopens the site (no resource refund). */
void UBaseManagerSubsystem::CancelExpansionConstruction(UStrategyBase* ExpansionBase, UStrategySiteDefinition* Site)
{
    if (!ExpansionBase)
    {
        return;
    }

    const EFactionType Faction = ExpansionBase->OwningFaction;

    if (IsCommandCenterOperational(ExpansionBase))
    {
        return;
    }

    UStrategySiteDefinition* LinkedSite = Site ? Site : ExpansionBase->BuiltOnSite;

    TArray<UStrategyBase*>& Bases = GetMutableBases(Faction);
    Bases.Remove(ExpansionBase);

    if (LinkedSite)
    {
        LinkedSite->bHasBeenUsed = false;
    }

    ExpansionBase->BuiltOnSite = nullptr;
    ExpansionBase->Facilities.Empty();

    OnBaseListChanged.Broadcast(Faction);
    OnFacilityListChanged.Broadcast(Faction);

    if (UStrategyEventDispatcher* EventDisp = GetGameInstance()->GetSubsystem<UStrategyEventDispatcher>())
    {
        EventDisp->OnBaseExpansionCancelled.Broadcast(Faction, LinkedSite);
    }

    UE_LOG(LogTemp, Display, TEXT("[BASE EXPANSION] %s expansion at '%s' cancelled — site reopened for contest"),
        *UEnum::GetValueAsString(Faction),
        LinkedSite ? *LinkedSite->SiteName : TEXT("unknown site"));
}

/** Orders a vehicle to race to a site, claim it, guard CC construction, then return home. */
bool UBaseManagerSubsystem::StartBaseExpansion(EFactionType Faction, UStrategySiteDefinition* TargetSite,
    UStrategyBase* OriginBase, UStrategyVehicle* Vehicle, FText BaseName)
{
    if (!TargetSite || !OriginBase || !Vehicle)
    {
        return false;
    }

    if (!CanBuildBaseOnSite(Faction, TargetSite))
    {
        return false;
    }

    UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();
    UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>();
    if (!MissionMgr)
    {
        return false;
    }

    if (Campaign && Campaign->bBaseExpansionRequiresVehicleGuard)
    {
        const int32 MaxExpansion = Campaign->MaxActiveExpansionMissionsPerFaction;
        if (MissionMgr->CountActiveExpansionMissions(Faction) >= MaxExpansion)
        {
            UE_LOG(LogTemp, Verbose, TEXT("[BASE EXPANSION] %s at expansion mission cap (%d)"),
                *UEnum::GetValueAsString(Faction), MaxExpansion);
            return false;
        }
    }

    if (!MissionMgr->StartBaseExpansionMission(OriginBase, Vehicle, TargetSite, BaseName, Faction))
    {
        return false;
    }

    if (UStrategyEventDispatcher* EventDisp = GetGameInstance()->GetSubsystem<UStrategyEventDispatcher>())
    {
        EventDisp->OnBaseExpansionOrdered.Broadcast(Faction, TargetSite, Vehicle);
    }

    return true;
}

/** Subtracts daily extraction from each base's linked site resource pool. */
void UBaseManagerSubsystem::ProcessDailyResourceExtraction(EFactionType Faction)
{
    TArray<UStrategyBase*>& Bases = GetMutableBases(Faction);

    for (UStrategyBase* Base : Bases)
    {
        if (!Base || !Base->BuiltOnSite) continue;

        UStrategySiteDefinition* Site = Base->BuiltOnSite;
        if (!Site || Site->CurrentResources.IsEmpty()) continue; // Nothing left to extract

        FResourceStockpile Extraction = Base->GetDailyExtractionFromSite();

        // Subtract extraction from site (clamp to zero)
        Site->CurrentResources.Money = FMath::Max(0, Site->CurrentResources.Money - Extraction.Money);
        Site->CurrentResources.Metals = FMath::Max(0, Site->CurrentResources.Metals - Extraction.Metals);
        Site->CurrentResources.Biologicals = FMath::Max(0, Site->CurrentResources.Biologicals - Extraction.Biologicals);
        Site->CurrentResources.Chemicals = FMath::Max(0, Site->CurrentResources.Chemicals - Extraction.Chemicals);
        Site->CurrentResources.ExoticMaterial = FMath::Max(0, Site->CurrentResources.ExoticMaterial - Extraction.ExoticMaterial);
    }
}

/** Places Human and Enemy Command Centers on random sites with minimum separation. */
void UBaseManagerSubsystem::InitializeStartingBases(int32 MinDistanceBetweenFactions)
{
    if (AllPotentialSites.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[BASE INIT] No sites generated yet. Cannot place starting bases."));
        return;
    }

    // === 1. Pick random site for Human (Player) ===
    int32 HumanIndex = FMath::RandRange(0, AllPotentialSites.Num() - 1);
    UStrategySiteDefinition* HumanSite = AllPotentialSites[HumanIndex];

    FText HumanBaseName = FText::FromString("Command Center");
    UStrategyBase* HumanBase = BuildNewBase(EFactionType::Human, HumanBaseName, HumanSite->Location, HumanSite);

    if (HumanBase)
    {
        HumanBase->BuiltOnSite = HumanSite;
        HumanSite->bHasBeenUsed = true;
        UE_LOG(LogTemp, Display, TEXT("[BASE INIT] Human starting base placed on site #%d at (%.0f, %.0f)"),
            HumanIndex, HumanSite->Location.X, HumanSite->Location.Y);
    }

    // === 2. Pick random site for Enemy (far from Human) ===
    UStrategySiteDefinition* EnemySite = nullptr;
    int32 EnemyIndex = -1;

    TArray<int32> ValidIndices;
    for (int32 i = 0; i < AllPotentialSites.Num(); i++)
    {
        if (i == HumanIndex) continue;

        float Distance = FVector2D::Distance(AllPotentialSites[i]->Location, HumanSite->Location);
        if (Distance >= MinDistanceBetweenFactions)
        {
            ValidIndices.Add(i);
        }
    }

    if (ValidIndices.Num() > 0)
    {
        EnemyIndex = ValidIndices[FMath::RandRange(0, ValidIndices.Num() - 1)];
        EnemySite = AllPotentialSites[EnemyIndex];
    }
    else
    {
        // Fallback: pick any site that isn't the human's
        for (int32 i = 0; i < AllPotentialSites.Num(); i++)
        {
            if (i != HumanIndex)
            {
                EnemySite = AllPotentialSites[i];
                EnemyIndex = i;
                break;
            }
        }
    }

    if (EnemySite)
    {
        FText EnemyBaseName = FText::FromString("Command Center");
        UStrategyBase* EnemyBase = BuildNewBase(EFactionType::Enemy, EnemyBaseName, EnemySite->Location, EnemySite);

        if (EnemyBase)
        {
            EnemyBase->BuiltOnSite = EnemySite;
            EnemySite->bHasBeenUsed = true;
            UE_LOG(LogTemp, Display, TEXT("[BASE INIT] Enemy starting base placed on site #%d at (%.0f, %.0f)"),
                EnemyIndex, EnemySite->Location.X, EnemySite->Location.Y);
        }
    }
}