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

    int32 CurrentPOW = ContainedPOWs.Num();
    int32 MaxSlots = GetTotalContainmentSlots();

    if (CurrentPOW >= MaxSlots)
    {
        UE_LOG(LogTemp, Warning, TEXT("[POW] Containment FULL at base '%s' (%d/%d) — soldier '%s' could not be stored!"),
            *BaseName.ToString(), CurrentPOW, MaxSlots, *Soldier->SoldierName);
        return;   // do not add if full
    }

    ContainedPOWs.AddUnique(Soldier);
    Soldier->StationedBase = this;
    Soldier->bIsPOW = true;

    UE_LOG(LogTemp, Display, TEXT("[POW] %s added to base '%s' Containment (%d/%d)"),
        *Soldier->SoldierName, *BaseName.ToString(), ContainedPOWs.Num(), MaxSlots);
}

void UStrategyBase::AddKIABody(UStrategySoldier* Soldier)
{
    if (!Soldier) return;

    int32 CurrentKIA = StoredKIABodies.Num();
    int32 MaxSlots = GetTotalAutopsySlots();

    if (CurrentKIA >= MaxSlots)
    {
        UE_LOG(LogTemp, Warning, TEXT("[KIA] Autopsy FULL at base '%s' (%d/%d) — body of '%s' could not be stored!"),
            *BaseName.ToString(), CurrentKIA, MaxSlots, *Soldier->SoldierName);
        return;
    }

    StoredKIABodies.AddUnique(Soldier);
    Soldier->bIsKIA = true;

    UE_LOG(LogTemp, Display, TEXT("[KIA] Body of %s added to base '%s' Autopsy (%d/%d)"),
        *Soldier->SoldierName, *BaseName.ToString(), StoredKIABodies.Num(), MaxSlots);
}

// Helper implementations
int32 UStrategyBase::GetTotalContainmentSlots() const
{
    int32 Slots = 0;
    for (UStrategyFacility* Fac : Facilities)
    {
        if (Fac && Fac->FacilityDefinition && Fac->FacilityDefinition->FacilityType == EFacilityType::Containment && Fac->bIsOperational)
            Slots += Fac->FacilityDefinition->ProductionSlots;
    }
    return Slots;
}

int32 UStrategyBase::GetTotalAutopsySlots() const
{
    int32 Slots = 0;
    for (UStrategyFacility* Fac : Facilities)
    {
        if (Fac && Fac->FacilityDefinition && Fac->FacilityDefinition->FacilityType == EFacilityType::Autopsy && Fac->bIsOperational)
            Slots += Fac->FacilityDefinition->ProductionSlots;
    }
    return Slots;
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

TArray<UStrategySoldier*> UStrategyBase::GetContainedPOWs() const
{
    return ContainedPOWs;
}

void UStrategyBase::ReleasePOW(UStrategySoldier* POW)
{
    if (!POW || !ContainedPOWs.Contains(POW))
        return;

    ContainedPOWs.Remove(POW);

    // === Give player a resource bonus for releasing the POW ===
    UResourceManagerSubsystem* ResourceMgr = GetWorld()->GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>();
    if (ResourceMgr)
    {
        FResourceStockpile Bonus;
        Bonus.Money = 1200;   // tweak these numbers as you like
        Bonus.ResearchPoints = 600;
        // Bonus.Metals     = 200;    // optional

        ResourceMgr->AddResources(OwningFaction, Bonus);

        UE_LOG(LogTemp, Display, TEXT("[POW] Released %s from base '%s' Containment — +1200 Money +600 Research"),
            *POW->SoldierName, *BaseName.ToString());
    }

    // POW is now gone (player got the benefit)
    POW->ConditionalBeginDestroy();
}

void UStrategyBase::ProcessKIABody(UStrategySoldier* Body)
{
    if (!Body || !StoredKIABodies.Contains(Body)) return;

    StoredKIABodies.Remove(Body);

    // === Give research / intel bonus via ResourceManager (correct subsystem) ===
    UResourceManagerSubsystem* ResourceMgr = GetWorld()->GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>();
    if (ResourceMgr)
    {
        FResourceStockpile Bonus;
        Bonus.ResearchPoints = 800;     // ← tweak this number as you like
        // Bonus.ExoticMaterial = 50;   // example: add exotics if you want
        // Bonus.Money = 200;           // example: small cash bonus

        ResourceMgr->AddResources(OwningFaction, Bonus);

        UE_LOG(LogTemp, Display, TEXT("[KIA] Autopsied %s at base '%s' — +800 Research Points granted"),
            *Body->SoldierName, *BaseName.ToString());
    }

    // Body is now fully processed and deleted
    Body->ConditionalBeginDestroy();
}

FResourceStockpile UStrategyBase::GetDailyExtractionFromSite() const
{
    FResourceStockpile TotalExtraction;

    for (UStrategyFacility* Facility : Facilities)
    {
        // Only count facilities that are built and operational
        if (!Facility || Facility->BuildProgressDays > 0 || !Facility->bIsOperational)
            continue;

        if (UFacilityDefinition* Def = Facility->FacilityDefinition)
        {
            TotalExtraction.Money += Def->ExtractionPerDay.Money;
            TotalExtraction.Metals += Def->ExtractionPerDay.Metals;
            TotalExtraction.Biologicals += Def->ExtractionPerDay.Biologicals;
            TotalExtraction.Chemicals += Def->ExtractionPerDay.Chemicals;
            TotalExtraction.ExoticMaterial += Def->ExtractionPerDay.ExoticMaterial;
        }
    }

    return TotalExtraction;
}