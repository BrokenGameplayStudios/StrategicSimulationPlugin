#include "UResearchManagerSubsystem.h"
#include "UStrategyEventDispatcher.h"
#include "Engine/Engine.h"

void UResearchManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    if (UTimeManagerSubsystem* TimeMgr = GetGameInstance()->GetSubsystem<UTimeManagerSubsystem>())
    {
        TimeMgr->OnDayPassed.AddDynamic(this, &UResearchManagerSubsystem::OnDayPassed);
    }

    UE_LOG(LogTemp, Display, TEXT("UResearchManagerSubsystem initialized"));
}

UActiveResearchProject* UResearchManagerSubsystem::StartResearch(EFactionType Faction, UResearchTechDefinition* ProjectDef)
{
    if (!ProjectDef) return nullptr;

    UActiveResearchProject* NewProject = NewObject<UActiveResearchProject>();
    NewProject->ResearchDefinition = ProjectDef;
    NewProject->RemainingDays = ProjectDef->ResearchDays;

    if (Faction == EFactionType::Human)
        HumanResearchQueue.Add(NewProject);
    else
        EnemyResearchQueue.Add(NewProject);

    OnResearchListChanged.Broadcast(Faction);
    UE_LOG(LogTemp, Display, TEXT("Started research '%s' for %s (%d days)"), *ProjectDef->ProjectName.ToString(), *UEnum::GetValueAsString(Faction), NewProject->RemainingDays);

    return NewProject;
}

TArray<UActiveResearchProject*> UResearchManagerSubsystem::GetActiveResearch(EFactionType Faction) const
{
    return (Faction == EFactionType::Human) ? HumanResearchQueue : EnemyResearchQueue;
}

void UResearchManagerSubsystem::OnDayPassed(int32 NewDay)
{
    for (UActiveResearchProject* Proj : HumanResearchQueue)
    {
        if (Proj && !Proj->bIsCompleted)
        {
            Proj->RemainingDays--;
            if (Proj->RemainingDays <= 0)
            {
                Proj->bIsCompleted = true;
                if (UStrategyEventDispatcher* EventDisp = GetGameInstance()->GetSubsystem<UStrategyEventDispatcher>())
                    EventDisp->OnResearchCompleted.Broadcast(EFactionType::Human, Proj->ResearchDefinition);
                UE_LOG(LogTemp, Display, TEXT("[RESEARCH] Human completed: %s"), *Proj->ResearchDefinition->ProjectName.ToString());
            }
        }
    }

    for (UActiveResearchProject* Proj : EnemyResearchQueue)
    {
        if (Proj && !Proj->bIsCompleted)
        {
            Proj->RemainingDays--;
            if (Proj->RemainingDays <= 0)
            {
                Proj->bIsCompleted = true;
                if (UStrategyEventDispatcher* EventDisp = GetGameInstance()->GetSubsystem<UStrategyEventDispatcher>())
                    EventDisp->OnResearchCompleted.Broadcast(EFactionType::Enemy, Proj->ResearchDefinition);
                UE_LOG(LogTemp, Display, TEXT("[RESEARCH] Enemy completed: %s"), *Proj->ResearchDefinition->ProjectName.ToString());
            }
        }
    }
}

bool UResearchManagerSubsystem::IsResearchInProgress(EFactionType Faction, UResearchTechDefinition* Tech) const
{
    if (!Tech) return false;

    const TArray<UActiveResearchProject*>& Queue = (Faction == EFactionType::Human) ? HumanResearchQueue : EnemyResearchQueue;
    for (UActiveResearchProject* Proj : Queue)
    {
        if (Proj && Proj->ResearchDefinition == Tech && !Proj->bIsCompleted)
            return true;
    }
    return false;
}

bool UResearchManagerSubsystem::HasCompletedResearch(EFactionType Faction, UResearchTechDefinition* Tech) const
{
    if (!Tech) return false;

    const TArray<UActiveResearchProject*>& Queue = (Faction == EFactionType::Human) ? HumanResearchQueue : EnemyResearchQueue;
    for (UActiveResearchProject* Proj : Queue)
    {
        if (Proj && Proj->ResearchDefinition == Tech && Proj->bIsCompleted)
            return true;
    }
    return false;
}

void UResearchManagerSubsystem::AdvanceDay(EFactionType Faction)
{
    TArray<UActiveResearchProject*>& Queue = (Faction == EFactionType::Human) ? HumanResearchQueue : EnemyResearchQueue;

    for (UActiveResearchProject* Proj : Queue)
    {
        if (Proj && !Proj->bIsCompleted)
        {
            Proj->RemainingDays--;
            UE_LOG(LogTemp, Verbose, TEXT("[RESEARCH] %s %s progress: %d days left"),
                *UEnum::GetValueAsString(Faction), *Proj->ResearchDefinition->ProjectName.ToString(), Proj->RemainingDays);

            if (Proj->RemainingDays <= 0)
            {
                Proj->bIsCompleted = true;
                UE_LOG(LogTemp, Display, TEXT("[RESEARCH] ✅ %s completed research: %s"),
                    *UEnum::GetValueAsString(Faction), *Proj->ResearchDefinition->ProjectName.ToString());
            }
        }
    }
}