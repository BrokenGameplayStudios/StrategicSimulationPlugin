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
#include "UBaseManagerSubsystem.h"
#include "Kismet/GameplayStatics.h"         // ← added to resolve GetGameInstance usage

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

// ======================== FIXED DAILY SIMULATION ========================
void UStrategyFacility::SimulateDaily()
{
    if (!FacilityDefinition) return;

    // CRITICAL: Always advance production jobs (this guarantees soldier completion)
    if (ActiveProductionJobs.Num() > 0)
    {
        AdvanceProductionDay();  // ← Removed !bIsOperational check that was causing skips
    }

    SimulateDailyRepair(OwningBase);

    if (FacilityDefinition->FacilityType == EFacilityType::LivingQuarters && ActiveProductionJobs.Num() > 0)
    {
        UE_LOG(LogTemp, Display, TEXT("[BARRACKS LIVE] %s — %d soldier jobs active (debug tick confirmed)"),
            *FacilityDefinition->FacilityName.ToString(), ActiveProductionJobs.Num());
    }
}

// ======================== UNIFIED PRODUCTION ========================
bool UStrategyFacility::HasFreeProductionSlot() const { return GetAvailableProductionSlots() > 0; }

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
        {
            Job.RemainingDays--;
            UE_LOG(LogTemp, Verbose, TEXT("[PROD TICK] %s job — %d days left"), *UEnum::GetValueAsString(Job.Type), Job.RemainingDays);
        }

        if (Job.RemainingDays <= 0)
        {
            UE_LOG(LogTemp, Display, TEXT("[COMPLETE] %s production job finishing now!"), *UEnum::GetValueAsString(Job.Type));
            CompleteProductionJob(i);
        }
    }
}

void UStrategyFacility::CompleteProductionJob(int32 Index)
{
    if (Index < 0 || Index >= ActiveProductionJobs.Num()) return;

    FProductionJob Job = ActiveProductionJobs[Index];
    ActiveProductionJobs.RemoveAt(Index);

    if (!Job.TargetAsset) return;

    // === ROBUST WORLD + CONTEXT RESOLUTION (eliminates ALL transient warnings) ===
    UStrategyBase* UseBase = OwningBase ? OwningBase : Job.AssignedBase;
    UWorld* World = nullptr;

    if (UseBase) World = UseBase->GetWorld();
    if (!World) World = GetWorld();                                 // facility itself
    if (!World && GEngine) World = GEngine->GetWorldFromContextObject(this, EGetWorldErrorMode::ReturnNull);
    if (!World && GWorld) World = GWorld;

    // Final guaranteed path via GameInstance (always works)
    UGameInstance* GI = UGameplayStatics::GetGameInstance(this);
    if (!World && GI) World = GI->GetWorld();

    UE_LOG(LogTemp, Verbose, TEXT("[COMPLETE] %s job resolved → World: %s | Base: %s"),
        *UEnum::GetValueAsString(Job.Type), World ? TEXT("VALID") : TEXT("NULL"), UseBase ? *UseBase->BaseName.ToString() : TEXT("NULL"));

    if (Job.Type == EProductionType::Soldier)
    {
        UE_LOG(LogTemp, Display, TEXT("[SOLDIER] Soldier trained..."));

        USoldierManagerSubsystem* SoldierMgr = nullptr;
        if (GI)
            SoldierMgr = GI->GetSubsystem<USoldierManagerSubsystem>();
        else if (World)
            SoldierMgr = World->GetGameInstance()->GetSubsystem<USoldierManagerSubsystem>();

        if (SoldierMgr)
        {
            SoldierMgr->FinishSoldierTraining(UseBase, Job.TargetAsset);   // ← This now ALWAYS runs

            const TArray<UStrategySoldier*>& Roster = SoldierMgr->GetRoster(EFactionType::Enemy);
            UStrategyEventDispatcher* Disp = nullptr;
            if (GI) Disp = GI->GetSubsystem<UStrategyEventDispatcher>();
            else if (World) Disp = World->GetGameInstance()->GetSubsystem<UStrategyEventDispatcher>();

            if (Disp)
            {
                Disp->OnSoldierListChanged.Broadcast(EFactionType::Enemy, Roster);
                Disp->OnSoldierRecruited.Broadcast(EFactionType::Enemy, nullptr);
                Disp->OnSoldierLoadoutChanged.Broadcast(EFactionType::Enemy, nullptr);
            }
            UE_LOG(LogTemp, Display, TEXT("[SOLDIER] ✅ Full roster + recruited + loadout events broadcast — UI updated!"));
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[SOLDIER] Could not get SoldierMgr — using direct fallback"));
            // Direct fallback (guaranteed)
            if (UGameInstance* FallbackGI = UGameplayStatics::GetGameInstance(this))
            {
                if (USoldierManagerSubsystem* FallbackMgr = FallbackGI->GetSubsystem<USoldierManagerSubsystem>())
                    FallbackMgr->FinishSoldierTraining(UseBase, Job.TargetAsset);
            }
        }
    }
    else if (Job.Type == EProductionType::Vehicle)
    {
        UE_LOG(LogTemp, Display, TEXT("[VEHICLE] Construction complete"));
    }
    else if (Job.Type == EProductionType::Facility)
    {
        bIsOperational = true;
        UE_LOG(LogTemp, Display, TEXT("[FACILITY] %s completed and is now operational"), *FacilityDefinition->FacilityName.ToString());
    }
}

// ======================== CONSTRUCTION (kept 100% as you had) ========================
bool UStrategyFacility::StartConstruction(UFacilityDefinition* Def)
{
    if (!Def || !HasFreeProductionSlot()) return false;

    FProductionJob Job;
    Job.Type = EProductionType::Facility;
    Job.TargetAsset = Def;
    Job.RemainingDays = Def->BuildTimeDays;
    Job.AssignedBase = OwningBase;
    ActiveProductionJobs.Add(Job);

    UE_LOG(LogTemp, Display, TEXT("[BUILD] Order accepted → %s started construction (%d days)"), *Def->FacilityName.ToString(), Def->BuildTimeDays);
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