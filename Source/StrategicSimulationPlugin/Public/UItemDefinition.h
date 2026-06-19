#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "StrategicSimulationTypes.h"
#include "UItemDefinition.generated.h"

/**
 * Equippable or producible item (soldier gear, vehicle hardpoints, consumables).
 *
 * Designer order: Identity → Economy → Soldier combat → Vehicle combat → Consumable → Production.
 */
UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UItemDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // === Identity ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    FText ItemName;

    /** Determines soldier vs vehicle usage, simulation bonuses, and research grouping. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity",
        meta = (ToolTip = "Soldier Weapon/Armor for infantry loadouts; Vehicle Weapon/Defense for hardpoints; Consumable for ammo/medkits."))
    EItemCategory Category = EItemCategory::None;

    // === Economy ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy|Cost")
    FResourceStockpile PurchaseCost;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy|Production",
        meta = (ClampMin = "1", ToolTip = "Days to fabricate in a Workshop when queued for production."))
    int32 ProductionDays = 5;

    // === Soldier combat ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Soldier",
        meta = (EditCondition = "Category == EItemCategory::SoldierWeapon || Category == EItemCategory::Melee || Category == EItemCategory::Ballistic || Category == EItemCategory::Explosive || Category == EItemCategory::Energy", EditConditionHides))
    int32 Damage = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Soldier",
        meta = (EditCondition = "Category == EItemCategory::SoldierArmor", EditConditionHides))
    int32 ArmorBonus = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Soldier",
        meta = (EditCondition = "Category == EItemCategory::SoldierWeapon || Category == EItemCategory::Melee || Category == EItemCategory::Ballistic || Category == EItemCategory::Explosive || Category == EItemCategory::Energy", EditConditionHides))
    int32 AimBonus = 0;

    // === Vehicle combat ===
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Vehicle",
        meta = (EditCondition = "Category == EItemCategory::VehicleWeapon", EditConditionHides))
    int32 VehicleDamageBonus = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Vehicle",
        meta = (EditCondition = "Category == EItemCategory::VehicleDefense", EditConditionHides))
    int32 VehicleDefenseBonus = 0;

    /** Ammo capacity for vehicle weapons (0 = infinite / not tracked). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Vehicle",
        meta = (EditCondition = "Category == EItemCategory::VehicleWeapon || Category == EItemCategory::Consumable", EditConditionHides, ClampMin = "0"))
    int32 MaxAmmo = 0;

    // === Consumable ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consumable",
        meta = (EditCondition = "Category == EItemCategory::Consumable || Category == EItemCategory::Medical", EditConditionHides))
    bool bIsConsumable = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consumable",
        meta = (EditCondition = "bIsConsumable", EditConditionHides, ClampMin = "1"))
    int32 QuantityPerStack = 1;

    UFUNCTION(BlueprintCallable, Category = "Item")
    bool IsVehicleWeapon() const { return Category == EItemCategory::VehicleWeapon; }

    UFUNCTION(BlueprintCallable, Category = "Item")
    bool IsVehicleDefense() const { return Category == EItemCategory::VehicleDefense; }
};