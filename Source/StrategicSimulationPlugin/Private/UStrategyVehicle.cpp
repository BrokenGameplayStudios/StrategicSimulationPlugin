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
    bool bNeeds = CurrentHealth < MaxH || DamageState != EVehicleDamageState::Undamaged;

    if (bNeeds && CurrentHealth >= MaxH)
    {
        UE_LOG(LogTemp, Warning, TEXT("[VEHICLE] %s NeedsRepair() returned true even at full health! Forcing false."), *VehicleDefinition->VehicleName.ToString());
        return false;
    }
    return bNeeds;
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
        UE_LOG(LogTemp, Verbose, TEXT("[VEHICLE] %s is already checked out to another repair bay - skipping"), *VehicleDefinition->VehicleName.ToString());
        return false;
    }

    if (!NeedsRepair())
    {
        UE_LOG(LogTemp, Verbose, TEXT("[VEHICLE] %s is full health - skipping checkout"), *VehicleDefinition->VehicleName.ToString());
        return false;
    }

    if (CurrentHanger && !HomeHanger)
    {
        HomeHanger = CurrentHanger;
        UE_LOG(LogTemp, Display, TEXT("[VEHICLE] %s → HomeHanger assigned to %s"), *VehicleDefinition->VehicleName.ToString(), *HomeHanger->FacilityDefinition->FacilityName.ToString());
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
    UpdateDamageStateFromHealth();

    bool Parked = false;

    UE_LOG(LogTemp, Display, TEXT("[RETURN DEBUG] %s attempting return | HomeHanger=%s | HomeBase=%s"),
        *VehicleDefinition->VehicleName.ToString(),
        HomeHanger ? *HomeHanger->FacilityDefinition->FacilityName.ToString() : TEXT("NULL"),
        HomeBase ? *HomeBase->BaseName.ToString() : TEXT("NULL"));

    // === FORCE RETURN TO OWNED HOME HANGER (this fixes the bug) ===
    if (HomeHanger && HomeHanger->FacilityDefinition && HomeHanger->FacilityDefinition->FacilityType == EFacilityType::Hanger)
    {
        int32 CurrentSlots = HomeHanger->ParkedVehicles.Num();
        int32 MaxSlots = HomeHanger->FacilityDefinition->Capacity;

        UE_LOG(LogTemp, Display, TEXT("[RETURN DEBUG] HomeHanger '%s' capacity: %d/%d"), *HomeHanger->FacilityDefinition->FacilityName.ToString(), CurrentSlots, MaxSlots);

        if (CurrentSlots < MaxSlots)
        {
            HomeHanger->ParkedVehicles.Add(this);
            CurrentHanger = HomeHanger;
            Parked = true;
            UE_LOG(LogTemp, Display, TEXT("[VEHICLE] %s fully repaired and returned to its HOME HANGER (reserved slot)"), *VehicleDefinition->VehicleName.ToString());
        }
        else
        {
            // FORCE RECLAIM: evict the last vehicle to guarantee our owned slot
            if (HomeHanger->ParkedVehicles.Num() > 0)
            {
                UStrategyVehicle* Evicted = HomeHanger->ParkedVehicles.Last();
                UE_LOG(LogTemp, Display, TEXT("[RETURN DEBUG] HomeHanger full — evicting %s to reclaim slot for %s"), *Evicted->VehicleDefinition->VehicleName.ToString(), *VehicleDefinition->VehicleName.ToString());
                HomeHanger->ParkedVehicles.RemoveAt(HomeHanger->ParkedVehicles.Num() - 1);
            }

            HomeHanger->ParkedVehicles.Add(this);
            CurrentHanger = HomeHanger;
            Parked = true;
            UE_LOG(LogTemp, Display, TEXT("[VEHICLE] %s FORCIBLY returned to its HOME HANGER (slot reclaimed)"), *VehicleDefinition->VehicleName.ToString());
        }
    }

    // Fallback only if something went wrong with HomeHanger
    if (!Parked && HomeBase)
    {
        UE_LOG(LogTemp, Display, TEXT("[RETURN DEBUG] No space in HomeHanger — checking all hangers in base '%s'"), *HomeBase->BaseName.ToString());
        for (UStrategyFacility* Hanger : HomeBase->Facilities)
        {
            if (Hanger && Hanger->FacilityDefinition && Hanger->FacilityDefinition->FacilityType == EFacilityType::Hanger)
            {
                int32 CurrentSlots = Hanger->ParkedVehicles.Num();
                int32 MaxSlots = Hanger->FacilityDefinition->Capacity;
                UE_LOG(LogTemp, Display, TEXT("[RETURN DEBUG]   → Hanger '%s': %d/%d"), *Hanger->FacilityDefinition->FacilityName.ToString(), CurrentSlots, MaxSlots);

                if (CurrentSlots < MaxSlots)
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
        UE_LOG(LogTemp, Error, TEXT("[VEHICLE] CRITICAL — %s repaired but NO HANGER SPACE AVAILABLE anywhere!"), *VehicleDefinition->VehicleName.ToString());
    }
}