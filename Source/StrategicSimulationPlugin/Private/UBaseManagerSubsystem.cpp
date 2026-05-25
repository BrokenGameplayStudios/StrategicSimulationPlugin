#include "UBaseManagerSubsystem.h"
#include "UStrategyEventDispatcher.h"
#include "Engine/Engine.h"

void UBaseManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    if (UTimeManagerSubsystem* TimeMgr = GetGameInstance()->GetSubsystem<UTimeManagerSubsystem>())
    {
        TimeMgr->OnDayPassed.AddDynamic(this, &UBaseManagerSubsystem::OnDayPassed);
    }

    UE_LOG(LogTemp, Display, TEXT("UBaseManagerSubsystem initialized — base systems online"));
}

UStrategyFacility* UBaseManagerSubsystem::BuildFacility(EFactionType Faction, UFacilityDefinition* FacilityDef)
{
    if (!FacilityDef) return nullptr;

    UStrategyFacility* NewFacility = NewObject<UStrategyFacility>();
    NewFacility->FacilityDefinition = FacilityDef;
    NewFacility->BuildProgressDays = FacilityDef->BuildTimeDays;
    NewFacility->bIsOperational = false;
    NewFacility->CurrentPowerDraw = FacilityDef->PowerDraw;

    if (Faction == EFactionType::Human)
        HumanFacilities.Add(NewFacility);
    else
        EnemyFacilities.Add(NewFacility);

    OnFacilityListChanged.Broadcast(Faction);
    UE_LOG(LogTemp, Display, TEXT("Started construction of %s for %s (%d days)"), *FacilityDef->FacilityName.ToString(), *UEnum::GetValueAsString(Faction), NewFacility->BuildProgressDays);

    return NewFacility;
}

TArray<UStrategyFacility*> UBaseManagerSubsystem::GetFacilities(EFactionType Faction) const
{
    return (Faction == EFactionType::Human) ? HumanFacilities : EnemyFacilities;
}

int32 UBaseManagerSubsystem::GetTotalPowerProvided(EFactionType Faction) const
{
    int32 Total = 0;
    const TArray<UStrategyFacility*>& Facilities = (Faction == EFactionType::Human) ? HumanFacilities : EnemyFacilities;
    for (UStrategyFacility* Fac : Facilities)
    {
        if (Fac && Fac->bIsOperational)
            Total += Fac->FacilityDefinition->PowerProvided;
    }
    return Total;
}

int32 UBaseManagerSubsystem::GetTotalPowerDrawn(EFactionType Faction) const
{
    int32 Total = 0;
    const TArray<UStrategyFacility*>& Facilities = (Faction == EFactionType::Human) ? HumanFacilities : EnemyFacilities;
    for (UStrategyFacility* Fac : Facilities)
    {
        if (Fac && Fac->bIsOperational)
            Total += Fac->CurrentPowerDraw;
    }
    return Total;
}

int32 UBaseManagerSubsystem::GetNetPower(EFactionType Faction) const
{
    return GetTotalPowerProvided(Faction) - GetTotalPowerDrawn(Faction);
}

void UBaseManagerSubsystem::OnDayPassed(int32 NewDay)
{
    for (UStrategyFacility* Fac : HumanFacilities)
    {
        if (Fac && !Fac->bIsOperational)
        {
            Fac->BuildProgressDays--;
            if (Fac->BuildProgressDays <= 0)
            {
                Fac->bIsOperational = true;
                if (UStrategyEventDispatcher* EventDisp = GetGameInstance()->GetSubsystem<UStrategyEventDispatcher>())
                    EventDisp->OnFacilityCompleted.Broadcast(EFactionType::Human, Fac);
                UE_LOG(LogTemp, Display, TEXT("[BASE] Human facility completed: %s"), *Fac->FacilityDefinition->FacilityName.ToString());
            }
        }
    }

    for (UStrategyFacility* Fac : EnemyFacilities)
    {
        if (Fac && !Fac->bIsOperational)
        {
            Fac->BuildProgressDays--;
            if (Fac->BuildProgressDays <= 0)
            {
                Fac->bIsOperational = true;
                if (UStrategyEventDispatcher* EventDisp = GetGameInstance()->GetSubsystem<UStrategyEventDispatcher>())
                    EventDisp->OnFacilityCompleted.Broadcast(EFactionType::Enemy, Fac);
                UE_LOG(LogTemp, Display, TEXT("[BASE] Enemy facility completed: %s"), *Fac->FacilityDefinition->FacilityName.ToString());
            }
        }
    }
}