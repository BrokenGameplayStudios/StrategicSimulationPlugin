#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UVehicleDefinition.h"
#include "StrategicSimulationTypes.h"
#include "UStrategyFacility.h"
#include "UStrategyVehicle.generated.h"

class UStrategyBase;
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

    /** Persistent reference to the hanger this vehicle is assigned to (reserved slot from mission/creation). Used for reliable return after repair. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
    UStrategyFacility* HomeHanger = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
    UMissionGroup* CurrentMission;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
    TArray<class UStrategySoldier*> CurrentPassengers;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
    int32 RemainingFuelDays;

    // === Vehicle Damage & Repair System ===
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Damage & Repair")
    EVehicleDamageState DamageState = EVehicleDamageState::Undamaged;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Damage & Repair")
    int32 CurrentHealth = 100;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Damage & Repair")
    UStrategyFacility* CurrentRepairBay = nullptr;

    UFUNCTION(BlueprintCallable, Category = "Vehicle")
    bool IsAtHome() const { return CurrentMission == nullptr && CurrentRepairBay == nullptr; }

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Damage")
    void ApplyDamage(int32 DamageAmount);

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Repair")
    bool NeedsRepair() const;

    /** Updates DamageState based on CurrentHealth percentage. Call after any health change. */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Damage")
    void UpdateDamageStateFromHealth();

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Repair")
    bool CheckoutToRepair(UStrategyFacility* RepairBay);

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Repair")
    void ReturnFromRepair();
};