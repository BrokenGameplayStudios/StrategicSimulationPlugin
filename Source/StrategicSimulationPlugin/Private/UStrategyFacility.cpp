#include "UStrategyFacility.h"
#include "UStrategyVehicle.h"
#include "UFacilityDefinition.h"
#include "UStrategyBase.h"

void UStrategyFacility::SimulateDailyRepair(UStrategyBase* OwningBase)
{
    if (!FacilityDefinition || FacilityDefinition->RepairHealthPerDay <= 0 || !bIsOperational)
        return;

    if (!OwningBase)
        return;

    // Repair bays only heal parked vehicles — no moving, no lists
    int32 RepairsRemaining = FacilityDefinition->Capacity;

    UE_LOG(LogTemp, Verbose, TEXT("[REPAIR TICK] %s can repair up to %d vehicles (+%d HP each)"),
        *FacilityDefinition->FacilityName.ToString(), RepairsRemaining, FacilityDefinition->RepairHealthPerDay);

    for (UStrategyFacility* Hanger : OwningBase->Facilities)
    {
        if (!Hanger || !Hanger->FacilityDefinition || Hanger->FacilityDefinition->FacilityType != EFacilityType::Hanger)
            continue;

        for (UStrategyVehicle* Vehicle : Hanger->ParkedVehicles)
        {
            if (!Vehicle || !Vehicle->NeedsRepair() || RepairsRemaining <= 0)
                continue;

            int32 OldHealth = Vehicle->CurrentHealth;

            Vehicle->CurrentHealth = FMath::Min(
                Vehicle->CurrentHealth + FacilityDefinition->RepairHealthPerDay,
                Vehicle->VehicleDefinition ? Vehicle->VehicleDefinition->MaxHealth : 100
            );

            Vehicle->UpdateDamageStateFromHealth();

            UE_LOG(LogTemp, Display, TEXT("[REPAIR] %s repaired +%d HP → %d/%d"),
                *Vehicle->VehicleDefinition->VehicleName.ToString(),
                FacilityDefinition->RepairHealthPerDay,
                Vehicle->CurrentHealth,
                Vehicle->VehicleDefinition ? Vehicle->VehicleDefinition->MaxHealth : 100);

            RepairsRemaining--;

            if (!Vehicle->NeedsRepair())
            {
                UE_LOG(LogTemp, Display, TEXT("[REPAIR] %s has reached full health"),
                    *Vehicle->VehicleDefinition->VehicleName.ToString());
            }
        }
    }
}