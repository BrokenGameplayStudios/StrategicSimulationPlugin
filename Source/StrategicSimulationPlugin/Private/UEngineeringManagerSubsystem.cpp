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

bool UEngineeringManagerSubsystem::PurchaseItem(EFactionType Faction, UItemDefinition* ItemDef, UStrategySoldier* TargetSoldier /*= nullptr*/)
{
    if (!ItemDef) return false;

    UResourceManagerSubsystem* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>();
    if (!ResourceMgr) return false;

    // Use the full PurchaseCost from the ItemDefinition (now supports Metals, Biologicals, Chemicals, etc.)
    const FResourceStockpile& Cost = ItemDef->PurchaseCost;

    if (!ResourceMgr->SubtractResources(Faction, Cost))
    {
        return false; // logging already happens inside SubtractResources
    }

    // Equip to soldier (unchanged behavior)
    if (TargetSoldier)
    {
        TargetSoldier->CurrentLoadout.Add(ItemDef);
        UE_LOG(LogTemp, Display, TEXT("[PURCHASE] %s bought %s for soldier %s"),
            *UEnum::GetValueAsString(Faction), *ItemDef->ItemName.ToString(), *TargetSoldier->SoldierName);
    }

    if (UStrategyEventDispatcher* EventDisp = GetGameInstance()->GetSubsystem<UStrategyEventDispatcher>())
    {
        EventDisp->OnSoldierLoadoutChanged.Broadcast(Faction, TargetSoldier);
    }

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

bool UEngineeringManagerSubsystem::PurchaseAndEquipVehicleWeapon(EFactionType Faction, UStrategyVehicle* TargetVehicle, UItemDefinition* WeaponDef)
{
    if (!TargetVehicle || !WeaponDef) return false;

    // Purchase first (uses full resource cost)
    if (!PurchaseItem(Faction, WeaponDef, nullptr))
    {
        return false;
    }

    // Then equip to the vehicle
    if (TargetVehicle->EquipWeapon(WeaponDef))
    {
        UE_LOG(LogTemp, Display, TEXT("[VEHICLE] %s equipped weapon '%s' (fleet effectiveness now higher)"),
            *TargetVehicle->VehicleDefinition->VehicleName.ToString(), *WeaponDef->ItemName.ToString());
        return true;
    }

    return false;
}

bool UEngineeringManagerSubsystem::PurchaseAmmoForVehicle(EFactionType Faction, UStrategyVehicle* TargetVehicle, int32 WeaponIndex)
{
    if (!TargetVehicle || !TargetVehicle->EquippedWeapons.IsValidIndex(WeaponIndex))
        return false;

    UItemDefinition* Weapon = TargetVehicle->EquippedWeapons[WeaponIndex].Get();
    if (!Weapon || Weapon->MaxAmmo <= 0)
        return false;

    // Simple ammo cost: 20% of the weapon's purchase cost, biased toward Metals + Chemicals
    FResourceStockpile AmmoCost;
    AmmoCost.Money = Weapon->PurchaseCost.Money / 5;
    AmmoCost.Metals = Weapon->PurchaseCost.Metals / 2;
    AmmoCost.Chemicals = Weapon->PurchaseCost.Chemicals / 2;

    UResourceManagerSubsystem* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>();
    if (!ResourceMgr || !ResourceMgr->CanAfford(Faction, AmmoCost))
    {
        UE_LOG(LogTemp, Verbose, TEXT("[AI] Cannot afford ammo refill for %s"), *Weapon->ItemName.ToString());
        return false;
    }

    if (ResourceMgr->SubtractResources(Faction, AmmoCost))
    {
        TargetVehicle->WeaponAmmoCounts[WeaponIndex] = Weapon->MaxAmmo; // full refill
        UE_LOG(LogTemp, Display, TEXT("[AI] %s refilled ammo for weapon '%s' (cost: %d Money, %d Metals, %d Chemicals)"),
            *UEnum::GetValueAsString(Faction), *Weapon->ItemName.ToString(),
            AmmoCost.Money, AmmoCost.Metals, AmmoCost.Chemicals);
        return true;
    }

    return false;
}