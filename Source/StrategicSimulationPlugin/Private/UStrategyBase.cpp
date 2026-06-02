#include "UStrategyBase.h"
#include "UStrategyFacility.h"
#include "UStrategySoldier.h"
#include "USoldierManagerSubsystem.h"
#include "Engine/Engine.h"          // ← Added for GEngine
#include "Kismet/GameplayStatics.h" // ← Added for safety

TArray<UStrategySoldier*> UStrategyBase::GetStationedSoldiers() const
{
    TArray<UStrategySoldier*> Soldiers;

    // Fixed: UObject does not have GetGameInstance(). Use GEngine safely.
    UWorld* World = GEngine ? GEngine->GetCurrentPlayWorld() : nullptr;
    if (World)
    {
        if (UGameInstance* GI = World->GetGameInstance())
        {
            if (USoldierManagerSubsystem* SoldierMgr = GI->GetSubsystem<USoldierManagerSubsystem>())
            {
                // Assuming Enemy for now (you can change to Human when player side is added)
                Soldiers = SoldierMgr->GetRoster(EFactionType::Enemy);
            }
        }
    }

    return Soldiers;
}

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
        if (Facility && Facility->bIsOperational && Facility->FacilityDefinition)
        {
            PowerProvided += Facility->FacilityDefinition->PowerProvided;
            PowerDraw += Facility->FacilityDefinition->PowerDraw;
        }
    }

    UE_LOG(LogTemp, Display, TEXT("[POWER] Base '%s' net power updated to %d (Provided %d | Draw %d)"),
        *BaseName.ToString(), GetNetPower(), PowerProvided, PowerDraw);
}

bool UStrategyBase::CanBuildFacilityType(EFacilityType FacilityType) const
{
    // Find the definition for this facility type
    UFacilityDefinition* Def = nullptr;
    // (Your game initializer already loads FacilityDatabase — we use the same lookup you already have)
    for (UFacilityDefinition* Candidate : /* your facility database array — replace with actual lookup if needed */)
    {
        if (Candidate && Candidate->FacilityType == FacilityType)
        {
            Def = Candidate;
            break;
        }
    }
    if (!Def) return false;

    // Check every prerequisite
    for (EFacilityType Req : Def->PrerequisiteFacilities)
    {
        if (!HasOperationalFacilityOfType(Req))
            return false;
    }
    return true;
}