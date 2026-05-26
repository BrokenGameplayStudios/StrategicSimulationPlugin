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

    UE_LOG(LogTemp, Display, TEXT("UEngineeringManagerSubsystem initialized"));
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

    // Enforce per-base ProductionSlots limit
    int32 CurrentJobs = 0;
    for (UActiveProductionJob* Job : (Faction == EFactionType::Human ? HumanProductionQueue : EnemyProductionQueue))
    {
        if (Job && Job->OwningBase == ChosenBase) CurrentJobs++;
    }

    if (CurrentJobs >= ChosenBase->GetTotalProductionSlots())
    {
        UE_LOG(LogTemp, Display, TEXT("[PRODUCTION] No free slots in base '%s' (%d/%d)"), *ChosenBase->BaseName.ToString(), CurrentJobs, ChosenBase->GetTotalProductionSlots());
        return nullptr;
    }

    UActiveProductionJob* NewJob = NewObject<UActiveProductionJob>();
    NewJob->ItemToProduce = ItemDef;
    NewJob->Quantity = Quantity;
    NewJob->RemainingDays = ItemDef->ProductionDays;
    NewJob->OwningBase = ChosenBase;

    if (Faction == EFactionType::Human)
        HumanProductionQueue.Add(NewJob);
    else
        EnemyProductionQueue.Add(NewJob);

    UE_LOG(LogTemp, Display, TEXT("[PRODUCTION] Started %s x%d in base '%s' (%d days)"),
        *ItemDef->ItemName.ToString(), Quantity, *ChosenBase->BaseName.ToString(), NewJob->RemainingDays);

    return NewJob;
}

TArray<UActiveProductionJob*> UEngineeringManagerSubsystem::GetActiveProduction(EFactionType Faction) const
{
    return (Faction == EFactionType::Human) ? HumanProductionQueue : EnemyProductionQueue;
}

bool UEngineeringManagerSubsystem::TryProduce(EFactionType Faction)
{
    // Simple AI production call - you can expand this later
    return false;
}

void UEngineeringManagerSubsystem::OnDayPassed(int32 NewDay)
{
    // Progress Human jobs
    for (UActiveProductionJob* Job : HumanProductionQueue)
    {
        if (Job && !Job->bIsCompleted)
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

    // Progress Enemy jobs
    for (UActiveProductionJob* Job : EnemyProductionQueue)
    {
        if (Job && !Job->bIsCompleted)
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