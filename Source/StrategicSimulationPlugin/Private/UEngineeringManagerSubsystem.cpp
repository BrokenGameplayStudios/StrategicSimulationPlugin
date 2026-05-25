#include "UEngineeringManagerSubsystem.h"
#include "UResourceManagerSubsystem.h"
#include "USoldierManagerSubsystem.h"
#include "UStrategyEventDispatcher.h"
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
    UE_LOG(LogTemp, Display, TEXT("[PURCHASE] === %s attempting to buy item ==="), *UEnum::GetValueAsString(Faction));

    if (!ItemDef)
    {
        UE_LOG(LogTemp, Error, TEXT("[PURCHASE] FAILED — ItemDef is nullptr!"));
        return false;
    }

    UResourceManagerSubsystem* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>();
    if (!ResourceMgr)
    {
        UE_LOG(LogTemp, Error, TEXT("[PURCHASE] FAILED — ResourceManager not found!"));
        return false;
    }

    FResourceStockpile Cost = ItemDef->PurchaseCost;
    FResourceStockpile Current = ResourceMgr->GetResources(Faction);

    UE_LOG(LogTemp, Display, TEXT("[PURCHASE] %s wants %s | Cost: Money=%d Supplies=%d | Has: Money=%d Supplies=%d"),
        *UEnum::GetValueAsString(Faction), *ItemDef->ItemName.ToString(),
        Cost.Money, Cost.Supplies, Current.Money, Current.Supplies);

    if (Current.Money < Cost.Money || Current.Supplies < Cost.Supplies || Current.ExoticMaterial < Cost.ExoticMaterial)
    {
        UE_LOG(LogTemp, Warning, TEXT("[PURCHASE] FAILED — Not enough resources for %s"), *ItemDef->ItemName.ToString());
        return false;
    }

    // Spend resources
    ResourceMgr->AddResources(Faction, { -Cost.Money, -Cost.Supplies, -Cost.ExoticMaterial, -Cost.ResearchPoints });

    if (TargetSoldier)
    {
        TargetSoldier->CurrentLoadout.Add(ItemDef);
        UE_LOG(LogTemp, Display, TEXT("[PURCHASE] ✅ SUCCESS — %s bought and equipped %s on %s"),
            *UEnum::GetValueAsString(Faction), *ItemDef->ItemName.ToString(), *TargetSoldier->SoldierName);
    }
    else
    {
        UE_LOG(LogTemp, Display, TEXT("[PURCHASE] ✅ SUCCESS — %s bought %s (stockpile)"),
            *UEnum::GetValueAsString(Faction), *ItemDef->ItemName.ToString());
    }

    if (UStrategyEventDispatcher* EventDisp = GetGameInstance()->GetSubsystem<UStrategyEventDispatcher>())
        EventDisp->OnProductionCompleted.Broadcast(Faction, ItemDef);

    return true;
}

UActiveProductionJob* UEngineeringManagerSubsystem::StartProduction(EFactionType Faction, UItemDefinition* ItemDef, int32 Quantity)
{
    if (!ItemDef) return nullptr;

    UActiveProductionJob* NewJob = NewObject<UActiveProductionJob>();
    NewJob->ItemToProduce = ItemDef;
    NewJob->Quantity = Quantity;
    NewJob->RemainingDays = 3;

    if (Faction == EFactionType::Human)
        HumanProductionQueue.Add(NewJob);
    else
        EnemyProductionQueue.Add(NewJob);

    UE_LOG(LogTemp, Display, TEXT("Started production of %s x%d for %s"), *ItemDef->ItemName.ToString(), Quantity, *UEnum::GetValueAsString(Faction));

    return NewJob;
}

TArray<UActiveProductionJob*> UEngineeringManagerSubsystem::GetActiveProduction(EFactionType Faction) const
{
    return (Faction == EFactionType::Human) ? HumanProductionQueue : EnemyProductionQueue;
}

void UEngineeringManagerSubsystem::OnDayPassed(int32 NewDay)
{
    // Progress production queue
    for (UActiveProductionJob* Job : HumanProductionQueue)
    {
        if (Job && !Job->bIsCompleted)
        {
            Job->RemainingDays--;
            if (Job->RemainingDays <= 0)
            {
                Job->bIsCompleted = true;
                UE_LOG(LogTemp, Display, TEXT("[PRODUCTION] Human completed: %s x%d"), *Job->ItemToProduce->ItemName.ToString(), Job->Quantity);
            }
        }
    }

    for (UActiveProductionJob* Job : EnemyProductionQueue)
    {
        if (Job && !Job->bIsCompleted)
        {
            Job->RemainingDays--;
            if (Job->RemainingDays <= 0)
            {
                Job->bIsCompleted = true;
                UE_LOG(LogTemp, Display, TEXT("[PRODUCTION] Enemy completed: %s x%d"), *Job->ItemToProduce->ItemName.ToString(), Job->Quantity);
            }
        }
    }
}