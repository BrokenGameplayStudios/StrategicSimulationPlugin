#include "UResearchManagerSubsystem.h"
#include "UStrategyEventDispatcher.h"
#include "Engine/Engine.h"
#include "UBaseManagerSubsystem.h"
#include "UStrategyFacility.h"
#include "UStrategyBase.h"

void UResearchManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    UE_LOG(LogTemp, Display, TEXT("UResearchManagerSubsystem initialized"));
}

UActiveResearchProject* UResearchManagerSubsystem::StartResearch(EFactionType Faction, UResearchTechDefinition* ProjectDef)
{
    if (!ProjectDef) return nullptr;

    UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
    if (!BaseMgr) return nullptr;

    // Find any base with an operational Laboratory and free slot
    for (UStrategyBase* Base : BaseMgr->GetBases(Faction))
    {
        for (UStrategyFacility* Lab : Base->Facilities)
        {
            if (Lab && Lab->FacilityDefinition && Lab->FacilityDefinition->FacilityType == EFacilityType::Laboratory)
            {
                if (Lab->HasFreeProductionSlot())
                {
                    if (Lab->StartProduction(EProductionType::Research, ProjectDef, ProjectDef->ResearchDays))
                    {
                        // Create dummy object for Blueprint/UI compatibility (no queue)
                        UActiveResearchProject* NewProject = NewObject<UActiveResearchProject>();
                        NewProject->ResearchDefinition = ProjectDef;
                        NewProject->RemainingDays = ProjectDef->ResearchDays;
                        NewProject->bIsCompleted = false;
                        // NewProject->OwningBase = Base;

                        OnResearchListChanged.Broadcast(Faction);
                        UE_LOG(LogTemp, Display, TEXT("[RESEARCH] Started '%s' for %s (%d days) in lab"),
                            *ProjectDef->ProjectName.ToString(), *UEnum::GetValueAsString(Faction), ProjectDef->ResearchDays);

                        return NewProject;
                    }
                }
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("[RESEARCH] Cannot start %s — No free lab slot found!"), *ProjectDef->ProjectName.ToString());
    return nullptr;
}

TArray<UActiveResearchProject*> UResearchManagerSubsystem::GetActiveResearch(EFactionType Faction) const
{
    TArray<UActiveResearchProject*> Result;
    UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
    if (!BaseMgr) return Result;

    const TArray<UStrategyBase*>& Bases = BaseMgr->GetBases(Faction);
    for (UStrategyBase* Base : Bases)
    {
        for (UStrategyFacility* Facility : Base->Facilities)
        {
            if (Facility && Facility->FacilityDefinition && Facility->FacilityDefinition->FacilityType == EFacilityType::Laboratory)
            {
                for (const FProductionJob& Job : Facility->ActiveProductionJobs)
                {
                    if (Job.Type == EProductionType::Research && Job.TargetAsset)
                    {
                        UActiveResearchProject* ActiveProject = NewObject<UActiveResearchProject>(GetTransientPackage());
                        ActiveProject->ResearchDefinition = Cast<UResearchTechDefinition>(Job.TargetAsset);
                        ActiveProject->RemainingDays = Job.RemainingDays;
                        ActiveProject->bIsCompleted = false;
                        // ActiveProject->OwningBase = Base;
                        Result.Add(ActiveProject);
                    }
                }
            }
        }
    }
    return Result;
}

bool UResearchManagerSubsystem::IsResearchInProgress(EFactionType Faction, UResearchTechDefinition* Tech) const
{
    if (!Tech) return false;

    UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
    if (!BaseMgr) return false;

    for (UStrategyBase* Base : BaseMgr->GetBases(Faction))
    {
        for (UStrategyFacility* Facility : Base->Facilities)
        {
            if (Facility && Facility->FacilityDefinition && Facility->FacilityDefinition->FacilityType == EFacilityType::Laboratory)
            {
                for (const FProductionJob& Job : Facility->ActiveProductionJobs)
                {
                    if (Job.Type == EProductionType::Research && Job.TargetAsset == Tech)
                        return true;
                }
            }
        }
    }
    return false;
}

bool UResearchManagerSubsystem::HasCompletedResearch(EFactionType Faction, UResearchTechDefinition* Tech) const
{
    // Note: Since we remove completed jobs immediately, this will almost always be false for in-progress jobs.
    // If you track completed techs elsewhere (e.g. in a base or player profile), update this function.
    // For now it returns false — you can extend later if needed.
    return false;
}

void UResearchManagerSubsystem::AdvanceDay(EFactionType Faction)
{
    // No longer needed — daily simulation happens inside UStrategyFacility::SimulateDaily()
    // Left here for Blueprint compatibility
}

void UResearchManagerSubsystem::ResetResearch()
{
    UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
    if (!BaseMgr) return;

    // Clear ALL research jobs from every lab
    for (UStrategyBase* Base : BaseMgr->GetBases(EFactionType::Human))
    {
        for (UStrategyFacility* Facility : Base->Facilities)
        {
            if (Facility && Facility->FacilityDefinition && Facility->FacilityDefinition->FacilityType == EFacilityType::Laboratory)
            {
                for (int32 i = Facility->ActiveProductionJobs.Num() - 1; i >= 0; --i)
                {
                    if (Facility->ActiveProductionJobs[i].Type == EProductionType::Research)
                    {
                        Facility->ActiveProductionJobs.RemoveAt(i);
                    }
                }
            }
        }
    }

    OnResearchListChanged.Broadcast(EFactionType::Human);
    OnResearchListChanged.Broadcast(EFactionType::Enemy);

    UE_LOG(LogTemp, Display, TEXT("[RESET] All research jobs cleared from laboratories"));
}