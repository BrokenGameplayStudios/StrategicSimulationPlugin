#include "UStrategyBase.h"
#include "UStrategyFacility.h"
#include "UStrategySoldier.h"
#include "UStrategyCampaignSubsystem.h"
#include "USoldierManagerSubsystem.h"
#include "UFacilityDatabase.h"
#include "UFacilityDefinition.h"
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

int32 UStrategyBase::GetTotalBuiltOfType(EFacilityType FacilityType) const
{
    int32 Count = 0;
    for (UStrategyFacility* Fac : Facilities)
        if (Fac && Fac->FacilityDefinition && Fac->FacilityDefinition->FacilityType == FacilityType && Fac->bIsOperational)
            Count++;
    return Count;
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

// NEW HELPER — counts ANY facility of this type (operational OR under construction)
bool UStrategyBase::HasAnyFacilityOfType(EFacilityType FacilityType) const
{
    for (UStrategyFacility* Fac : Facilities)
    {
        if (Fac && Fac->FacilityDefinition && Fac->FacilityDefinition->FacilityType == FacilityType)
            return true;
    }
    return false;
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
    UFacilityDefinition* Def = nullptr;

    if (UWorld* World = GetWorld())
    {
        if (UStrategyCampaignSubsystem* Campaign = World->GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>())
        {
            if (UFacilityDatabase* FacilityDB = Campaign->GetFacilityDatabase())
            {
                for (const TSoftObjectPtr<UFacilityDefinition>& CandidateSoft : FacilityDB->AvailableFacilities)
                {
                    UFacilityDefinition* Candidate = CandidateSoft.Get();
                    if (!Candidate)
                    {
                        Candidate = CandidateSoft.LoadSynchronous();
                    }
                    if (Candidate && Candidate->FacilityType == FacilityType)
                    {
                        Def = Candidate;
                        break;
                    }
                }
            }
        }
    }

    if (!Def)
    {
        UE_LOG(LogTemp, Warning, TEXT("[FACILITY] CanBuildFacilityType: No definition found for %s - allowing build as fallback"), *UEnum::GetValueAsString(FacilityType));
        return true;
    }

    // NEW: Use HasFacilityOfType (existence) for prereqs so timing issues don't block the tech tree
    // (operational check is still used inside HasOperationalFacilityOfType for "use" logic)
    for (EFacilityType Req : Def->PrerequisiteFacilities)
    {
        if (!HasFacilityOfType(Req))   // CHANGED from HasOperationalFacilityOfType
        {
            UE_LOG(LogTemp, Verbose, TEXT("[FACILITY] %s skipped — missing prerequisite %s (existence check)"),
                *UEnum::GetValueAsString(FacilityType), *UEnum::GetValueAsString(Req));
            return false;
        }
    }

    return true;
}

void UStrategyBase::AddPOW(UStrategySoldier* Soldier)
{
    if (!Soldier) return;
    ContainedPOWs.AddUnique(Soldier);
    Soldier->StationedBase = this;
    UE_LOG(LogTemp, Display, TEXT("[POW] %s added to base '%s' Containment"), *Soldier->SoldierName, *BaseName.ToString());
}

void UStrategyBase::AddKIABody(UStrategySoldier* Soldier)
{
    if (!Soldier) return;
    StoredKIABodies.AddUnique(Soldier);
    UE_LOG(LogTemp, Display, TEXT("[KIA] Body of %s added to base '%s' Autopsy"), *Soldier->SoldierName, *BaseName.ToString());
}

void UStrategyBase::ProcessContainment()
{
    // Called by Containment facility daily
    if (ContainedPOWs.Num() == 0) return;
    // Bonus logic moved to facility (we just provide the list)
}

void UStrategyBase::ProcessAutopsy()
{
    // Called by Autopsy facility daily
    if (StoredKIABodies.Num() == 0) return;
    // Bodies will be disposed after processing
}