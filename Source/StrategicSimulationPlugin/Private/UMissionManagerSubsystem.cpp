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

    // Mark vehicles as on mission (checked out from hanger)
    for (UStrategyVehicle* Vehicle : Vehicles)
    {
        Vehicle->CurrentMission = NewMission;
        Vehicle->CurrentHanger = nullptr;
    }

    UE_LOG(LogTemp, Display, TEXT("[MISSION] Launched mission with %d vehicles from base '%s'"),
        Vehicles.Num(), *OriginBase->BaseName.ToString());

    return NewMission;
}

void UMissionManagerSubsystem::SimulateOneDay()
{
    // Placeholder for Phase 2 (daily simulation + random events)
    // We will expand this next
}