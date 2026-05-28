#include "UStrategyFacility.h"
#include "UStrategyVehicle.h"
#include "UFacilityDefinition.h"

void UStrategyFacility::SimulateDailyRepair()
{
    UE_LOG(LogTemp, Verbose, TEXT("[REPAIR TICK] SimulateDailyRepair called on %s (Type: %s, Operational: %s, RepairHP/Day: %d, VehiclesInRepair: %d)"),
        FacilityDefinition ? *FacilityDefinition->FacilityName.ToString() : TEXT("NULL"),
        FacilityDefinition ? *UEnum::GetValueAsString(FacilityDefinition->FacilityType) : TEXT("NULL"),
        bIsOperational ? TEXT("YES") : TEXT("NO"),
        FacilityDefinition ? FacilityDefinition->RepairHealthPerDay : 0,
        VehiclesInRepair.Num());

    if (!FacilityDefinition)
    {
        UE_LOG(LogTemp, Warning, TEXT("[REPAIR TICK] Skipping - no FacilityDefinition"));
        return;
    }

    if (FacilityDefinition->RepairHealthPerDay <= 0)
    {
        UE_LOG(LogTemp, Verbose, TEXT("[REPAIR TICK] Skipping - RepairHealthPerDay = %d"), FacilityDefinition->RepairHealthPerDay);
        return;
    }

    if (!bIsOperational)
    {
        UE_LOG(LogTemp, Warning, TEXT("[REPAIR TICK] Skipping - facility is NOT operational (bIsOperational = false)"));
        return;
    }

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

    // Return fully repaired vehicles to home base
    for (UStrategyVehicle* Vehicle : ToReturn)
    {
        VehiclesInRepair.Remove(Vehicle);
        Vehicle->ReturnFromRepair();
    }
}