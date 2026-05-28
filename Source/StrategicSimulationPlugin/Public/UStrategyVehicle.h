#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UVehicleDefinition.h"
#include "StrategicSimulationTypes.h"
#include "UStrategyVehicle.generated.h"

class UStrategyBase;
class UStrategyFacility;
class UMissionGroup;

UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UStrategyVehicle : public UObject
{
    GENERATED_BODY()

public:
    UStrategyVehicle();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
    UVehicleDefinition* VehicleDefinition;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
    UStrategyBase* HomeBase;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
    UStrategyFacility* CurrentHanger;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
    UMissionGroup* CurrentMission;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
    TArray<class UStrategySoldier*> CurrentPassengers;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
    int32 RemainingFuelDays;

    // === NEW: Vehicle Damage & Repair System ===
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Damage & Repair")
    EVehicleDamageState DamageState = EVehicleDamageState::Undamaged;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Damage & Repair")
    int32 CurrentHealth = 100;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Damage & Repair")
    UStrategyFacility* CurrentRepairBay = nullptr;   // When checked out for repair

    /** Returns true if the vehicle is not on mission and not in repair */
    UFUNCTION(BlueprintCallable, Category = "Vehicle")
    bool IsAtHome() const { return CurrentMission == nullptr && CurrentRepairBay == nullptr; }

    /** Apply damage from mission outcome */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Damage")
    void ApplyDamage(int32 DamageAmount);

    /** Check if vehicle needs repair */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Repair")
    bool NeedsRepair() const;

    /** Checkout to a repair bay (called by MissionManager or AI/Player) */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Repair")
    bool CheckoutToRepair(UStrategyFacility* RepairBay);

    /** Return from repair bay (called when repair completes) */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Repair")
    void ReturnFromRepair();
};