#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "StrategicSimulationTypes.h"
#include "UVehicleDefinition.generated.h"

UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UVehicleDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
    FText VehicleName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
    EVehicleType VehicleType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
    int32 SoldierCapacity = 8;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
    int32 MaxMissionDurationDays = 30;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
    int32 AttackPower = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
    FResourceStockpile BuildCost;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
    int32 ProductionDays = 8;

    /** === NEW: Hardpoint System (Phase 6.2) ===
 *  Defines how many weapons/defense systems this vehicle type can carry.
 *  e.g. Transport = 1 weapon slot, Fighter Jet = 4+ weapon slots
 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle|Hardpoints")
    int32 MaxWeaponSlots = 2;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle|Hardpoints")
    int32 MaxDefenseSlots = 1;

    /** Optional future filtering (e.g. only allow missiles on certain vehicles) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle|Hardpoints")
    TArray<EItemCategory> AllowedWeaponCategories;

    // === NEW: Vehicle Damage & Repair System (Phase 3.5) ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Damage & Repair")
    int32 MaxHealth = 100;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Damage & Repair")
    EVehicleDamageState DefaultDamageState = EVehicleDamageState::Undamaged;
};