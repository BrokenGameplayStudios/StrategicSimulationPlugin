#include "UStrategyFacility.h"
#include "UStrategyVehicle.h"
#include "UFacilityDefinition.h"

void UStrategyFacility::SimulateDailyRepair()
{
    if (!FacilityDefinition || FacilityDefinition->RepairHealthPerDay <= 0)
        return;

    if (!bIsOperational)
        return;

    TArray<UStrategyVehicle*> ToReturn;

    for (UStrategyVehicle* Vehicle : VehiclesInRepair)
    {
        if (!Vehicle) continue;

        Vehicle->CurrentHealth = FMath::Min(
            Vehicle->CurrentHealth + FacilityDefinition->RepairHealthPerDay,
            Vehicle->VehicleDefinition ? Vehicle->VehicleDefinition->MaxHealth : 100
        );

        UE_LOG(LogTemp, Display, TEXT("[REPAIR] %s repaired +%d HP → %d/%d"),
            *Vehicle->VehicleDefinition->VehicleName.ToString(),
            FacilityDefinition->RepairHealthPerDay,
            Vehicle->CurrentHealth,
            Vehicle->VehicleDefinition ? Vehicle->VehicleDefinition->MaxHealth : 100);

        if (!Vehicle->NeedsRepair())
        {
            ToReturn.Add(Vehicle);
        }
    }

    // Return fully repaired vehicles to home base (no longer occupy repair slot)
    for (UStrategyVehicle* Vehicle : ToReturn)
    {
        VehiclesInRepair.Remove(Vehicle);
        Vehicle->ReturnFromRepair();   // This now automatically parks the vehicle back into a hanger
    }
}