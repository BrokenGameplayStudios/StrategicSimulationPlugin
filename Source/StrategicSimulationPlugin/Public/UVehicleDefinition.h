#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "StrategicSimulationTypes.h"
#include "UVehicleDefinition.generated.h"

/** Primary data asset describing a vehicle type: stats, range, hardpoints, and build cost. */
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

    /** NEW: Maximum travel distance this vehicle type can fly on one mission (in MapLocation units) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Range")
    float MaxRange = 800.0f;

    // === NEW: Radar Range ===
    /** Radar detection radius in map pixels. Used during live scouting missions. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Radar")
    float RadarRangePixels = 64.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
    int32 AttackPower = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
    FResourceStockpile BuildCost;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
    int32 ProductionDays = 8;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle|Hardpoints")
    int32 MaxWeaponSlots = 2;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle|Hardpoints")
    int32 MaxDefenseSlots = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle|Hardpoints")
    TArray<EItemCategory> AllowedWeaponCategories;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Damage & Repair")
    int32 MaxHealth = 100;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Damage & Repair")
    EVehicleDamageState DefaultDamageState = EVehicleDamageState::Undamaged;

    /** Default behavior tendency for this vehicle type (used by AI when deciding how to react to detections) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Behavior")
    EVehicleBehavior DefaultBehavior = EVehicleBehavior::Scouting;
};