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

    /** === NEW: Vehicle Hardpoint System (Phase 6.2) === */
/** Weapons currently equipped (launcher systems). Respects MaxWeaponSlots from definition. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Vehicle|Hardpoints")
    TArray<TSoftObjectPtr<UItemDefinition>> EquippedWeapons;

    /** Defense systems (ECM, armor plating, etc.). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Vehicle|Hardpoints")
    TArray<TSoftObjectPtr<UItemDefinition>> EquippedDefenseSystems;

    /** Parallel array — ammo count for each equipped weapon (launcher). */
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, SaveGame, Category = "Vehicle|Hardpoints")
    TArray<int32> WeaponAmmoCounts;

    /** Maximum number of weapons this vehicle can carry (pulled from VehicleDefinition) */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Hardpoints")
    int32 GetMaxWeaponSlots() const;

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Hardpoints")
    int32 GetMaxDefenseSlots() const;

    /** Returns true if there is room to equip another weapon. */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Hardpoints")
    bool CanEquipWeapon(UItemDefinition* Weapon) const;

    /** Equips a weapon (launcher) if possible and initializes ammo. */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Hardpoints")
    bool EquipWeapon(UItemDefinition* Weapon);

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Hardpoints")
    bool EquipDefenseSystem(UItemDefinition* DefenseItem);

    /** Returns currently equipped weapons (valid only). */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Hardpoints")
    TArray<UItemDefinition*> GetEquippedWeapons() const;

    /** Total offensive rating used by mission simulation (base AttackPower + equipped weapons + ammo bonus). */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Stats")
    int32 GetVehicleOffensiveRating() const;

    /** Total defensive rating (armor + defense systems). */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Stats")
    int32 GetVehicleDefensiveRating() const;

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
};