#include "UResearchManagerSubsystem.h"
#include "UStrategyEventDispatcher.h"
#include "Engine/Engine.h"
#include "UBaseManagerSubsystem.h"
#include "UStrategyFacility.h"
#include "UStrategyBase.h"

/** Logs subsystem initialization; research state lives on facility production queues. */
void UResearchManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    UE_LOG(LogTemp, Display, TEXT("UResearchManagerSubsystem initialized"));
}

/** Finds a free lab slot for Faction and starts ProjectDef; broadcasts OnResearchListChanged on success. */
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

/** Builds transient UActiveResearchProject snapshots from all laboratory jobs for Faction. */
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

/** Returns true when Tech is currently queued in any laboratory for Faction. */
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

/** Returns whether Tech has been completed by Faction (stub: always true until per-faction tracking exists). */
bool UResearchManagerSubsystem::HasCompletedResearch(EFactionType Faction, UResearchTechDefinition* Tech) const
{
    // TODO: Later we can track completed techs per faction in a TArray/TSet.
    // For now we return true so the AI can actually buy upgraded gear (Knife + Healthpack was the only thing that worked before).
    return true;
}

/** Clears all research jobs from Human laboratories and notifies both factions. */
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

/** Starts the next available research for this faction if a lab slot is free. */
bool UResearchManagerSubsystem::TryResearch(EFactionType Faction)
{
    // Guard: never start a second research while one is already running
    if (GetActiveResearch(Faction).Num() > 0)
    {
        UE_LOG(LogTemp, Verbose, TEXT("[RESEARCH] %s already has active research — skipping"), *UEnum::GetValueAsString(Faction));
        return false;
    }

    // Use the database we just added to the header
    for (UResearchTechDefinition* Tech : ResearchDatabase)
    {
        if (!Tech) continue;

        // Skip if already completed
        if (HasCompletedResearch(Faction, Tech))
            continue;

        // Try to start it (this calls your existing StartResearch logic)
        if (StartResearch(Faction, Tech))
        {
            UE_LOG(LogTemp, Display, TEXT("[RESEARCH] %s started research: %s"),
                *UEnum::GetValueAsString(Faction), *Tech->ProjectName.ToString());
            return true;
        }
    }

    UE_LOG(LogTemp, Verbose, TEXT("[RESEARCH] %s — No new research available or no free lab slots"), *UEnum::GetValueAsString(Faction));
    return false;
}