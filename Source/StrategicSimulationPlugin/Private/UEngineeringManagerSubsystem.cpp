#include "UEngineeringManagerSubsystem.h"
#include "UResourceManagerSubsystem.h"
#include "UStrategyCampaignSubsystem.h"
#include "UBaseManagerSubsystem.h"
#include "USoldierManagerSubsystem.h"
#include "UStrategyEventDispatcher.h"
#include "UItemDatabase.h"
#include "UActiveProductionJob.h"
#include "UStrategyBase.h"
#include "UStrategyFacility.h"
#include "Engine/Engine.h"

void UEngineeringManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

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

    // Unified system — only facilities control slots now
    for (UStrategyFacility* Workshop : ChosenBase->Facilities)
    {
        if (Workshop && Workshop->FacilityDefinition && Workshop->FacilityDefinition->FacilityType == EFacilityType::Workshop)
        {
            if (Workshop->HasFreeProductionSlot())
            {
                if (Workshop->StartProduction(EProductionType::Item, ItemDef, ItemDef->ProductionDays))
                {
                    UE_LOG(LogTemp, Display, TEXT("[PRODUCTION] %s started %s x%d in base '%s'"),
                        *UEnum::GetValueAsString(Faction), *ItemDef->ItemName.ToString(), Quantity, *ChosenBase->BaseName.ToString());
                    return nullptr;  // UI should call GetActiveProduction() after start
                }
            }
        }
    }
    return nullptr;
}

TArray<UActiveProductionJob*> UEngineeringManagerSubsystem::GetActiveProduction(EFactionType Faction) const
{
    TArray<UActiveProductionJob*> Result;
    UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
    if (!BaseMgr) return Result;

    const TArray<UStrategyBase*>& Bases = BaseMgr->GetBases(Faction);
    for (UStrategyBase* Base : Bases)
    {
        for (UStrategyFacility* Facility : Base->Facilities)
        {
            if (Facility && Facility->FacilityDefinition && Facility->FacilityDefinition->FacilityType == EFacilityType::Workshop)
            {
                for (const FProductionJob& Job : Facility->ActiveProductionJobs)
                {
                    if (Job.Type == EProductionType::Item && Job.TargetAsset)
                    {
                        UActiveProductionJob* ActiveJob = NewObject<UActiveProductionJob>(GetTransientPackage());
                        ActiveJob->ItemToProduce = Cast<UItemDefinition>(Job.TargetAsset);
                        ActiveJob->Quantity = 1;           // extend FProductionJob later if you need batches
                        ActiveJob->RemainingDays = Job.RemainingDays;
                        ActiveJob->bIsCompleted = false;
                        ActiveJob->bIsQueued = false;
                        ActiveJob->OwningBase = Base;
                        Result.Add(ActiveJob);
                    }
                }
            }
        }
    }
    return Result;
}

bool UEngineeringManagerSubsystem::TryProduce(EFactionType Faction)
{
    return false; // Now handled by facility queue
}

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