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
    return CurrentHealth < MaxH || DamageState != EVehicleDamageState::Undamaged;
}

bool UStrategyVehicle::CheckoutToRepair(UStrategyFacility* RepairBay)
{
    if (!RepairBay || !RepairBay->FacilityDefinition || RepairBay->FacilityDefinition->FacilityType != EFacilityType::VehicleRepair)
    {
        UE_LOG(LogTemp, Error, TEXT("[VEHICLE] Cannot checkout to repair - invalid repair bay"));
        return false;
    }

    if (CurrentRepairBay)
    {
        UE_LOG(LogTemp, Warning, TEXT("[VEHICLE] %s is already in repair"), *VehicleDefinition->VehicleName.ToString());
        return false;
    }

    if (CurrentHanger && !HomeHanger)
    {
        HomeHanger = CurrentHanger;
    }

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
    if (!CurrentRepairBay)
    {
        UE_LOG(LogTemp, Warning, TEXT("[VEHICLE] ReturnFromRepair called but no CurrentRepairBay set"));
        return;
    }

    CurrentHealth = VehicleDefinition ? VehicleDefinition->MaxHealth : 100;
    DamageState = EVehicleDamageState::Undamaged;

    bool Parked = false;

    if (HomeHanger && HomeHanger->FacilityDefinition && HomeHanger->FacilityDefinition->FacilityType == EFacilityType::Hanger)
    {
        if (HomeHanger->ParkedVehicles.Num() < HomeHanger->FacilityDefinition->Capacity)
        {
            HomeHanger->ParkedVehicles.Add(this);
            CurrentHanger = HomeHanger;
            Parked = true;
            UE_LOG(LogTemp, Display, TEXT("[VEHICLE] %s fully repaired and returned to its HOME HANGER (reserved slot)"), *VehicleDefinition->VehicleName.ToString());
        }
    }

    if (!Parked && HomeBase)
    {
        for (UStrategyFacility* Hanger : HomeBase->Facilities)
        {
            if (Hanger && Hanger->FacilityDefinition && Hanger->FacilityDefinition->FacilityType == EFacilityType::Hanger)
            {
                if (Hanger->ParkedVehicles.Num() < Hanger->FacilityDefinition->Capacity)
                {
                    Hanger->ParkedVehicles.Add(this);
                    CurrentHanger = Hanger;
                    if (!HomeHanger) HomeHanger = Hanger;
                    Parked = true;
                    UE_LOG(LogTemp, Display, TEXT("[VEHICLE] %s fully repaired and returned to fallback hanger"), *VehicleDefinition->VehicleName.ToString());
                    break;
                }
            }
        }
    }

    CurrentRepairBay = nullptr;

    if (!Parked)
    {
        UE_LOG(LogTemp, Warning, TEXT("[VEHICLE] %s repaired but NO HANGER SPACE AVAILABLE anywhere"), *VehicleDefinition->VehicleName.ToString());
    }
}