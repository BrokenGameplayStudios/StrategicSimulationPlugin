#include "UStrategyVehicle.h"
#include "UStrategyBase.h"
#include "UStrategyFacility.h"
#include "UMissionGroup.h"
#include "UFacilityDefinition.h"
#include "UItemDefinition.h"

UStrategyVehicle::UStrategyVehicle()
{
    RemainingFuelDays = 0;
    CurrentHealth = 100;
    DamageState = EVehicleDamageState::Undamaged;
}

void UStrategyVehicle::ApplyDamage(int32 DamageAmount)
{
    if (DamageAmount <= 0) return;

    CurrentHealth = FMath::Max(0, CurrentHealth - DamageAmount);
    UpdateDamageStateFromHealth();

    UE_LOG(LogTemp, Warning, TEXT("[VEHICLE] %s took %d damage → Health: %d/%d (%s)"),
        *VehicleDefinition->VehicleName.ToString(),
        DamageAmount, CurrentHealth,
        VehicleDefinition ? VehicleDefinition->MaxHealth : 100,
        *UEnum::GetValueAsString(DamageState));
}

void UStrategyVehicle::UpdateDamageStateFromHealth()
{
    if (!VehicleDefinition || VehicleDefinition->MaxHealth <= 0)
    {
        DamageState = (CurrentHealth <= 0) ? EVehicleDamageState::Destroyed : EVehicleDamageState::Undamaged;
        return;
    }

    float HealthPercent = (float)CurrentHealth / VehicleDefinition->MaxHealth;

    if (HealthPercent <= 0.0f)
        DamageState = EVehicleDamageState::Destroyed;
    else if (HealthPercent <= 0.3f)
        DamageState = EVehicleDamageState::HeavilyDamaged;
    else if (HealthPercent <= 0.7f)
        DamageState = EVehicleDamageState::LightlyDamaged;
    else
        DamageState = EVehicleDamageState::Undamaged;
}

bool UStrategyVehicle::NeedsRepair() const
{
    int32 MaxH = VehicleDefinition ? VehicleDefinition->MaxHealth : 100;
    bool bNeeds = CurrentHealth < MaxH || DamageState != EVehicleDamageState::Undamaged;

    if (bNeeds && CurrentHealth >= MaxH)
    {
        UE_LOG(LogTemp, Warning, TEXT("[VEHICLE] %s NeedsRepair() returned true even at full health! Forcing false."), *VehicleDefinition->VehicleName.ToString());
        return false;
    }
    return bNeeds;
}

bool UStrategyVehicle::EquipWeapon(UItemDefinition* Weapon)
{
    if (!Weapon || VehicleInventory.Num() >= MaxWeaponSlots)
    {
        UE_LOG(LogTemp, Warning, TEXT("[VEHICLE] %s cannot equip %s (full or invalid)"),
            *VehicleDefinition->VehicleName.ToString(), *Weapon->ItemName.ToString());
        return false;
    }

    VehicleInventory.Add(Weapon);
    UE_LOG(LogTemp, Display, TEXT("[VEHICLE] %s equipped weapon: %s"),
        *VehicleDefinition->VehicleName.ToString(), *Weapon->ItemName.ToString());
    return true;
}

bool UStrategyVehicle::UnequipWeapon(UItemDefinition* Weapon)
{
    if (!Weapon) return false;
    return VehicleInventory.Remove(Weapon) > 0;
}

TArray<UItemDefinition*> UStrategyVehicle::GetLoadedWeapons() const
{
    TArray<UItemDefinition*> Loaded;
    for (const TSoftObjectPtr<UItemDefinition>& ItemPtr : VehicleInventory)
    {
        if (UItemDefinition* Item = ItemPtr.Get())
        {
            Loaded.Add(Item);
        }
    }
    return Loaded;
}

int32 UStrategyVehicle::GetTotalWeaponBonus() const
{
    int32 Total = 0;
    for (const TSoftObjectPtr<UItemDefinition>& ItemPtr : VehicleInventory)
    {
        if (UItemDefinition* Item = ItemPtr.Get())
        {
            // TODO: later add more fields to UItemDefinition (e.g. VehicleDamageBonus, AccuracyBonus)
            // For now we just reuse AimBonus as a simple power bonus
            Total += Item->AimBonus;
        }
    }
    return Total;
}