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

int32 UStrategyVehicle::GetMaxWeaponSlots() const
{
    return VehicleDefinition ? VehicleDefinition->MaxWeaponSlots : 2;
}

int32 UStrategyVehicle::GetMaxDefenseSlots() const
{
    return VehicleDefinition ? VehicleDefinition->MaxDefenseSlots : 1;
}

bool UStrategyVehicle::CanEquipWeapon(UItemDefinition* Weapon) const
{
    if (!Weapon || !Weapon->IsVehicleWeapon()) return false;
    return EquippedWeapons.Num() < GetMaxWeaponSlots();
}

bool UStrategyVehicle::EquipWeapon(UItemDefinition* Weapon)
{
    if (!CanEquipWeapon(Weapon)) return false;

    EquippedWeapons.Add(Weapon);
    // Initialize ammo to MaxAmmo from the launcher definition
    WeaponAmmoCounts.Add(Weapon->MaxAmmo);

    UE_LOG(LogTemp, Display, TEXT("[VEHICLE] %s equipped weapon '%s' (ammo: %d)"),
        *VehicleDefinition->VehicleName.ToString(), *Weapon->ItemName.ToString(), Weapon->MaxAmmo);
    return true;
}

bool UStrategyVehicle::EquipDefenseSystem(UItemDefinition* DefenseItem)
{
    if (!DefenseItem || !DefenseItem->IsVehicleDefense() || EquippedDefenseSystems.Num() >= GetMaxDefenseSlots())
        return false;

    EquippedDefenseSystems.Add(DefenseItem);
    UE_LOG(LogTemp, Display, TEXT("[VEHICLE] %s equipped defense system '%s'"),
        *VehicleDefinition->VehicleName.ToString(), *DefenseItem->ItemName.ToString());
    return true;
}

TArray<UItemDefinition*> UStrategyVehicle::GetEquippedWeapons() const
{
    TArray<UItemDefinition*> Result;
    for (const TSoftObjectPtr<UItemDefinition>& Ptr : EquippedWeapons)
    {
        if (UItemDefinition* Item = Ptr.Get()) Result.Add(Item);
    }
    return Result;
}

int32 UStrategyVehicle::GetVehicleOffensiveRating() const
{
    int32 Rating = VehicleDefinition ? VehicleDefinition->AttackPower : 0;

    for (int32 i = 0; i < EquippedWeapons.Num(); ++i)
    {
        if (UItemDefinition* Weapon = EquippedWeapons[i].Get())
        {
            Rating += Weapon->VehicleDamageBonus;
            // Small ammo bonus if partially loaded
            if (WeaponAmmoCounts.IsValidIndex(i) && Weapon->MaxAmmo > 0)
                Rating += (WeaponAmmoCounts[i] * 5); // tune later
        }
    }
    return Rating;
}

int32 UStrategyVehicle::GetVehicleDefensiveRating() const
{
    int32 Rating = 0;
    for (const TSoftObjectPtr<UItemDefinition>& Ptr : EquippedDefenseSystems)
    {
        if (UItemDefinition* Def = Ptr.Get())
            Rating += Def->VehicleDefenseBonus;
    }
    return Rating;
}