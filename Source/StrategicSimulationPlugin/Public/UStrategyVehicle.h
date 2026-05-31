#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UVehicleDefinition.h"
#include "StrategicSimulationTypes.h"
#include "UStrategyVehicle.generated.h"

class UStrategyBase;
class UMissionGroup;
class UStrategyFacility;

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

    /** Permanent reserved hanger slot — belongs to this vehicle until it is destroyed */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
    UStrategyFacility* HomeHanger = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
    UMissionGroup* CurrentMission;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
    TArray<class UStrategySoldier*> CurrentPassengers;

    /** Vehicle-specific weapons/gear (rockets, cannons, etc.). Can be equipped via production or research. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Vehicle|Inventory")
    TArray<TSoftObjectPtr<UItemDefinition>> VehicleInventory;

    /** Maximum number of weapons this vehicle can carry (tune per vehicle type later). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle|Inventory")
    int32 MaxWeaponSlots = 4;

    /** Equips a weapon if there's room. Returns true on success. */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Inventory")
    bool EquipWeapon(UItemDefinition* Weapon);

    /** Removes a specific weapon from the vehicle. */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Inventory")
    bool UnequipWeapon(UItemDefinition* Weapon);

    /** Returns all currently equipped (valid) weapons. */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Inventory")
    TArray<UItemDefinition*> GetLoadedWeapons() const;

    /** Calculates total weapon bonus for mission simulation (damage, accuracy, etc.). */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Inventory")
    int32 GetTotalWeaponBonus() const;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
    int32 RemainingFuelDays;

    // === Vehicle Damage & Repair System ===
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Damage & Repair")
    EVehicleDamageState DamageState = EVehicleDamageState::Undamaged;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Damage & Repair")
    int32 CurrentHealth = 100;

    UFUNCTION(BlueprintCallable, Category = "Vehicle")
    bool IsAtHome() const { return CurrentMission == nullptr; }

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Damage")
    void ApplyDamage(int32 DamageAmount);

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Repair")
    bool NeedsRepair() const;

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Damage")
    void UpdateDamageStateFromHealth();

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Inventory")
    TArray<UItemDefinition*> GetLoadedWeapons() const;
};