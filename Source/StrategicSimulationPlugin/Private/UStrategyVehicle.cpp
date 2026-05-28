#include "UStrategyVehicle.h"
#include "UStrategyBase.h"
#include "UStrategyFacility.h"
#include "UMissionGroup.h"
#include "UFacilityDefinition.h"

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

    float HealthPercent = (float)CurrentHealth / (VehicleDefinition ? VehicleDefinition->MaxHealth : 100);
    if (HealthPercent <= 0.0f)
        DamageState = EVehicleDamageState::Destroyed;
    else if (HealthPercent <= 0.3f)
        DamageState = EVehicleDamageState::HeavilyDamaged;
    else if (HealthPercent <= 0.7f)
        DamageState = EVehicleDamageState::LightlyDamaged;
    else
        DamageState = EVehicleDamageState::Undamaged;

    UE_LOG(LogTemp, Warning, TEXT("[VEHICLE] %s took %d damage → Health: %d/%d (%s)"),
        *VehicleDefinition->VehicleName.ToString(),
        DamageAmount, CurrentHealth,
        VehicleDefinition ? VehicleDefinition->MaxHealth : 100,
        *UEnum::GetValueAsString(DamageState));
}

bool UStrategyVehicle::NeedsRepair() const
{
    return DamageState != EVehicleDamageState::Undamaged;
}

bool UStrategyVehicle::CheckoutToRepair(UStrategyFacility* RepairBay)
{
    if (!RepairBay || !RepairBay->FacilityDefinition || RepairBay->FacilityDefinition->FacilityType != EFacilityType::Workshop)
    {
        UE_LOG(LogTemp, Error, TEXT("[VEHICLE] Cannot checkout to repair - invalid repair bay"));
        return false;
    }

    if (CurrentRepairBay)
    {
        UE_LOG(LogTemp, Warning, TEXT("[VEHICLE] %s is already in repair"), *VehicleDefinition->VehicleName.ToString());
        return false;
    }

    // Remove from current hanger if parked
    if (CurrentHanger)
    {
        CurrentHanger->ParkedVehicles.Remove(this);
        CurrentHanger = nullptr;
    }

    CurrentRepairBay = RepairBay;
    UE_LOG(LogTemp, Display, TEXT("[VEHICLE] %s checked out to repair bay"), *VehicleDefinition->VehicleName.ToString());
    return true;
}

void UStrategyVehicle::ReturnFromRepair()
{
    if (!CurrentRepairBay) return;

    CurrentHealth = VehicleDefinition ? VehicleDefinition->MaxHealth : 100;
    DamageState = EVehicleDamageState::Undamaged;

    // === AUTO-PARK back into first available hanger slot ===
    bool Parked = false;
    if (HomeBase)
    {
        for (UStrategyFacility* Hanger : HomeBase->Facilities)
        {
            if (Hanger && Hanger->FacilityDefinition && Hanger->FacilityDefinition->FacilityType == EFacilityType::Hanger)
            {
                if (Hanger->ParkedVehicles.Num() < Hanger->FacilityDefinition->Capacity)
                {
                    Hanger->ParkedVehicles.Add(this);
                    CurrentHanger = Hanger;
                    Parked = true;
                    break;
                }
            }
        }
    }

    CurrentRepairBay = nullptr;

    if (Parked)
    {
        UE_LOG(LogTemp, Display, TEXT("[VEHICLE] %s fully repaired and returned to hanger slot"), *VehicleDefinition->VehicleName.ToString());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[VEHICLE] %s repaired but no hanger space available"), *VehicleDefinition->VehicleName.ToString());
    }
}