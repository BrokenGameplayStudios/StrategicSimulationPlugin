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

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
    UStrategyFacility* HomeHanger = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
    UMissionGroup* CurrentMission;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
    TArray<class UStrategySoldier*> CurrentPassengers;

    // === Hardpoint System ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Vehicle|Hardpoints")
    TArray<TSoftObjectPtr<UItemDefinition>> EquippedWeapons;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Vehicle|Hardpoints")
    TArray<TSoftObjectPtr<UItemDefinition>> EquippedDefenseSystems;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, SaveGame, Category = "Vehicle|Hardpoints")
    TArray<int32> WeaponAmmoCounts;

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Hardpoints")
    int32 GetMaxWeaponSlots() const;

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Hardpoints")
    int32 GetMaxDefenseSlots() const;

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Hardpoints")
    bool CanEquipWeapon(UItemDefinition* Weapon) const;

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Hardpoints")
    bool EquipWeapon(UItemDefinition* Weapon);

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Hardpoints")
    bool EquipDefenseSystem(UItemDefinition* DefenseItem);

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Hardpoints")
    TArray<UItemDefinition*> GetEquippedWeapons() const;

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Stats")
    int32 GetVehicleOffensiveRating() const;

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Stats")
    int32 GetVehicleDefensiveRating() const;

    // === NEW: Range / Fuel System ===
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Range")
    float CurrentRangeLeft = 0.0f;

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Range")
    float GetMaxRange() const;

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Range")
    bool HasEnoughRangeForMission(float RequiredDistance) const;

    // === Damage & Repair ===
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