#include "UBaseManagerSubsystem.h"
#include "UStrategyEventDispatcher.h"
#include "Engine/Engine.h"
#include "UFacilityDatabase.h"
#include "UResourceManagerSubsystem.h"
#include "UStrategyCampaignSubsystem.h"
#include "UStrategyBase.h"
#include "UStrategyFacility.h"
#include "USoldierManagerSubsystem.h"

void UBaseManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    if (UTimeManagerSubsystem* TimeMgr = GetGameInstance()->GetSubsystem<UTimeManagerSubsystem>())
    {
        TimeMgr->OnDayPassed.AddDynamic(this, &UBaseManagerSubsystem::OnDayPassed);
    }

    UE_LOG(LogTemp, Display, TEXT("UBaseManagerSubsystem initialized — multiple-base systems online"));
}

// === FULL FUNCTION: UBaseManagerSubsystem::BuildNewBase (COMMANDER FIXED) ===
// Commander is now spawned using the FIRST class in SoldierClassDatabase (your DA_Sol_Commander).
// It is placed in the initial Command Center and does NOT use barracks slots.
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

    // === NORMAL PAID EXPANSION (unchanged) ===
    UResourceManagerSubsystem* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>();
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

    if (ResourceMgr && CommandDef)
    {
        if (!ResourceMgr->CanAfford(Faction, CommandDef->BuildCost))
        {
            UE_LOG(LogTemp, Warning, TEXT("[BASE] Cannot afford Command Center — ABORTING new base"));
            if (Faction == EFactionType::Enemy) EnemyBases.Remove(NewBase);
            else HumanBases.Remove(NewBase);
            return nullptr;
        }

        FResourceStockpile NegativeCost = CommandDef->BuildCost;
        NegativeCost.Money = -NegativeCost.Money;
        NegativeCost.Metals = -NegativeCost.Metals;
        NegativeCost.Biologicals = -NegativeCost.Biologicals;
        NegativeCost.Chemicals = -NegativeCost.Chemicals;
        ResourceMgr->AddResources(Faction, NegativeCost);

        UE_LOG(LogTemp, Display, TEXT("[BUILD] Order accepted — deducted cost for Command Center"));
    }

    UStrategyFacility* CommandFacility = BuildFacility(Faction, CommandDef, NewBase);

    if (CommandFacility)
    {
        CommandFacility->bIsOperational = false;
        NewBase->UpdatePowerFromFacilities();

        UE_LOG(LogTemp, Display, TEXT("[FACILITY] Command Center construction started in new base '%s' (%d days)"),
            *NewBase->BaseName.ToString(), CommandDef ? CommandDef->BuildTimeDays : 4);
    }

    UE_LOG(LogTemp, Display, TEXT("[AI] Expanded to new base '%s'"), *NewBase->BaseName.ToString());
    return NewBase;
}

const TArray<UStrategyBase*>& UBaseManagerSubsystem::GetBases(EFactionType Faction) const
{
    return GetBasesInternal(Faction);
}

const TArray<UStrategyBase*>& UBaseManagerSubsystem::GetBasesInternal(EFactionType Faction) const
{
    return (Faction == EFactionType::Human) ? HumanBases : EnemyBases;
}

TArray<UStrategyBase*>& UBaseManagerSubsystem::GetMutableBases(EFactionType Faction)
{
    return (Faction == EFactionType::Human) ? HumanBases : EnemyBases;
}

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
        UE_LOG(LogTemp, Warning, TEXT("[BaseManager] Tried to build facility with no bases. Expansion should go through TryBuildBaseOnSite()."));
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

    if (FacilityDef->BuildTimeDays > 0)
    {
        NewFacility->StartConstruction(FacilityDef);
        UE_LOG(LogTemp, Display, TEXT("[BUILD DEBUG] StartConstruction called (BuildTime = %d days)"), FacilityDef->BuildTimeDays);
    }
    else
    {
        NewFacility->bIsOperational = true;
        UE_LOG(LogTemp, Display, TEXT("[BUILD DEBUG] Instant operational facility (0 build days)"));
    }

    OnFacilityListChanged.Broadcast(Faction);

    UE_LOG(LogTemp, Display, TEXT("✅ [BUILD] Started construction of %s in base '%s' (%d days)"),
        *FacilityDef->FacilityName.ToString(), *ChosenBase->BaseName.ToString(), NewFacility->BuildProgressDays);

    return NewFacility;
}

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

int32 UBaseManagerSubsystem::GetNetPower(EFactionType Faction) const
{
    return GetTotalPowerProvided(Faction) - GetTotalPowerDrawn(Faction);
}

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

bool UBaseManagerSubsystem::HasFacilityOfType(EFactionType Faction, EFacilityType FacilityType) const
{
    for (UStrategyBase* Base : GetBasesInternal(Faction))
    {
        if (Base && Base->HasFacilityOfType(FacilityType))
            return true;
    }
    return false;
}

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

