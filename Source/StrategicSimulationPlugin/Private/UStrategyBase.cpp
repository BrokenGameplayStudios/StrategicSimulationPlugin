#include "UStrategyBase.h"

bool UStrategyBase::HasOperationalCommandCenter() const
{
    for (UStrategyFacility* Fac : Facilities)
    {
        if (Fac && Fac->bIsOperational && Fac->FacilityDefinition &&
            Fac->FacilityDefinition->FacilityType == EFacilityType::Command)
        {
            return true;
        }
    }
    return false;
}

bool UStrategyBase::HasOperationalFacilityOfType(EFacilityType FacilityType) const
{
    for (UStrategyFacility* Fac : Facilities)
    {
        if (Fac && Fac->bIsOperational && Fac->FacilityDefinition &&
            Fac->FacilityDefinition->FacilityType == FacilityType)
        {
            return true;
        }
    }
    return false;
}

int32 UStrategyBase::GetTotalProductionSlots() const
{
    int32 Total = 0;
    for (UStrategyFacility* Fac : Facilities)
    {
        if (Fac && Fac->bIsOperational && Fac->FacilityDefinition)
        {
            Total += Fac->FacilityDefinition->ProductionSlots;
        }
    }
    return Total;
}

int32 UStrategyBase::GetTotalCapacityForType(EFacilityType FacilityType) const
{
    int32 Total = 0;
    for (UStrategyFacility* Fac : Facilities)
    {
        if (Fac && Fac->bIsOperational && Fac->FacilityDefinition && Fac->FacilityDefinition->FacilityType == FacilityType)
        {
            Total += Fac->FacilityDefinition->Capacity;
        }
    }
    return Total;
}

bool UStrategyBase::HasFacilityOfType(EFacilityType FacilityType) const
{
    for (UStrategyFacility* Fac : Facilities)
    {
        if (Fac && Fac->FacilityDefinition && Fac->FacilityDefinition->FacilityType == FacilityType)
            return true;
    }
    return false;
}

int32 UStrategyBase::GetCountOfType(EFacilityType FacilityType) const
{
    int32 Count = 0;
    for (UStrategyFacility* Fac : Facilities)
    {
        if (Fac && Fac->FacilityDefinition && Fac->FacilityDefinition->FacilityType == FacilityType)
            Count++;
    }
    return Count;
}

bool UStrategyBase::IsOperational() const
{
    return HasOperationalCommandCenter();
}

void UStrategyBase::AddFacility(UStrategyFacility* NewFacility)
{
    if (NewFacility)
    {
        Facilities.Add(NewFacility);
        OnFacilitiesChanged.Broadcast(this);
    }
}

void UStrategyBase::UpdatePowerFromFacilities()
{
    PowerProvided = 0;
    PowerDraw = 0;

    for (UStrategyFacility* Facility : Facilities)
    {
        if (Facility && Facility->bIsOperational)
        {
            PowerProvided += Facility->FacilityDefinition->PowerProvided;
            PowerDraw += Facility->FacilityDefinition->PowerDraw;
        }
    }

    UE_LOG(LogTemp, Display, TEXT("[POWER] Base '%s' net power updated to %d (Provided %d | Draw %d)"),
        *BaseName.ToString(), GetNetPower(), PowerProvided, PowerDraw);
}