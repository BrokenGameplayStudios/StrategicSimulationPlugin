#include "UMissionManagerSubsystem.h"
#include "UStrategyVehicle.h"
#include "UStrategyBase.h"
#include "UStrategyFacility.h"
#include "UTimeManagerSubsystem.h"
#include "UResourceManagerSubsystem.h"
#include "USoldierManagerSubsystem.h"
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
            ResolveMissionOutcome(Mission);
            ToRemove.Add(Mission);
        }
    }

    for (UMissionGroup* Mission : ToRemove)
    {
        ActiveMissions.Remove(Mission);
    }
}

void UMissionManagerSubsystem::ResolveMissionOutcome(UMissionGroup* Mission)
{
    if (!Mission || !Mission->OriginBase || Mission->VehiclesInFleet.Num() == 0) return;

    // Weighted random outcome (tweak percentages anytime)
    const int32 Roll = FMath::RandRange(1, 100);
    if (Roll <= 55)      Mission->Outcome = EMissionOutcome::Success;
    else if (Roll <= 80) Mission->Outcome = EMissionOutcome::PartialSuccess;
    else if (Roll <= 95) Mission->Outcome = EMissionOutcome::Failure;
    else                 Mission->Outcome = EMissionOutcome::CatastrophicFailure;

    FString OutcomeStr;
    FResourceStockpile Reward;
    int32 SoldiersLost = 0;
    int32 VehiclesLostThisMission = 0;

    switch (Mission->Outcome)
    {
    case EMissionOutcome::Success:
        OutcomeStr = TEXT("SUCCESS");
        Reward.Money = FMath::RandRange(1500, 2800);
        Reward.Supplies = FMath::RandRange(800, 1600);
        SoldiersLost = FMath::RandRange(0, 2);
        break;

    case EMissionOutcome::PartialSuccess:
        OutcomeStr = TEXT("PARTIAL SUCCESS");
        Reward.Money = FMath::RandRange(700, 1500);
        Reward.Supplies = FMath::RandRange(400, 900);
        SoldiersLost = FMath::RandRange(1, 4);
        break;

    case EMissionOutcome::Failure:
        OutcomeStr = TEXT("FAILURE");
        Reward.Money = FMath::RandRange(0, 500);
        Reward.Supplies = FMath::RandRange(0, 300);
        SoldiersLost = FMath::RandRange(3, 6);
        break;

    case EMissionOutcome::CatastrophicFailure:
        OutcomeStr = TEXT("CATASTROPHIC FAILURE");
        Reward.Money = -FMath::RandRange(400, 1200);
        Reward.Supplies = -FMath::RandRange(300, 800);
        SoldiersLost = FMath::RandRange(5, 10);
        VehiclesLostThisMission = FMath::RandRange(0, 2); // can lose up to 2 vehicles
        break;
    }

    Mission->ResourcesGained = Reward;
    Mission->SoldiersKilled = SoldiersLost;
    Mission->VehiclesLost = VehiclesLostThisMission;

    // === Apply Rewards / Losses ===
    UResourceManagerSubsystem* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>();
    USoldierManagerSubsystem* SoldierMgr = GetGameInstance()->GetSubsystem<USoldierManagerSubsystem>();

    if (ResourceMgr)
    {
        // TODO Phase 4: Make this dynamic per-base faction (currently Enemy because AI launches)
        ResourceMgr->AddResources(EFactionType::Enemy, Reward);
    }

    // Soldier casualties (remove from roster)
    if (SoldierMgr && SoldiersLost > 0)
    {
        TArray<UStrategySoldier*> AllPassengers;
        for (UStrategyVehicle* Veh : Mission->VehiclesInFleet)
        {
            AllPassengers.Append(Veh->CurrentPassengers);
        }

        for (int32 i = 0; i < SoldiersLost && AllPassengers.Num() > 0; ++i)
        {
            int32 RandomIndex = FMath::RandRange(0, AllPassengers.Num() - 1);
            UStrategySoldier* DeadSoldier = AllPassengers[RandomIndex];
            SoldierMgr->DismissSoldier(DeadSoldier);
            AllPassengers.RemoveAt(RandomIndex);
        }
    }

    // === Return / Destroy Vehicles ===
    int32 VehiclesToDestroy = VehiclesLostThisMission;
    for (UStrategyVehicle* Vehicle : Mission->VehiclesInFleet)
    {
        if (!Vehicle) continue;

        if (VehiclesToDestroy > 0)
        {
            VehiclesToDestroy--;
            UE_LOG(LogTemp, Warning, TEXT("[MISSION] 💥 Vehicle '%s' was DESTROYED"), *Vehicle->VehicleDefinition->VehicleName.ToString());
            continue; // vehicle is gone
        }

        // Park surviving vehicle back in first available hanger slot
        bool Parked = false;
        for (UStrategyFacility* Hanger : Mission->OriginBase->Facilities)
        {
            if (Hanger && Hanger->FacilityDefinition && Hanger->FacilityDefinition->FacilityType == EFacilityType::Hanger)
            {
                if (Hanger->ParkedVehicles.Num() < Hanger->FacilityDefinition->Capacity)
                {
                    Hanger->ParkedVehicles.Add(Vehicle);
                    Vehicle->CurrentHanger = Hanger;
                    Vehicle->CurrentMission = nullptr;
                    Parked = true;
                    break;
                }
            }
        }
        if (!Parked)
        {
            UE_LOG(LogTemp, Warning, TEXT("[MISSION] ⚠️ No hanger space for returning vehicle '%s'"), *Vehicle->VehicleDefinition->VehicleName.ToString());
        }
    }

    Mission->Status = EMissionStatus::Completed;

    UE_LOG(LogTemp, Display, TEXT("[MISSION] 🚀 Mission from '%s' RETURNED → %s | +%d 💰 +%d 📦 | %d soldiers KIA | %d vehicles lost"),
        *Mission->OriginBase->BaseName.ToString(),
        *OutcomeStr,
        Mission->ResourcesGained.Money,
        Mission->ResourcesGained.Supplies,
        Mission->SoldiersKilled,
        Mission->VehiclesLost);

    OnMissionCompleted.Broadcast(Mission);
}