void UBaseManagerSubsystem::AdvanceFacilityConstruction(EFactionType Faction)
{
    for (UStrategyBase* Base : GetBasesInternal(Faction))
    {
        if (!Base) continue;

        bool bAnyCompletedThisTick = false;

        for (UStrategyFacility* Fac : Base->Facilities)
        {
            if (!Fac || Fac->bIsOperational || !Fac->FacilityDefinition) continue;

            Fac->BuildProgressDays--;

            if (Fac->BuildProgressDays <= 0)
            {
                bAnyCompletedThisTick = true;

                Fac->bIsOperational = true;

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

void UBaseManagerSubsystem::OnDayPassed(int32 NewDay)
{
    AdvanceFacilityConstruction(EFactionType::Human);
    AdvanceFacilityConstruction(EFactionType::Enemy);
    AdvanceAllConstruction();           // NEW: Processes the construction queue every day
    SimulateDailyRepairs(EFactionType::Human);
    SimulateDailyRepairs(EFactionType::Enemy);
}

bool UBaseManagerSubsystem::CanBuildNewBase(EFactionType Faction) const
{
    const TArray<UStrategyBase*>& Bases = GetBasesInternal(Faction);
    if (Bases.Num() >= 10)
    {
        return false;
    }

    int32 OperationalHangers = GetNumberOfOperationalHangers(Faction);
    int32 MaxAllowedBases = 1 + OperationalHangers;

    return Bases.Num() < MaxAllowedBases;
}

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
            if (Fac && Fac->FacilityDefinition && Fac->FacilityDefinition->FacilityType == EFacilityType::Hanger)
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
            if (Fac && Fac->FacilityDefinition && Fac->FacilityDefinition->FacilityType == EFacilityType::Hanger)
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

// ==================== PASTE THIS FULL FUNCTION (replace the old AddDiscoveredSite) ====================
UStrategySiteDefinition* UBaseManagerSubsystem::AddDiscoveredSite(EFactionType Faction, FVector2D Location, EStrategySiteType Type, float OptionalScore)
{
    // Find existing site in AllPotentialSites (exact or nearest match)
    UStrategySiteDefinition* ExistingSite = nullptr;
    float BestDist = MAX_FLT;

    for (UStrategySiteDefinition* Site : AllPotentialSites)
    {
        if (!Site) continue;
        float Dist = FVector2D::Distance(Site->Location, Location);
        if (Dist < BestDist)
        {
            BestDist = Dist;
            ExistingSite = Site;
        }
    }

    if (!ExistingSite)
    {
        // Fallback: create new only if truly new
        ExistingSite = NewObject<UStrategySiteDefinition>(this);
        ExistingSite->Location = Location;
        ExistingSite->SiteType = Type;
        AllPotentialSites.Add(ExistingSite);
    }

    // Mark discovered for the correct faction
    if (Faction == EFactionType::Human)
    {
        DiscoveredSitesHuman.AddUnique(ExistingSite);
    }
    else
    {
        DiscoveredSitesEnemy.AddUnique(ExistingSite);
    }

    UE_LOG(LogTemp, Display, TEXT("[DISCOVERY] %s discovered node at (%.0f, %.0f)"),
        *UEnum::GetValueAsString(Faction), Location.X, Location.Y);

    return ExistingSite;
}

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

            UStrategySiteDefinition* NewSite = NewObject<UStrategySiteDefinition>();
            NewSite->Location = NewLoc;
            NewSite->SiteType = EStrategySiteType::PotentialBase;
            NewSite->SiteName = FString::Printf(TEXT("Potential Base %d"), SitesPlaced + 1);

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

bool UBaseManagerSubsystem::CanBuildBaseOnSite(EFactionType Faction, UStrategySiteDefinition* Site) const
{
    if (!Site) return false;

    // Site must be discovered by this faction
    const TArray<UStrategySiteDefinition*>& Discovered = (Faction == EFactionType::Human)
        ? DiscoveredSitesHuman
        : DiscoveredSitesEnemy;

    if (!Discovered.Contains(Site)) return false;

    // Site must not already be used
    if (Site->bHasBeenUsed) return false;

    // Check base limit
    if (!CanBuildNewBase(Faction)) return false;

    return true;
}

bool UBaseManagerSubsystem::TryBuildBaseOnSite(EFactionType Faction, UStrategySiteDefinition* TargetSite, FText BaseName)
{
    if (!CanBuildBaseOnSite(Faction, TargetSite)) return false;

    // Build the base at the site's location
    UStrategyBase* NewBase = BuildNewBase(Faction, BaseName, TargetSite->Location);
    if (!NewBase) return false;

    // Link the base to the site
    NewBase->BuiltOnSite = TargetSite;
    TargetSite->bHasBeenUsed = true;

    UE_LOG(LogTemp, Display, TEXT("[BASE EXPANSION] %s built new base '%s' on site '%s' at (%.0f, %.0f)"),
        *UEnum::GetValueAsString(Faction), *BaseName.ToString(), *TargetSite->SiteName,
        TargetSite->Location.X, TargetSite->Location.Y);

    return true;
}