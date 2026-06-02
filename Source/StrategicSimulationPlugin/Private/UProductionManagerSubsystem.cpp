#include "UProductionManagerSubsystem.h"
#include "UStrategyFacility.h"
#include "USoldierManagerSubsystem.h"
#include "UBaseManagerSubsystem.h"          // ← NEW INCLUDE
#include "UStrategyEventDispatcher.h"
#include "UStrategyBase.h"
#include "USoldierClassDefinition.h"
#include "UVehicleDefinition.h"
#include "Kismet/GameplayStatics.h"

void UProductionManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
}

void UProductionManagerSubsystem::CompleteJob(FProductionJob Job, UStrategyFacility* Facility)
{
    if (!Job.TargetAsset) return;

    if (Job.Type == EProductionType::Soldier)
    {
        CompleteSoldierJob(Job, Facility);
    }
    else if (Job.Type == EProductionType::Vehicle)
    {
        CompleteVehicleJob(Job, Facility);
    }
    else if (Job.Type == EProductionType::Facility)
    {
        CompleteFacilityJob(Job, Facility);
    }
    else if (Job.Type == EProductionType::Research)
    {
        CompleteResearchJob(Job, Facility);
    }
}

void UProductionManagerSubsystem::CompleteResearchJob(const FProductionJob& Job, UStrategyFacility* Facility)
{
    UResearchManagerSubsystem* ResearchMgr = GetGameInstance()->GetSubsystem<UResearchManagerSubsystem>();
    UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();

    if (!ResearchMgr || !Campaign) return;

    UResearchTechDefinition* TechDef = Cast<UResearchTechDefinition>(Job.TargetAsset);
    if (!TechDef) return;

    // Mark as completed so unlocks work
    UE_LOG(LogTemp, Display, TEXT("[RESEARCH] %s research completed: %s"),
        *UEnum::GetValueAsString(Facility ? Facility->OwningBase->OwningFaction : EFactionType::Human),
        *TechDef->ProjectName.ToString());

    // Broadcast so UI / AI knows
    ResearchMgr->OnResearchListChanged.Broadcast(Facility ? Facility->OwningBase->OwningFaction : EFactionType::Human);

    // The placeholder in Campaign::HasCompletedResearch will now let IsItemUnlocked succeed for everything this tech unlocks
    if (UStrategyEventDispatcher* Disp = GetGameInstance()->GetSubsystem<UStrategyEventDispatcher>())
    {
        Disp->OnResearchCompleted.Broadcast(Facility ? Facility->OwningBase->OwningFaction : EFactionType::Human, TechDef);
    }
}

void UProductionManagerSubsystem::CompleteSoldierJob(const FProductionJob& Job, UStrategyFacility* Facility)
{
    UStrategyBase* UseBase = Facility ? Facility->OwningBase : Job.AssignedBase;

    if (USoldierManagerSubsystem* SoldierMgr = GetGameInstance()->GetSubsystem<USoldierManagerSubsystem>())
    {
        // Determine which faction owns this base (so Human gets their own soldiers)
        EFactionType JobFaction = EFactionType::Human;
        UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
        if (BaseMgr && UseBase)
        {
            if (BaseMgr->GetBases(EFactionType::Enemy).Contains(UseBase))
                JobFaction = EFactionType::Enemy;
            else if (BaseMgr->GetBases(EFactionType::Human).Contains(UseBase))
                JobFaction = EFactionType::Human;
        }

        SoldierMgr->FinishSoldierTraining(UseBase, Job.TargetAsset, JobFaction);   // ← NOW 3 ARGS
    }
}

void UProductionManagerSubsystem::CompleteVehicleJob(const FProductionJob& Job, UStrategyFacility* Facility)
{
    UVehicleDefinition* VehDef = Cast<UVehicleDefinition>(Job.TargetAsset);
    if (!VehDef || !Facility || Facility->FacilityDefinition->FacilityType != EFacilityType::Hanger) return;

    UObject* Outer = GetGameInstance() ? static_cast<UObject*>(GetGameInstance()) : static_cast<UObject*>(Facility);

    UStrategyVehicle* NewVehicle = NewObject<UStrategyVehicle>(Outer);
    NewVehicle->VehicleDefinition = VehDef;
    NewVehicle->CurrentHanger = Facility;
    NewVehicle->HomeHanger = Facility;
    NewVehicle->HomeBase = Facility->OwningBase;
    NewVehicle->CurrentHealth = VehDef->MaxHealth;
    NewVehicle->RemainingFuelDays = 30;

    Facility->ParkedVehicles.Add(NewVehicle);

    UE_LOG(LogTemp, Display, TEXT("[VEHICLE] %s added to hanger '%s' (now %d parked)"),
        *VehDef->VehicleName.ToString(), *Facility->FacilityDefinition->FacilityName.ToString(), Facility->ParkedVehicles.Num());
}

void UProductionManagerSubsystem::CompleteFacilityJob(const FProductionJob& Job, UStrategyFacility* Facility)
{
    if (Facility)
    {
        Facility->bIsOperational = true;
        UE_LOG(LogTemp, Display, TEXT("[FACILITY] %s completed and is now operational"), *Facility->FacilityDefinition->FacilityName.ToString());
    }
}