#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "StrategicSimulationTypes.h"
#include "UVehicleDefinition.generated.h"

/**
 * Vehicle hull type: crew, combat, range, hardpoints, and hangar build cost.
 *
 * Designer order: Identity → Crew → Combat → Range & Radar → Durability → Hardpoints → Build → AI.
 */
UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UVehicleDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // === Identity ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    FText VehicleName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity",
        meta = (ToolTip = "Scout, Gunship, Transport, etc. — drives AI mission preferences and combat rules."))
    EVehicleType VehicleType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity", meta = (MultiLine = true))
    FText Description;

    // === Crew ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew",
        meta = (ClampMin = "1", ToolTip = "Passengers assignable to missions from this vehicle."))
    int32 SoldierCapacity = 8;

    // === Combat ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat",
        meta = (ClampMin = "0", ToolTip = "Base hull offensive rating before weapon modules."))
    int32 AttackPower = 0;

    // === Range & radar ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Range & Radar",
        meta = (ClampMin = "50.0", ToolTip = "Maximum round-trip travel distance on one mission (map pixels)."))
    float MaxRange = 800.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Range & Radar",
        meta = (ClampMin = "16.0", ToolTip = "Passive/active radar sweep radius during live missions (map pixels)."))
    float RadarRangePixels = 64.0f;

    // === Durability ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Durability",
        meta = (ClampMin = "1", ToolTip = "Maximum hull hit points."))
    int32 MaxHealth = 100;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Durability",
        meta = (ToolTip = "Starting damage band when spawned from the hangar."))
    EVehicleDamageState DefaultDamageState = EVehicleDamageState::Undamaged;

    // === Hardpoints ===
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hardpoints",
        meta = (ClampMin = "0", ToolTip = "Weapon modules that can be equipped after production."))
    int32 MaxWeaponSlots = 2;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hardpoints",
        meta = (ClampMin = "0", ToolTip = "Defense modules that can be equipped after production."))
    int32 MaxDefenseSlots = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hardpoints",
        meta = (ToolTip = "Item categories accepted in weapon hardpoints (empty = any Vehicle Weapon)."))
    TArray<EItemCategory> AllowedWeaponCategories;

    // === Build ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build|Cost",
        meta = (ToolTip = "Resources spent when hangar production starts."))
    FResourceStockpile BuildCost;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build|Timing",
        meta = (ClampMin = "1", ToolTip = "Days to build in a Hangar production slot."))
    int32 ProductionDays = 8;

    // === AI ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Behavior",
        meta = (ToolTip = "Default tactical response when detecting threats during live movement."))
    EVehicleBehavior DefaultBehavior = EVehicleBehavior::Scouting;
};