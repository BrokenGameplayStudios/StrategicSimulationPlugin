#include "UStrategyBase.h"
#include "UStrategyFacility.h"
#include "UStrategySoldier.h"
#include "UStrategyCampaignSubsystem.h"
#include "USoldierManagerSubsystem.h"
#include "UFacilityDatabase.h"
#include "UFacilityDefinition.h"
#include "UMissionManagerSubsystem.h"
#include "UStrategyVehicle.h"
#include "Engine/Engine.h"          // ← Added for GEngine
#include "Kismet/GameplayStatics.h" // ← Added for safety

/** Returns roster soldiers stationed at this base. */
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

/** True when operational Command facility exists. */
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

/** True when operational facility of type exists. */
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

/** Sums production slots from operational facilities. */
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

/** Sums capacity for operational facilities of type. */
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

/** Counts operational facilities of type. */
int32 UStrategyBase::GetTotalBuiltOfType(EFacilityType FacilityType) const
{
    int32 Count = 0;
    for (UStrategyFacility* Fac : Facilities)
        if (Fac && Fac->FacilityDefinition && Fac->FacilityDefinition->FacilityType == FacilityType && Fac->bIsOperational)
            Count++;
    return Count;
}

/** True if any facility of type exists. */
bool UStrategyBase::HasFacilityOfType(EFacilityType FacilityType) const
{
    for (UStrategyFacility* Fac : Facilities)
    {
        if (Fac && Fac->FacilityDefinition && Fac->FacilityDefinition->FacilityType == FacilityType)
            return true;
    }
    return false;
}

/** Counts all facilities of type including under construction. */
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

/** True if any facility of type exists (built or in progress). */
bool UStrategyBase::HasAnyFacilityOfType(EFacilityType FacilityType) const
{
    for (UStrategyFacility* Fac : Facilities)
    {
        if (Fac && Fac->FacilityDefinition && Fac->FacilityDefinition->FacilityType == FacilityType)
            return true;
    }
    return false;
}

/** True when base has operational Command Center. */
bool UStrategyBase::IsOperational() const
{
    return HasOperationalCommandCenter();
}

/** Adds facility and broadcasts OnFacilitiesChanged. */
void UStrategyBase::AddFacility(UStrategyFacility* NewFacility)
{
    if (NewFacility)
    {
        Facilities.Add(NewFacility);
        OnFacilitiesChanged.Broadcast(this);
    }
}

/** Recalculates base power provided and draw. */
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

/** Checks prerequisite facilities exist for a facility type. */
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

// Helper implementations
/** Sums containment production slots. */
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

/** Sums autopsy production slots. */
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

/** Returns copy of contained POW array. */
TArray<UStrategySoldier*> UStrategyBase::GetContainedPOWs() const
{
    return ContainedPOWs;
}

/** Grants research bonus and removes processed KIA body. */
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

/** Sums daily extraction from operational facilities. */
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

/** Returns roster soldiers stationed at this base. */
int32 UStrategyBase::GetStationedSoldiersCount() const
{
    int32 Count = 0;

    UGameInstance* GI = GetTypedOuter<UGameInstance>();
    if (!GI)
    {
        if (UWorld* World = GetWorld())
            GI = World->GetGameInstance();
    }

    if (GI)
    {
        if (UStrategyCampaignSubsystem* Campaign = GI->GetSubsystem<UStrategyCampaignSubsystem>())
        {
            if (USoldierManagerSubsystem* SoldierMgr = Campaign->GetSoldierManager())
            {
                // This matches exactly how DebugPrintFullBaseState does it
                for (UStrategySoldier* Soldier : SoldierMgr->GetRoster(OwningFaction))
                {
                    if (Soldier && Soldier->StationedBase == this)
                    {
                        Count++;
                    }
                }
            }
        }
    }

    return Count;
}

/** Counts soldiers on missions from this base. */
int32 UStrategyBase::GetSoldiersOnMissionCount() const
{
    int32 Count = 0;

    UGameInstance* GI = GetTypedOuter<UGameInstance>();
    if (!GI)
    {
        if (UWorld* World = GetWorld())
            GI = World->GetGameInstance();
    }

    if (GI)
    {
        if (UStrategyCampaignSubsystem* Campaign = GI->GetSubsystem<UStrategyCampaignSubsystem>())
        {
            if (UMissionManagerSubsystem* MissionMgr = Campaign->GetMissionManager())
            {
                for (UMissionGroup* Mission : MissionMgr->ActiveMissions)
                {
                    if (Mission && Mission->OriginBase == this)
                    {
                        for (UStrategyVehicle* Vehicle : Mission->VehiclesInFleet)
                        {
                            if (Vehicle)
                            {
                                Count += Vehicle->CurrentPassengers.Num();
                            }
                        }
                    }
                }
            }
        }
    }

    return Count;
}

/** Returns first operational hangar or nullptr. */
UStrategyFacility* UStrategyBase::FindFirstOperationalHangar() const
{
    for (UStrategyFacility* Facility : Facilities)
    {
        if (Facility && Facility->bIsOperational && Facility->FacilityDefinition &&
            Facility->FacilityDefinition->FacilityType == EFacilityType::Hanger)
        {
            return Facility;
        }
    }
    return nullptr;
}

/** Counts vehicles in operational hangars. */
int32 UStrategyBase::GetStationedVehiclesCount() const
{
    int32 Count = 0;

    for (UStrategyFacility* Facility : Facilities)
    {
        if (Facility && Facility->bIsOperational && Facility->FacilityDefinition &&
            Facility->FacilityDefinition->FacilityType == EFacilityType::Hanger)
        {
            Count += Facility->ParkedVehicles.Num();
        }
    }

    return Count;
}

/** Counts vehicles on live missions from this base. */
int32 UStrategyBase::GetVehiclesOnMissionCount() const
{
    int32 Count = 0;

    UGameInstance* GI = GetTypedOuter<UGameInstance>();
    if (!GI)
    {
        if (UWorld* World = GetWorld())
            GI = World->GetGameInstance();
    }

    if (GI)
    {
        if (UStrategyCampaignSubsystem* Campaign = GI->GetSubsystem<UStrategyCampaignSubsystem>())
        {
            if (UMissionManagerSubsystem* MissionMgr = Campaign->GetMissionManager())
            {
                for (UMissionGroup* Mission : MissionMgr->ActiveMissions)
                {
                    if (!Mission || Mission->OriginBase != this || !Mission->bMovementActivated)
                    {
                        continue;
                    }

                    for (UStrategyVehicle* Vehicle : Mission->VehiclesInFleet)
                    {
                        if (Vehicle && !Vehicle->IsDestroyed() &&
                            Vehicle->GetMissionPhase() != EVehicleMissionPhase::Docked)
                        {
                            Count++;
                        }
                    }
                }
            }
        }
    }

    return Count;
}