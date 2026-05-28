#include "UStrategySoldier.h"
#include "UStrategyFacility.h"
#include "UFacilityDefinition.h"

UStrategySoldier::UStrategySoldier()
{
    CurrentStats.Health = 10;
    Status = ESoldierStatus::Healthy;
    bIsWounded = false;
    DaysUntilRecovered = 0;
}

void UStrategySoldier::ApplyDamage(int32 DamageAmount)
{
    if (DamageAmount <= 0) return;

    CurrentStats.Health = FMath::Max(0, CurrentStats.Health - DamageAmount);
    UpdateStatusFromHealth();

    UE_LOG(LogTemp, Warning, TEXT("[SOLDIER] %s took %d damage → Health: %d (%s)"),
        *SoldierName, DamageAmount, CurrentStats.Health, *UEnum::GetValueAsString(Status));
}

bool UStrategySoldier::NeedsHealing() const
{
    return Status != ESoldierStatus::Healthy && Status != ESoldierStatus::Dead;
}

void UStrategySoldier::Heal(int32 Amount)
{
    if (Amount <= 0 || Status == ESoldierStatus::Dead) return;

    CurrentStats.Health = FMath::Min(CurrentStats.Health + Amount, 10);
    UpdateStatusFromHealth();

    UE_LOG(LogTemp, Display, TEXT("[MEDICAL] %s healed +%d HP → Health: %d (%s)"),
        *SoldierName, Amount, CurrentStats.Health, *UEnum::GetValueAsString(Status));
}

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

void UStrategySoldier::PrintInfo() const
{
    UE_LOG(LogTemp, Display, TEXT("[SOLDIER] %s | Class: %s | Health: %d | Status: %s | Wounded: %s | Days to recover: %d"),
        *SoldierName,
        ClassDefinition ? *ClassDefinition->ClassName.ToString() : TEXT("None"),
        CurrentStats.Health,
        *UEnum::GetValueAsString(Status),
        bIsWounded ? TEXT("Yes") : TEXT("No"),
        DaysUntilRecovered);
}