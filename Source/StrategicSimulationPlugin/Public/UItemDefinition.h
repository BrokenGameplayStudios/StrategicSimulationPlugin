#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "StrategicSimulationTypes.h"
#include "UStrategyTechDefinition.h"
#include "UItemDefinition.generated.h"

/** Primary data asset for equippable and producible items (soldier gear, vehicle hardpoints, consumables). */
UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UItemDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
    FText ItemName;

    /** Primary category of this item — determines usage (soldier vs vehicle), simulation bonuses, and research grouping */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
    EItemCategory Category = EItemCategory::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cost")
    FResourceStockpile PurchaseCost;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    int32 Damage = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    int32 ArmorBonus = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    int32 AimBonus = 0;

    /** Vehicle-only bonuses */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle Stats")
    int32 VehicleDamageBonus = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle Stats")
    int32 VehicleDefenseBonus = 0;

    /** Ammo system stub (0 = infinite for now) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle Stats")
    int32 MaxAmmo = 0;

    /**
     * Returns whether this item is categorized as a vehicle weapon hardpoint module.
     * @return True when Category is VehicleWeapon.
     */
    UFUNCTION(BlueprintCallable, Category = "Item")
    bool IsVehicleWeapon() const { return Category == EItemCategory::VehicleWeapon; }

    /**
     * Returns whether this item is categorized as a vehicle defense system module.
     * @return True when Category is VehicleDefense.
     */
    UFUNCTION(BlueprintCallable, Category = "Item")
    bool IsVehicleDefense() const { return Category == EItemCategory::VehicleDefense; }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    bool bIsConsumable = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    int32 QuantityPerStack = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Production")
    int32 ProductionDays = 5;
};