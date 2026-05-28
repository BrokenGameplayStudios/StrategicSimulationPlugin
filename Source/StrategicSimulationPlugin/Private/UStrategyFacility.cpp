#include "UStrategyFacility.h"
#include "UStrategyVehicle.h"
#include "UFacilityDefinition.h"
#include "UStrategyBase.h"

void UStrategyFacility::SimulateDailyRepair(UStrategyBase* OwningBase)
{
    if (!FacilityDefinition || FacilityDefinition->RepairHealthPerDay <= 0 || !bIsOperational)
        return;

    UE_LOG(LogTemp, Verbose, TEXT("[REPAIR TICK] %s repairing %d vehicles (+%d HP/day)"),
        *FacilityDefinition->FacilityName.ToString(), VehiclesInRepair.Num(), FacilityDefinition->RepairHealthPerDay);

    TArray<UStrategyVehicle*> ToReturn;

    // PHASE 1: Repair only vehicles already assigned to THIS bay
    for (UStrategyVehicle* Vehicle : VehiclesInRepair)
    {
        if (!Vehicle) continue;

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

        if (!Vehicle->NeedsRepair())
        {
            UE_LOG(LogTemp, Display, TEXT("[REPAIR] %s has reached full health — scheduling return to hanger"),
                *Vehicle->VehicleDefinition->VehicleName.ToString());
            ToReturn.Add(Vehicle);
        }
    }

    for (UStrategyVehicle* Vehicle : ToReturn)
    {
        VehiclesInRepair.Remove(Vehicle);
        Vehicle->ReturnFromRepair();
    }

    // PHASE 2: Auto-assign (now with strict guard so only ONE repair bay can claim a vehicle)
    if (OwningBase)
    {
        for (UStrategyFacility* Hanger : OwningBase->Facilities)
        {
            if (!Hanger || !Hanger->FacilityDefinition || Hanger->FacilityDefinition->FacilityType != EFacilityType::Hanger)
                continue;

            for (int32 i = Hanger->ParkedVehicles.Num() - 1; i >= 0; --i)
            {
                UStrategyVehicle* Veh = Hanger->ParkedVehicles[i];
                if (Veh && Veh->NeedsRepair() && Veh->CurrentRepairBay == nullptr && VehiclesInRepair.Num() < FacilityDefinition->Capacity)
                {
                    if (Veh->CheckoutToRepair(this))
                    {
                        Hanger->ParkedVehicles.RemoveAt(i);
                        VehiclesInRepair.Add(Veh);
                        UE_LOG(LogTemp, Display, TEXT("[REPAIR] Auto-moved waiting damaged vehicle %s from hanger to repair bay"),
                            *Veh->VehicleDefinition->VehicleName.ToString());
                    }
                }
            }
        }
    }
}