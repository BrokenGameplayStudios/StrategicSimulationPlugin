#include "UBaseManagerSubsystem.h"
#include "UStrategyEventDispatcher.h"
#include "Engine/Engine.h"
#include "UFacilityDatabase.h"
#include "UResourceManagerSubsystem.h"
#include "UStrategyCampaignSubsystem.h"
#include "UStrategyBase.h"
#include "UStrategyFacility.h"

void UBaseManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    if (UTimeManagerSubsystem* TimeMgr = GetGameInstance()->GetSubsystem<UTimeManagerSubsystem>())
    {
        TimeMgr->OnDayPassed.AddDynamic(this, &UBaseManagerSubsystem::OnDayPassed);
    }

    UE_LOG(LogTemp, Display, TEXT("UBaseManagerSubsystem initialized — multiple-base systems online"));
}

UStrategyBase* UBaseManagerSubsystem::BuildNewBase(EFactionType Faction, FText BaseName, FVector2D MapLocation)
{
    UResourceManagerSubsystem* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>();
    UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();

    if (!ResourceMgr || !Campaign)
    {
        UE_LOG(LogTemp, Error, TEXT("[BASE] Critical — Missing subsystems"));
        return nullptr;
    }

    // === STRICT AFFORDABILITY CHECK FIRST ===
    UFacilityDefinition* CommandDef = nullptr;
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

    FResourceStockpile CurrentRes = ResourceMgr->GetResources(Faction);
    if (!CommandDef || CurrentRes.Money < CommandDef->BuildCost.Money || CurrentRes.Supplies < CommandDef->BuildCost.Supplies)
    {
        UE_LOG(LogTemp, Warning, TEXT("[BASE] Cannot afford Command Center — skipping new base creation"));
        UE_LOG(LogTemp, Warning, TEXT("[BUILD] Cannot afford Command Center (%d Money, %d Supplies needed)"),
            CommandDef ? CommandDef->BuildCost.Money : 8500, CommandDef ? CommandDef->BuildCost.Supplies : 600);
        return nullptr;   // ← ABORT — no base is created
    }

    // Deduct cost
    FResourceStockpile NegativeCost = CommandDef->BuildCost;
    NegativeCost.Money = -NegativeCost.Money;
    NegativeCost.Supplies = -NegativeCost.Supplies;
    ResourceMgr->AddResources(Faction, NegativeCost);

    // === Now safe to create the base ===
    UStrategyBase* NewBase = NewObject<UStrategyBase>();
    NewBase->BaseName = BaseName.IsEmpty() ? FText::FromString("New Base") : BaseName;
    NewBase->MapLocation = MapLocation;

    if (Faction == EFactionType::Human)
        HumanBases.Add(NewBase);
    else
        EnemyBases.Add(NewBase);

    OnBaseListChanged.Broadcast(Faction);
    OnFacilityListChanged.Broadcast(Faction);

    UE_LOG(LogTemp, Display, TEXT("Built new base '%s' for %s at (%.0f, %.0f)"),
        *NewBase->BaseName.ToString(), *UEnum::GetValueAsString(Faction), MapLocation.X, MapLocation.Y);

    // === Start proper construction of Command Center (respects BuildTimeDays) ===
    UStrategyFacility* CommandFacility = BuildFacility(Faction, CommandDef, NewBase);

    if (CommandFacility)
    {
        CommandFacility->bIsOperational = false;   // will become operational after build time
        NewBase->AddFacility(CommandFacility);
        NewBase->UpdatePowerFromFacilities();

        UE_LOG(LogTemp, Display, TEXT("[FACILITY] ✅ Command Center construction started in new base '%s' (%d days)"),
            *NewBase->BaseName.ToString(), CommandDef->BuildTimeDays);

        if (UStrategyEventDispatcher* EventDisp = GetGameInstance()->GetSubsystem<UStrategyEventDispatcher>())
            EventDisp->OnFacilityCompleted.Broadcast(Faction, CommandFacility);  // you can change this to OnFacilityStarted if you prefer
    }

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

UStrategyFacility* UBaseManagerSubsystem::BuildFacility(EFactionType Faction, UFacilityDefinition* FacilityDef, UStrategyBase* TargetBase /*= nullptr*/)
{
    if (!FacilityDef) return nullptr;

    UResourceManagerSubsystem* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>();
    if (ResourceMgr)
    {
        FResourceStockpile Current = ResourceMgr->GetResources(Faction);
        if (Current.Money < FacilityDef->BuildCost.Money || Current.Supplies < FacilityDef->BuildCost.Supplies)
        {
            UE_LOG(LogTemp, Warning, TEXT("[BUILD] Cannot afford %s (%d Money, %d Supplies needed)"),
                *FacilityDef->FacilityName.ToString(), FacilityDef->BuildCost.Money, FacilityDef->BuildCost.Supplies);
            return nullptr;
        }

        FResourceStockpile NegativeCost = FacilityDef->BuildCost;
        NegativeCost.Money = -NegativeCost.Money;
        NegativeCost.Supplies = -NegativeCost.Supplies;
        ResourceMgr->AddResources(Faction, NegativeCost);
    }

    TArray<UStrategyBase*>& Bases = GetMutableBases(Faction);
    if (Bases.IsEmpty())
    {
        FVector2D DefaultLoc = FVector2D(960.f, 540.f);
        BuildNewBase(Faction, FText::FromString("Command Center"), DefaultLoc);
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
        UE_LOG(LogTemp, Error, TEXT("[BASE] No valid base found to build %s!"), *FacilityDef->FacilityName.ToString());
        return nullptr;
    }

    if (FacilityDef->FacilityType != EFacilityType::Command)
    {
        if (!ChosenBase->HasOperationalCommandCenter())
        {
            UE_LOG(LogTemp, Warning, TEXT("[BASE] Cannot build %s — Command Center must be operational first in base '%s'!"),
                *FacilityDef->FacilityName.ToString(), *ChosenBase->BaseName.ToString());
            return nullptr;
        }
    }

    UStrategyFacility* NewFacility = NewObject<UStrategyFacility>();
    NewFacility->FacilityDefinition = FacilityDef;
    NewFacility->BuildProgressDays = FacilityDef->BuildTimeDays;
    NewFacility->bIsOperational = false;
    NewFacility->CurrentPowerDraw = FacilityDef->PowerDraw;

    ChosenBase->AddFacility(NewFacility);

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
                if (Fac && Fac->bIsOperational && Fac->FacilityDefinition)
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

// === NEW: Daily Repair Tick for all VehicleRepair facilities ===
void UBaseManagerSubsystem::SimulateDailyRepairs(EFactionType Faction)
{
    UE_LOG(LogTemp, Verbose, TEXT("[DAILY SIM] Starting daily repair/medical simulation for %s"), *UEnum::GetValueAsString(Faction));

    for (UStrategyBase* Base : GetBasesInternal(Faction))
    {
        if (!Base) continue;

        // Call on EVERY operational facility — this makes Medical bays work exactly like VehicleRepair
        for (UStrategyFacility* Fac : Base->Facilities)
        {
            if (Fac && Fac->bIsOperational && Fac->FacilityDefinition)
            {
                Fac->SimulateDailyRepair(Base);
            }
        }
    }
}