// (StartMission + LaunchMissionFromBase unchanged — just kept for completeness)
UMissionGroup* UMissionManagerSubsystem::StartMission(UStrategyBase* OriginBase, TArray<UStrategyVehicle*> Vehicles, int32 DurationDays)
{
    if (!OriginBase || Vehicles.Num() == 0) return nullptr;

    UMissionGroup* NewMission = NewObject<UMissionGroup>();
    NewMission->OriginBase = OriginBase;
    NewMission->VehiclesInFleet = Vehicles;
    NewMission->StartDay = GetGameInstance()->GetSubsystem<UTimeManagerSubsystem>()->GetCurrentDay();
    NewMission->DurationDays = DurationDays;
    NewMission->Status = EMissionStatus::InProgress;
    NewMission->Outcome = EMissionOutcome::Success; // default until resolved

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

    // Prevent spam
    for (UMissionGroup* Existing : ActiveMissions)
    {
        if (Existing && Existing->OriginBase == OriginBase && Existing->Status == EMissionStatus::InProgress)
            return nullptr;
    }

    TArray<UStrategyVehicle*> AvailableVehicles;
    for (UStrategyFacility* Fac : OriginBase->Facilities)
    {
        if (Fac && Fac->FacilityDefinition && Fac->FacilityDefinition->FacilityType == EFacilityType::Hanger)
        {
            AvailableVehicles.Append(Fac->ParkedVehicles);
        }
    }

    if (AvailableVehicles.Num() == 0) return nullptr;

    return StartMission(OriginBase, AvailableVehicles, DurationDays);
}