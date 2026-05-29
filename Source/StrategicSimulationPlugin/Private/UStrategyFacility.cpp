#include "UStrategyFacility.h"
#include "UStrategyVehicle.h"
#include "UStrategySoldier.h"
#include "UFacilityDefinition.h"
#include "UStrategyBase.h"
#include "UStrategyCampaignSubsystem.h"
#include "UResourceManagerSubsystem.h"
#include "Engine/World.h"

void UStrategyFacility::SimulateDailyRepair(UStrategyBase* OwningBase)
{
    if (!FacilityDefinition || !bIsOperational || !OwningBase)
        return;

    // Change from Display to Verbose so it doesn't spam unless you want to see it
    UE_LOG(LogTemp, Verbose, TEXT("[FACILITY TICK] %s (%s) in base '%s' is operational — processing daily simulation"),
        *FacilityDefinition->FacilityName.ToString(),
        *UEnum::GetValueAsString(FacilityDefinition->FacilityType),
        *OwningBase->BaseName.ToString());

    // === VEHICLE REPAIR ===
    if (FacilityDefinition->RepairHealthPerDay > 0 && FacilityDefinition->FacilityType == EFacilityType::VehicleRepair)
    {
        int32 RepairsRemaining = FacilityDefinition->Capacity;

        UE_LOG(LogTemp, Verbose, TEXT("[REPAIR TICK] Vehicle Repair Shop can repair up to %d vehicles (+%d HP each)"),
            FacilityDefinition->Capacity, FacilityDefinition->RepairHealthPerDay);

        for (UStrategyFacility* Hanger : OwningBase->Facilities)
        {
            if (!Hanger || !Hanger->FacilityDefinition || Hanger->FacilityDefinition->FacilityType != EFacilityType::Hanger)
                continue;

            for (UStrategyVehicle* Vehicle : Hanger->ParkedVehicles)
            {
                if (!Vehicle || !Vehicle->NeedsRepair() || RepairsRemaining <= 0)
                    continue;

                Vehicle->CurrentHealth = FMath::Min(
                    Vehicle->CurrentHealth + FacilityDefinition->RepairHealthPerDay,
                    Vehicle->VehicleDefinition ? Vehicle->VehicleDefinition->MaxHealth : 100
                );

                Vehicle->UpdateDamageStateFromHealth();

                UE_LOG(LogTemp, Display, TEXT("[REPAIR] %s repaired +%d HP → %d/%d"),
                    *Vehicle->VehicleDefinition->VehicleName.ToString(),
                    FacilityDefinition->RepairHealthPerDay,
                    Vehicle->CurrentHealth,
                    Vehicle->VehicleDefinition ? Vehicle->VehicleDefinition->MaxHealth : 100);

                RepairsRemaining--;

                if (!Vehicle->NeedsRepair())
                {
                    UE_LOG(LogTemp, Display, TEXT("[REPAIR] %s has reached full health"),
                        *Vehicle->VehicleDefinition->VehicleName.ToString());
                }
            }
        }
    }

    // === SOLDIER MEDICAL HEALING (exact parallel to vehicle repair) ===
    if (FacilityDefinition->FacilityType == EFacilityType::Medical)
    {
        int32 HealsRemaining = FacilityDefinition->Capacity;

        UE_LOG(LogTemp, Verbose, TEXT("[MEDICAL TICK] Medical Bay can heal up to %d soldiers (+%d HP each)"),
            FacilityDefinition->Capacity, FacilityDefinition->RepairHealthPerDay);

        for (UStrategySoldier* Soldier : OwningBase->GetStationedSoldiers())
        {
            if (!Soldier || !Soldier->NeedsHealing() || HealsRemaining <= 0)
                continue;

            Soldier->Heal(FacilityDefinition->RepairHealthPerDay);   // uses same per-day value as vehicles

            HealsRemaining--;
        }
    }
}

// === NEW CONSTRUCTION QUEUE FUNCTIONS (added on top of your code) ===

bool UStrategyFacility::CanQueueMoreOfType(EFacilityType Type) const
{
    int32 CurrentCount = 0;
    if (OwningBase)
    {
        for (UStrategyFacility* Fac : OwningBase->Facilities)
        {
            if (Fac && Fac->FacilityDefinition && Fac->FacilityDefinition->FacilityType == Type)
                CurrentCount++;
        }
    }

    for (const FConstructionJob& Job : ActiveConstructionJobs)
    {
        if (Job.FacilityDef && Job.FacilityDef->FacilityType == Type)
            CurrentCount++;
    }

    return CurrentCount < (FacilityDefinition ? FacilityDefinition->MaxBuilt : 999);
}

bool UStrategyFacility::StartConstruction(UFacilityDefinition* Def)
{
    if (!Def || !CanQueueMoreOfType(Def->FacilityType)) return false;

    FConstructionJob Job;
    Job.FacilityDef = Def;
    Job.RemainingDays = Def->BuildTimeDays;
    ActiveConstructionJobs.Add(Job);

    UE_LOG(LogTemp, Display, TEXT("[BUILD] Order accepted → %s started construction (%d days)"),
        *Def->FacilityName.ToString(), Def->BuildTimeDays);
    return true;
}

void UStrategyFacility::AdvanceConstructionDay()
{
    for (int32 i = ActiveConstructionJobs.Num() - 1; i >= 0; --i)
    {
        FConstructionJob& Job = ActiveConstructionJobs[i];
        if (Job.RemainingDays > 0)
            Job.RemainingDays--;

        if (Job.RemainingDays <= 0 && Job.FacilityDef && OwningBase)
        {
            UStrategyFacility* NewFacility = NewObject<UStrategyFacility>();
            NewFacility->FacilityDefinition = Job.FacilityDef;
            NewFacility->bIsOperational = true;
            NewFacility->OwningBase = OwningBase;

            OwningBase->AddFacility(NewFacility);
            OwningBase->UpdatePowerFromFacilities();

            UE_LOG(LogTemp, Display, TEXT("[FACILITY] %s completed construction and is now operational"),
                *Job.FacilityDef->FacilityName.ToString());

            ActiveConstructionJobs.RemoveAt(i);
        }
    }
}

bool UStrategyFacility::CancelConstruction(int32 JobIndex, bool bFullRefund)
{
    if (JobIndex < 0 || JobIndex >= ActiveConstructionJobs.Num()) return false;

    UFacilityDefinition* Def = ActiveConstructionJobs[JobIndex].FacilityDef;
    if (bFullRefund && Def && OwningBase)
    {
        // Safe version that compiles with your current UStrategyBase
        if (UWorld* World = OwningBase->GetWorld())
        {
            if (UResourceManagerSubsystem* ResMgr = World->GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>())
            {
                // TODO: Replace EFactionType::Enemy with real faction once UStrategyBase stores it
                ResMgr->AddResources(EFactionType::Enemy, Def->BuildCost);
                UE_LOG(LogTemp, Display, TEXT("[BUILD] Cancelled %s — full refund issued"), *Def->FacilityName.ToString());
            }
        }
    }

    ActiveConstructionJobs.RemoveAt(JobIndex);
    return true;
}

void UStrategyFacility::SimulateDaily()
{
    AdvanceConstructionDay();           // process build queue
    SimulateDailyRepair(OwningBase);    // your original daily repair/healing logic (unchanged)
}