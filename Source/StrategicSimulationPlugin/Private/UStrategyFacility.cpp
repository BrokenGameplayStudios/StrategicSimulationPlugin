#include "UStrategyFacility.h"
#include "UStrategyVehicle.h"
#include "UStrategySoldier.h"
#include "UFacilityDefinition.h"
#include "UStrategyBase.h"

void UStrategyFacility::SimulateDailyRepair(UStrategyBase* OwningBase)
{
    if (!FacilityDefinition || !bIsOperational || !OwningBase)
        return;

    // === VEHICLE REPAIR (unchanged from what was already working) ===
    if (FacilityDefinition->RepairHealthPerDay > 0 && FacilityDefinition->FacilityType == EFacilityType::VehicleRepair)
    {
        int32 RepairsRemaining = FacilityDefinition->Capacity;

        UE_LOG(LogTemp, Display, TEXT("[REPAIR TICK] %s can repair up to %d vehicles (+%d HP each)"),
            *FacilityDefinition->FacilityName.ToString(), RepairsRemaining, FacilityDefinition->RepairHealthPerDay);

        for (UStrategyFacility* Hanger : OwningBase->Facilities)
        {
            if (!Hanger || !Hanger->FacilityDefinition || Hanger->FacilityDefinition->FacilityType != EFacilityType::Hanger)
                continue;

            for (UStrategyVehicle* Vehicle : Hanger->ParkedVehicles)
            {
                if (!Vehicle || !Vehicle->NeedsRepair() || RepairsRemaining <= 0)
                    continue;

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

    // === SOLDIER MEDICAL HEALING (exactly parallel to vehicle repair) ===
    if (FacilityDefinition->FacilityType == EFacilityType::Medical)
    {
        int32 HealsRemaining = FacilityDefinition->Capacity;

        UE_LOG(LogTemp, Display, TEXT("[MEDICAL TICK] %s can heal up to %d soldiers"),
            *FacilityDefinition->FacilityName.ToString(), HealsRemaining);

        for (UStrategySoldier* Soldier : OwningBase->GetStationedSoldiers())
        {
            if (!Soldier || !Soldier->NeedsHealing() || HealsRemaining <= 0)
                continue;

            Soldier->Heal(3);   // 3 HP per day — easy to change later
            HealsRemaining--;
        }
    }
}