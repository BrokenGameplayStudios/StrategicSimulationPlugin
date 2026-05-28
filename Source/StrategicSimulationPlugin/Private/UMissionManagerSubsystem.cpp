#include "UMissionManagerSubsystem.h"
#include "UStrategyVehicle.h"
#include "UStrategyBase.h"
#include "UTimeManagerSubsystem.h"
#include "Engine/Engine.h"

void UMissionManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    UE_LOG(LogTemp, Display, TEXT("UMissionManagerSubsystem initialized — vehicle missions ready"));
}

void UMissionManagerSubsystem::OnDayPassed(int32 NewDay)
{
    UE_LOG(LogTemp, Verbose, TEXT("[MISSION] Day %d — SimulateOneDay() called (ActiveMissions: %d)"), NewDay, ActiveMissions.Num());
    SimulateOneDay();
}

UMissionGroup* UMissionManagerSubsystem::StartMission(UStrategyBase* OriginBase, TArray<UStrategyVehicle*> Vehicles, int32 DurationDays)
{
    if (!OriginBase || Vehicles.Num() == 0) return nullptr;

    UMissionGroup* NewMission = NewObject<UMissionGroup>();
    NewMission->OriginBase = OriginBase;
    NewMission->VehiclesInFleet = Vehicles;
    NewMission->StartDay = GetGameInstance()->GetSubsystem<UTimeManagerSubsystem>()->GetCurrentDay();
    NewMission->DurationDays = DurationDays;
    NewMission->Status = EMissionStatus::InProgress;

    ActiveMissions.Add(NewMission);

    // Check vehicles out of hangers
    for (UStrategyVehicle* Vehicle : Vehicles)
    {
        if (Vehicle->CurrentHanger)
        {
            Vehicle->CurrentHanger->ParkedVehicles.Remove(Vehicle);
            Vehicle->CurrentHanger = nullptr;
        }
        Vehicle->CurrentMission = NewMission;
    }

    UE_LOG(LogTemp, Display, TEXT("[MISSION] Launched mission with %d vehicles from base '%s' (duration: %d days)"),
        Vehicles.Num(), *OriginBase->BaseName.ToString(), DurationDays);

    return NewMission;
}

UMissionGroup* UMissionManagerSubsystem::LaunchMissionFromBase(UStrategyBase* OriginBase, int32 DurationDays)
{
    if (!OriginBase) return nullptr;

    // === PREVENT SPAM: Only launch if no active mission from this base ===
    for (UMissionGroup* Existing : ActiveMissions)
    {
        if (Existing && Existing->OriginBase == OriginBase && Existing->Status == EMissionStatus::InProgress)
        {
            return nullptr;
        }
    }

    TArray<UStrategyVehicle*> AvailableVehicles;
    for (UStrategyFacility* Fac : OriginBase->Facilities)
    {
        if (Fac && Fac->FacilityDefinition && Fac->FacilityDefinition->FacilityType == EFacilityType::Hanger)
        {
            AvailableVehicles.Append(Fac->ParkedVehicles);
        }
    }

    if (AvailableVehicles.Num() == 0)
    {
        return nullptr;
    }

    return StartMission(OriginBase, AvailableVehicles, DurationDays);
}

void UMissionManagerSubsystem::SimulateOneDay()
{
    UE_LOG(LogTemp, Verbose, TEXT("[MISSION] SimulateOneDay running — %d active missions"), ActiveMissions.Num());

    TArray<UMissionGroup*> ToRemove;

    for (UMissionGroup* Mission : ActiveMissions)
    {
        if (!Mission || Mission->Status != EMissionStatus::InProgress) continue;

        Mission->DurationDays--;

        UE_LOG(LogTemp, Verbose, TEXT("[MISSION] Mission from '%s' — %d days remaining"),
            *Mission->OriginBase->BaseName.ToString(), Mission->DurationDays);

        if (Mission->DurationDays <= 0)
        {
            Mission->Status = EMissionStatus::Returning;

            // Return vehicles to hangers
            for (UStrategyVehicle* Vehicle : Mission->VehiclesInFleet)
            {
                if (Vehicle && Vehicle->HomeBase)
                {
                    for (UStrategyFacility* Hanger : Vehicle->HomeBase->Facilities)
                    {
                        if (Hanger && Hanger->FacilityDefinition && Hanger->FacilityDefinition->FacilityType == EFacilityType::Hanger)
                        {
                            if (Hanger->ParkedVehicles.Num() < Hanger->FacilityDefinition->Capacity)
                            {
                                Hanger->ParkedVehicles.Add(Vehicle);
                                Vehicle->CurrentHanger = Hanger;
                                Vehicle->CurrentMission = nullptr;
                                break;
                            }
                        }
                    }
                }
            }

            UE_LOG(LogTemp, Display, TEXT("[MISSION] ✅ Mission from '%s' has RETURNED to base"),
                *Mission->OriginBase->BaseName.ToString());

            ToRemove.Add(Mission);
            OnMissionCompleted.Broadcast(Mission);
        }
    }

    for (UMissionGroup* Mission : ToRemove)
    {
        ActiveMissions.Remove(Mission);
    }
}