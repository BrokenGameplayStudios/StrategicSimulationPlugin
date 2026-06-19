#include "UEngineeringManagerSubsystem.h"
#include "UResourceManagerSubsystem.h"
#include "UBaseManagerSubsystem.h"
#include "UStrategyEventDispatcher.h"
#include "UStrategyBase.h"
#include "UStrategyFacility.h"
#include "Engine/Engine.h"

/** Logs subsystem initialization; production slots are owned by workshop facilities. */
void UEngineeringManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    UE_LOG(LogTemp, Display, TEXT("UEngineeringManagerSubsystem initialized — procurement enabled"));
}

/** Spends ItemDef->PurchaseCost and optionally adds the item to TargetSoldier's loadout. */
bool UEngineeringManagerSubsystem::PurchaseItem(EFactionType Faction, UItemDefinition* ItemDef, UStrategySoldier* TargetSoldier /*= nullptr*/)
{
    if (!ItemDef) return false;

    UResourceManagerSubsystem* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>();
    if (!ResourceMgr) return false;

    const FResourceStockpile& Cost = ItemDef->PurchaseCost;

    if (!ResourceMgr->SubtractResources(Faction, Cost))
    {
        return false;
    }

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

/** Clears workshop item production jobs from all faction bases. */
void UEngineeringManagerSubsystem::ResetProduction()
{
    UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
    if (!BaseMgr) return;

    for (const EFactionType Faction : {EFactionType::Human, EFactionType::Enemy})
    {
        for (UStrategyBase* Base : BaseMgr->GetBases(Faction))
        {
            if (!Base) continue;

            for (UStrategyFacility* Facility : Base->Facilities)
            {
                if (!Facility || !Facility->FacilityDefinition
                    || Facility->FacilityDefinition->FacilityType != EFacilityType::Workshop)
                {
                    continue;
                }

                for (int32 i = Facility->ActiveProductionJobs.Num() - 1; i >= 0; --i)
                {
                    if (Facility->ActiveProductionJobs[i].Type == EProductionType::Item)
                    {
                        Facility->ActiveProductionJobs.RemoveAt(i);
                    }
                }
            }
        }
    }

    UE_LOG(LogTemp, Display, TEXT("[RESET] All workshop item production jobs cleared"));
}

/** Purchases a weapon using full resource costs and equips it to a vehicle. Returns true on success. */
bool UEngineeringManagerSubsystem::PurchaseAndEquipVehicleWeapon(EFactionType Faction, UStrategyVehicle* TargetVehicle, UItemDefinition* WeaponDef)
{
    if (!TargetVehicle || !WeaponDef) return false;

    if (!PurchaseItem(Faction, WeaponDef, nullptr))
    {
        return false;
    }

    if (TargetVehicle->EquipWeapon(WeaponDef))
    {
        UE_LOG(LogTemp, Display, TEXT("[VEHICLE] %s equipped weapon '%s' (fleet effectiveness now higher)"),
            *TargetVehicle->VehicleDefinition->VehicleName.ToString(), *WeaponDef->ItemName.ToString());
        return true;
    }

    return false;
}

/** Buys ammo for a specific equipped weapon on a vehicle and refills it (cheap refill using Metals + Chemicals). */
bool UEngineeringManagerSubsystem::PurchaseAmmoForVehicle(EFactionType Faction, UStrategyVehicle* TargetVehicle, int32 WeaponIndex)
{
    if (!TargetVehicle || !TargetVehicle->EquippedWeapons.IsValidIndex(WeaponIndex))
        return false;

    UItemDefinition* Weapon = TargetVehicle->EquippedWeapons[WeaponIndex].Get();
    if (!Weapon || Weapon->MaxAmmo <= 0)
        return false;

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
        TargetVehicle->WeaponAmmoCounts[WeaponIndex] = Weapon->MaxAmmo;
        UE_LOG(LogTemp, Display, TEXT("[AI] %s refilled ammo for weapon '%s' (cost: %d Money, %d Metals, %d Chemicals)"),
            *UEnum::GetValueAsString(Faction), *Weapon->ItemName.ToString(),
            AmmoCost.Money, AmmoCost.Metals, AmmoCost.Chemicals);
        return true;
    }

    return false;
}