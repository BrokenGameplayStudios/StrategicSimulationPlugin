#include "UStrategyFacility.h"
#include "UStrategyVehicle.h"
#include "UStrategySoldier.h"
#include "UFacilityDefinition.h"
#include "UStrategyBase.h"
#include "UStrategyCampaignSubsystem.h"
#include "UResourceManagerSubsystem.h"
#include "Engine/World.h"
#include "USoldierClassDatabase.h"
#include "UVehicleDatabase.h"
#include "USoldierManagerSubsystem.h"
#include "UStrategyEventDispatcher.h"

void UStrategyFacility::SimulateDailyRepair(UStrategyBase* InOwningBase)
{
    if (!FacilityDefinition || !bIsOperational || !InOwningBase)
        return;

    UE_LOG(LogTemp, Verbose, TEXT("[FACILITY TICK] %s (%s) in base '%s' is operational — processing daily simulation"),
        *FacilityDefinition->FacilityName.ToString(),
        *UEnum::GetValueAsString(FacilityDefinition->FacilityType),
        *InOwningBase->BaseName.ToString());

    // === VEHICLE REPAIR ===
    if (FacilityDefinition->RepairHealthPerDay > 0 && FacilityDefinition->FacilityType == EFacilityType::VehicleRepair)
    {
        int32 RepairsRemaining = FacilityDefinition->ProductionSlots;

        UE_LOG(LogTemp, Verbose, TEXT("[REPAIR TICK] Vehicle Repair Shop can repair up to %d vehicles (+%d HP each)"),
            FacilityDefinition->ProductionSlots, FacilityDefinition->RepairHealthPerDay);

        for (UStrategyFacility* Hanger : InOwningBase->Facilities)
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

    // === SOLDIER MEDICAL HEALING ===
    if (FacilityDefinition->FacilityType == EFacilityType::Medical)
    {
        int32 HealsRemaining = FacilityDefinition->ProductionSlots;

        UE_LOG(LogTemp, Verbose, TEXT("[MEDICAL TICK] Medical Bay can heal up to %d soldiers (+%d HP each)"),
            FacilityDefinition->ProductionSlots, FacilityDefinition->RepairHealthPerDay);

        for (UStrategySoldier* Soldier : InOwningBase->GetStationedSoldiers())
        {
            if (!Soldier || !Soldier->NeedsHealing() || HealsRemaining <= 0)
                continue;

            Soldier->Heal(FacilityDefinition->RepairHealthPerDay);
            HealsRemaining--;
        }
    }
}

// ======================== UNIFIED PRODUCTION + CONSTRUCTION ========================
bool UStrategyFacility::HasFreeProductionSlot() const
{
    return GetAvailableProductionSlots() > 0;
}

int32 UStrategyFacility::GetAvailableProductionSlots() const
{
    if (!FacilityDefinition) return 0;
    return FacilityDefinition->ProductionSlots - ActiveProductionJobs.Num();
}

bool UStrategyFacility::StartProduction(EProductionType Type, UObject* TargetAsset, int32 BaseDays)
{
    if (!HasFreeProductionSlot() || !TargetAsset) return false;

    FProductionJob Job;
    Job.Type = Type;
    Job.TargetAsset = TargetAsset;
    Job.RemainingDays = BaseDays;
    Job.SpeedMultiplier = FacilityDefinition ? FacilityDefinition->ProductionSpeedMultiplier : 1.0f;
    Job.AssignedBase = OwningBase;

    ActiveProductionJobs.Add(Job);

    UE_LOG(LogTemp, Display, TEXT("[PRODUCTION] %s queued %s (slots left: %d)"),
        *FacilityDefinition->FacilityName.ToString(),
        *UEnum::GetValueAsString(Type), GetAvailableProductionSlots());

    return true;
}

void UStrategyFacility::AdvanceProductionDay()
{
    for (int32 i = ActiveProductionJobs.Num() - 1; i >= 0; --i)
    {
        FProductionJob& Job = ActiveProductionJobs[i];
        if (Job.RemainingDays > 0)
            Job.RemainingDays = FMath::Max(0, Job.RemainingDays - 1);

        if (Job.RemainingDays <= 0)
        {
            CompleteProductionJob(i);
        }
    }
}

void UStrategyFacility::CompleteProductionJob(int32 Index)
{
    if (Index < 0 || Index >= ActiveProductionJobs.Num()) return;

    FProductionJob Job = ActiveProductionJobs[Index];
    ActiveProductionJobs.RemoveAt(Index);

    if (!Job.TargetAsset || !OwningBase) return;

    UWorld* World = OwningBase->GetWorld();
    if (!World) return;

    if (Job.Type == EProductionType::Soldier)
    {
        UE_LOG(LogTemp, Display, TEXT("[RECRUIT] Training complete → Soldier ready from %s"), *FacilityDefinition->FacilityName.ToString());

        if (USoldierManagerSubsystem* SoldierMgr = World->GetGameInstance()->GetSubsystem<USoldierManagerSubsystem>())
        {
            SoldierMgr->FinishSoldierTraining(OwningBase, Job.TargetAsset);
        }
    }
    else if (Job.Type == EProductionType::Vehicle)
    {
        UE_LOG(LogTemp, Display, TEXT("[VEHICLE] Construction complete → Ship ready from %s"), *FacilityDefinition->FacilityName.ToString());

        if (UStrategyEventDispatcher* Disp = World->GetGameInstance()->GetSubsystem<UStrategyEventDispatcher>())
            Disp->OnSoldierRecruited.Broadcast(EFactionType::Enemy, nullptr); // UI refresh
    }
    else if (Job.Type == EProductionType::Facility)
    {
        UE_LOG(LogTemp, Display, TEXT("[FACILITY] %s completed and is now operational"), *FacilityDefinition->FacilityName.ToString());
    }
}

bool UStrategyFacility::StartConstruction(UFacilityDefinition* Def)
{
    if (!Def || !HasFreeProductionSlot()) return false;

    FProductionJob Job;
    Job.Type = EProductionType::Facility;
    Job.TargetAsset = Def;
    Job.RemainingDays = Def->BuildTimeDays;
    Job.AssignedBase = OwningBase;
    ActiveProductionJobs.Add(Job);

    UE_LOG(LogTemp, Display, TEXT("[BUILD] Order accepted → %s started construction (%d days)"),
        *Def->FacilityName.ToString(), Def->BuildTimeDays);
    return true;
}

bool UStrategyFacility::CancelConstruction(int32 JobIndex, bool bFullRefund)
{
    if (JobIndex < 0 || JobIndex >= ActiveProductionJobs.Num()) return false;

    UFacilityDefinition* Def = nullptr;
    if (ActiveProductionJobs[JobIndex].Type == EProductionType::Facility)
        Def = Cast<UFacilityDefinition>(ActiveProductionJobs[JobIndex].TargetAsset);

    if (bFullRefund && Def && OwningBase)
    {
        if (UWorld* World = OwningBase->GetWorld())
        {
            if (UResourceManagerSubsystem* ResMgr = World->GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>())
            {
                ResMgr->AddResources(EFactionType::Enemy, Def->BuildCost);
                UE_LOG(LogTemp, Display, TEXT("[BUILD] Cancelled %s — full refund issued"), *Def->FacilityName.ToString());
            }
        }
    }

    ActiveProductionJobs.RemoveAt(JobIndex);
    return true;
}

void UStrategyFacility::AdvanceConstructionDay()
{
    AdvanceProductionDay();   // unified
}

void UStrategyFacility::SimulateDaily()
{
    AdvanceConstructionDay();
    SimulateDailyRepair(OwningBase);
}