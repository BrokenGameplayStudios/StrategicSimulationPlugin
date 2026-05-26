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

const TArray<UStrategyFacility*>& UBaseManagerSubsystem::GetFacilities(EFactionType Faction) const
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
    // Human facilities
    for (UStrategyFacility* Fac : HumanFacilities)
    {
        if (Fac && !Fac->bIsOperational && Fac->FacilityDefinition)
        {
            int32 OldProgress = Fac->BuildProgressDays;
            Fac->BuildProgressDays--;
            UE_LOG(LogTemp, Display, TEXT("[BASE] Human %s progress: %d → %d days left"),
                *Fac->FacilityDefinition->FacilityName.ToString(), OldProgress, Fac->BuildProgressDays);

            if (Fac->BuildProgressDays <= 0)
            {
                Fac->bIsOperational = true;
                if (UStrategyEventDispatcher* EventDisp = GetGameInstance()->GetSubsystem<UStrategyEventDispatcher>())
                    EventDisp->OnFacilityCompleted.Broadcast(EFactionType::Human, Fac);
                UE_LOG(LogTemp, Display, TEXT("[BASE] ✅ Human facility COMPLETED: %s"), *Fac->FacilityDefinition->FacilityName.ToString());
            }
        }
    }

    // Enemy facilities
    for (UStrategyFacility* Fac : EnemyFacilities)   
    {
        if (Fac && !Fac->bIsOperational && Fac->FacilityDefinition)
        {
            int32 OldProgress = Fac->BuildProgressDays;
            Fac->BuildProgressDays--;
            UE_LOG(LogTemp, Display, TEXT("[BASE] Enemy %s progress: %d → %d days left"),
                *Fac->FacilityDefinition->FacilityName.ToString(), OldProgress, Fac->BuildProgressDays);

            if (Fac->BuildProgressDays <= 0)
            {
                Fac->bIsOperational = true;
                if (UStrategyEventDispatcher* EventDisp = GetGameInstance()->GetSubsystem<UStrategyEventDispatcher>())
                    EventDisp->OnFacilityCompleted.Broadcast(EFactionType::Enemy, Fac);
                UE_LOG(LogTemp, Display, TEXT("[BASE] ✅ Enemy facility COMPLETED: %s"), *Fac->FacilityDefinition->FacilityName.ToString());
            }
        }
    }
}

int32 UBaseManagerSubsystem::GetTotalBarracksCapacity(EFactionType Faction) const
{
    int32 Total = 0;
    const TArray<UStrategyFacility*>& Facilities = (Faction == EFactionType::Human) ? HumanFacilities : EnemyFacilities;

    for (UStrategyFacility* Fac : Facilities)
    {
        if (Fac && Fac->bIsOperational && Fac->FacilityDefinition)
        {
            Total += Fac->FacilityDefinition->Capacity;
        }
    }
    return Total;
}

bool UBaseManagerSubsystem::HasFacilityOfType(EFactionType Faction, EFacilityType FacilityType) const
{
    const TArray<UStrategyFacility*>& Facilities = (Faction == EFactionType::Human) ? HumanFacilities : EnemyFacilities;

    for (UStrategyFacility* Fac : Facilities)
    {
        if (Fac && Fac->FacilityDefinition && Fac->FacilityDefinition->FacilityType == FacilityType)
            return true;
    }
    return false;
}

void UBaseManagerSubsystem::AdvanceFacilityConstruction(EFactionType Faction)
{
    TArray<UStrategyFacility*>& Facilities = (Faction == EFactionType::Human) ? HumanFacilities : EnemyFacilities;

    for (UStrategyFacility* Fac : Facilities)
    {
        if (Fac && !Fac->bIsOperational && Fac->FacilityDefinition)
        {
            int32 OldProgress = Fac->BuildProgressDays;
            Fac->BuildProgressDays--;

            UE_LOG(LogTemp, Display, TEXT("[BASE] %s %s progress: %d → %d days left"),
                *UEnum::GetValueAsString(Faction),
                *Fac->FacilityDefinition->FacilityName.ToString(),
                OldProgress, Fac->BuildProgressDays);

            if (Fac->BuildProgressDays <= 0)
            {
                // FIXED: Temporarily include THIS facility's power contribution when checking
                int32 TempProvided = Fac->FacilityDefinition->PowerProvided;
                int32 TempDraw = Fac->FacilityDefinition->PowerDraw;

                int32 CurrentNetPower = GetNetPower(Faction);           // current operational facilities
                int32 NetPowerWithThisFacility = CurrentNetPower + TempProvided - TempDraw;

                Fac->bIsOperational = (NetPowerWithThisFacility >= 0);

                if (Fac->bIsOperational)
                {
                    if (UStrategyEventDispatcher* EventDisp = GetGameInstance()->GetSubsystem<UStrategyEventDispatcher>())
                        EventDisp->OnFacilityCompleted.Broadcast(Faction, Fac);

                    UE_LOG(LogTemp, Display, TEXT("[BASE] ✅ %s facility COMPLETED: %s (Net Power: %d)"),
                        *UEnum::GetValueAsString(Faction),
                        *Fac->FacilityDefinition->FacilityName.ToString(),
                        NetPowerWithThisFacility);
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("[BASE] ⚠️ %s facility completed but has INSUFFICIENT POWER (Net Power: %d)"),
                        *UEnum::GetValueAsString(Faction),
                        NetPowerWithThisFacility);
                }
            }
        }
    }
}

int32 UBaseManagerSubsystem::GetCurrentCountOfType(EFactionType Faction, EFacilityType FacilityType) const
{
    const TArray<UStrategyFacility*>& Facilities = (Faction == EFactionType::Human) ? HumanFacilities : EnemyFacilities;
    int32 Count = 0;

    for (UStrategyFacility* Fac : Facilities)
    {
        if (Fac && Fac->FacilityDefinition && Fac->FacilityDefinition->FacilityType == FacilityType)
            Count++;
    }
    return Count;
}