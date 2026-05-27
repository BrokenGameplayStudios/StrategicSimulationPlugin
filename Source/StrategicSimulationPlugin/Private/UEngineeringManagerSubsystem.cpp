#include "UEngineeringManagerSubsystem.h"
#include "UResourceManagerSubsystem.h"
#include "UStrategyCampaignSubsystem.h"
#include "UBaseManagerSubsystem.h"
#include "USoldierManagerSubsystem.h"
#include "UStrategyEventDispatcher.h"
#include "UItemDatabase.h"
#include "UActiveProductionJob.h"
#include "UStrategyBase.h"
#include "Engine/Engine.h"

void UEngineeringManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    if (UTimeManagerSubsystem* TimeMgr = GetGameInstance()->GetSubsystem<UTimeManagerSubsystem>())
    {
        TimeMgr->OnDayPassed.AddDynamic(this, &UEngineeringManagerSubsystem::OnDayPassed);
    }

    UE_LOG(LogTemp, Display, TEXT("UEngineeringManagerSubsystem initialized — production slots + queuing enabled"));
}

bool UEngineeringManagerSubsystem::PurchaseItem(EFactionType Faction, UItemDefinition* ItemDef, UStrategySoldier* TargetSoldier)
{
    if (!ItemDef) return false;

    UResourceManagerSubsystem* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>();
    if (!ResourceMgr) return false;

    FResourceStockpile Cost = ItemDef->PurchaseCost;
    FResourceStockpile Current = ResourceMgr->GetResources(Faction);

    if (Current.Money < Cost.Money || Current.Supplies < Cost.Supplies || Current.ExoticMaterial < Cost.ExoticMaterial)
    {
        return false;
    }

    ResourceMgr->AddResources(Faction, { -Cost.Money, -Cost.Supplies, -Cost.ExoticMaterial, -Cost.ResearchPoints });

    if (TargetSoldier)
    {
        TargetSoldier->CurrentLoadout.Add(ItemDef);
    }

    if (UStrategyEventDispatcher* EventDisp = GetGameInstance()->GetSubsystem<UStrategyEventDispatcher>())
        EventDisp->OnSoldierLoadoutChanged.Broadcast(Faction, TargetSoldier);

    return true;
}

// FULL PRODUCTION WITH SLOTS + QUEUE
UActiveProductionJob* UEngineeringManagerSubsystem::StartProduction(EFactionType Faction, UItemDefinition* ItemDef, int32 Quantity, UStrategyBase* TargetBase)
{
    if (!ItemDef) return nullptr;

    UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
    if (!BaseMgr) return nullptr;

    UStrategyBase* ChosenBase = TargetBase;
    if (!ChosenBase)
    {
        const TArray<UStrategyBase*>& Bases = BaseMgr->GetBases(Faction);
        if (!Bases.IsEmpty()) ChosenBase = Bases[0];
    }
    if (!ChosenBase) return nullptr;

    // Count currently active jobs on this base
    int32 CurrentActive = 0;
    TArray<UActiveProductionJob*>& Queue = (Faction == EFactionType::Human) ? HumanProductionQueue : EnemyProductionQueue;

    for (UActiveProductionJob* Job : Queue)
    {
        if (Job && Job->OwningBase == ChosenBase && !Job->bIsCompleted) CurrentActive++;
    }

    UActiveProductionJob* NewJob = NewObject<UActiveProductionJob>();
    NewJob->ItemToProduce = ItemDef;
    NewJob->Quantity = Quantity;
    NewJob->RemainingDays = ItemDef->ProductionDays;
    NewJob->OwningBase = ChosenBase;

    if (CurrentActive >= ChosenBase->GetTotalProductionSlots())
    {
        // Add to queue (waiting)
        NewJob->bIsQueued = true;
        UE_LOG(LogTemp, Display, TEXT("[PRODUCTION] %s — No free slots in base '%s' (queued)"),
            *UEnum::GetValueAsString(Faction), *ChosenBase->BaseName.ToString());
    }
    else
    {
        NewJob->bIsQueued = false;
    }

    Queue.Add(NewJob);

    UE_LOG(LogTemp, Display, TEXT("[PRODUCTION] %s started %s x%d in base '%s' (%s)"),
        *UEnum::GetValueAsString(Faction), *ItemDef->ItemName.ToString(), Quantity, *ChosenBase->BaseName.ToString(),
        NewJob->bIsQueued ? TEXT("queued") : TEXT("active"));

    return NewJob;
}

