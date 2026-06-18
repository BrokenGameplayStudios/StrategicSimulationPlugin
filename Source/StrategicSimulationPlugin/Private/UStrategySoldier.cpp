#include "UStrategySoldier.h"
#include "UStrategyFacility.h"
#include "UFacilityDefinition.h"

/** Default-constructs soldier health and status flags. */
UStrategySoldier::UStrategySoldier()
{
    CurrentStats.Health = 10;
    Status = ESoldierStatus::Healthy;
    bIsWounded = false;
    bIsPOW = false;
    bIsKIA = false;
    bIsMIA = false;
    DaysUntilRecovered = 0;
}

/** Reduces health and updates status. */
void UStrategySoldier::ApplyDamage(int32 DamageAmount)
{
    if (DamageAmount <= 0) return;

    CurrentStats.Health = FMath::Max(0, CurrentStats.Health - DamageAmount);
    UpdateStatusFromHealth();

    UE_LOG(LogTemp, Warning, TEXT("[SOLDIER] %s took %d damage → Health: %d (%s)"),
        *SoldierName, DamageAmount, CurrentStats.Health, *UEnum::GetValueAsString(Status));
}

/** True when wounded but not dead. */
bool UStrategySoldier::NeedsHealing() const
{
    return Status != ESoldierStatus::Healthy && Status != ESoldierStatus::Dead;
}

/** Restores health and updates status. */
void UStrategySoldier::Heal(int32 Amount)
{
    if (Amount <= 0 || Status == ESoldierStatus::Dead) return;

    CurrentStats.Health = FMath::Min(CurrentStats.Health + Amount, 10);
    UpdateStatusFromHealth();

    UE_LOG(LogTemp, Display, TEXT("[MEDICAL] %s healed +%d HP → Health: %d (%s)"),
        *SoldierName, Amount, CurrentStats.Health, *UEnum::GetValueAsString(Status));
}

/** Derives status and recovery days from health. */
void UStrategySoldier::UpdateStatusFromHealth()
{
    if (CurrentStats.Health <= 0)
    {
        Status = ESoldierStatus::Dead;
        bIsWounded = true;
        DaysUntilRecovered = 0;
    }
    else if (CurrentStats.Health <= 3)
    {
        Status = ESoldierStatus::Critical;
        bIsWounded = true;
        DaysUntilRecovered = FMath::RandRange(3, 6);
    }
    else if (CurrentStats.Health <= 6)
    {
        Status = ESoldierStatus::Wounded;
        bIsWounded = true;
        DaysUntilRecovered = FMath::RandRange(1, 3);
    }
    else
    {
        Status = ESoldierStatus::Healthy;
        bIsWounded = false;
        DaysUntilRecovered = 0;
    }
}

/** Logs soldier debug info to output. */
void UStrategySoldier::PrintInfo() const
{
    UE_LOG(LogTemp, Display, TEXT("[SOLDIER] %s | Class: %s | Health: %d | Status: %s | Wounded: %s | POW: %s | KIA: %s | Days to recover: %d"),
        *SoldierName,
        ClassDefinition ? *ClassDefinition->ClassName.ToString() : TEXT("None"),
        CurrentStats.Health,
        *UEnum::GetValueAsString(Status),
        bIsWounded ? TEXT("Yes") : TEXT("No"),
        bIsPOW ? TEXT("Yes") : TEXT("No"),
        bIsKIA ? TEXT("Yes") : TEXT("No"),
        DaysUntilRecovered);
}

/** Combines base stats with loadout item bonuses. */
FSoldierStats UStrategySoldier::GetEffectiveStats() const
{
    FSoldierStats Effective = CurrentStats;  // base from class
    if (!ClassDefinition) return Effective;

    // TODO: later also add class bonuses if any

    for (const TSoftObjectPtr<UItemDefinition>& ItemPtr : CurrentLoadout)
    {
        if (UItemDefinition* Item = ItemPtr.Get())
        {
            Effective.Aim += Item->AimBonus;      // assuming these fields exist on UItemDefinition
            Effective.Defense += Item->ArmorBonus;
            // Add more (Mobility, etc.) as needed
        }
    }
    return Effective;
}

/** Returns effective Aim from GetEffectiveStats. */
int32 UStrategySoldier::GetEffectiveAim() const { return GetEffectiveStats().Aim; }
/** Returns effective Defense from GetEffectiveStats. */
int32 UStrategySoldier::GetEffectiveDefense() const { return GetEffectiveStats().Defense; }