TArray<UActiveProductionJob*> UEngineeringManagerSubsystem::GetActiveProduction(EFactionType Faction) const
{
    return (Faction == EFactionType::Human) ? HumanProductionQueue : EnemyProductionQueue;
}

bool UEngineeringManagerSubsystem::TryProduce(EFactionType Faction)
{
    // Simple AI production call - can be expanded later
    return false;
}

void UEngineeringManagerSubsystem::OnDayPassed(int32 NewDay)
{
    TArray<UActiveProductionJob*>& HumanQueue = HumanProductionQueue;
    TArray<UActiveProductionJob*>& EnemyQueue = EnemyProductionQueue;

    // Process Human jobs
    for (UActiveProductionJob* Job : HumanQueue)
    {
        if (Job && !Job->bIsCompleted)
        {
            if (Job->bIsQueued)
            {
                // Try to start queued job if slot is now free
                int32 ActiveCount = 0;
                for (UActiveProductionJob* J : HumanQueue)
                {
                    if (J && J->OwningBase == Job->OwningBase && !J->bIsCompleted && !J->bIsQueued) ActiveCount++;
                }
                if (ActiveCount < Job->OwningBase->GetTotalProductionSlots())
                {
                    Job->bIsQueued = false;
                    UE_LOG(LogTemp, Display, TEXT("[PRODUCTION] Queued job started in base '%s'"), *Job->OwningBase->BaseName.ToString());
                }
            }
            else
            {
                Job->RemainingDays--;
                if (Job->RemainingDays <= 0)
                {
                    Job->bIsCompleted = true;
                    UE_LOG(LogTemp, Display, TEXT("[PRODUCTION] Human completed: %s x%d in base '%s'"),
                        *Job->ItemToProduce->ItemName.ToString(), Job->Quantity,
                        Job->OwningBase ? *Job->OwningBase->BaseName.ToString() : TEXT("Unknown"));
                }
            }
        }
    }

    // Process Enemy jobs (identical logic)
    for (UActiveProductionJob* Job : EnemyQueue)
    {
        if (Job && !Job->bIsCompleted)
        {
            if (Job->bIsQueued)
            {
                int32 ActiveCount = 0;
                for (UActiveProductionJob* J : EnemyQueue)
                {
                    if (J && J->OwningBase == Job->OwningBase && !J->bIsCompleted && !J->bIsQueued) ActiveCount++;
                }
                if (ActiveCount < Job->OwningBase->GetTotalProductionSlots())
                {
                    Job->bIsQueued = false;
                    UE_LOG(LogTemp, Display, TEXT("[PRODUCTION] Queued job started in base '%s'"), *Job->OwningBase->BaseName.ToString());
                }
            }
            else
            {
                Job->RemainingDays--;
                if (Job->RemainingDays <= 0)
                {
                    Job->bIsCompleted = true;
                    UE_LOG(LogTemp, Display, TEXT("[PRODUCTION] Enemy completed: %s x%d in base '%s'"),
                        *Job->ItemToProduce->ItemName.ToString(), Job->Quantity,
                        Job->OwningBase ? *Job->OwningBase->BaseName.ToString() : TEXT("Unknown"));
                }
            }
        }
    }
}

// NEW FUNCTION
void UEngineeringManagerSubsystem::ResetProduction()
{
    for (UActiveProductionJob* Job : HumanProductionQueue)
    {
        if (Job) Job->ConditionalBeginDestroy();
    }
    HumanProductionQueue.Empty();

    for (UActiveProductionJob* Job : EnemyProductionQueue)
    {
        if (Job) Job->ConditionalBeginDestroy();
    }
    EnemyProductionQueue.Empty();

    UE_LOG(LogTemp, Display, TEXT("[RESET] All production jobs cleared for both factions"));
}