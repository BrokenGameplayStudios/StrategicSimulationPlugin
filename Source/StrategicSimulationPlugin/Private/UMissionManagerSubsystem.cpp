#include "UMissionManagerSubsystem.h"
#include "UStrategyVehicle.h"
#include "UStrategyBase.h"
#include "UStrategyFacility.h"
#include "UTimeManagerSubsystem.h"
#include "UResourceManagerSubsystem.h"
#include "USoldierManagerSubsystem.h"
#include "UStrategySoldier.h"
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

    const int32 Roll = FMath::RandRange(1, 100);
    if (Roll <= 55)      Mission->Outcome = EMissionOutcome::Success;
    else if (Roll <= 80) Mission->Outcome = EMissionOutcome::PartialSuccess;
    else if (Roll <= 95) Mission->Outcome = EMissionOutcome::Failure;
    else                 Mission->Outcome = EMissionOutcome::CatastrophicFailure;

    FString OutcomeStr;
    FResourceStockpile Reward;
    int32 SoldiersLost = 0;
    int32 VehiclesLostThisMission = 0;
    int32 BaseDamagePerVehicle = 0;

    switch (Mission->Outcome)
    {
    case EMissionOutcome::Success:         OutcomeStr = TEXT("SUCCESS");        Reward.Money = FMath::RandRange(1500, 2800); Reward.Supplies = FMath::RandRange(800, 1600); SoldiersLost = FMath::RandRange(0, 2); BaseDamagePerVehicle = 0; break;
    case EMissionOutcome::PartialSuccess:  OutcomeStr = TEXT("PARTIAL SUCCESS"); Reward.Money = FMath::RandRange(700, 1500); Reward.Supplies = FMath::RandRange(400, 900);  SoldiersLost = FMath::RandRange(1, 4); BaseDamagePerVehicle = 15; break;
    case EMissionOutcome::Failure:         OutcomeStr = TEXT("FAILURE");        Reward.Money = FMath::RandRange(0, 500);   Reward.Supplies = FMath::RandRange(0, 300);   SoldiersLost = FMath::RandRange(3, 6); BaseDamagePerVehicle = 35; break;
    case EMissionOutcome::CatastrophicFailure: OutcomeStr = TEXT("CATASTROPHIC FAILURE"); Reward.Money = -FMath::RandRange(400, 1200); Reward.Supplies = -FMath::RandRange(300, 800); SoldiersLost = FMath::RandRange(5, 10); VehiclesLostThisMission = FMath::RandRange(0, 2); BaseDamagePerVehicle = 70; break;
    }

    Mission->ResourcesGained = Reward;
    Mission->SoldiersKilled = SoldiersLost;
    Mission->VehiclesLost = VehiclesLostThisMission;

    UResourceManagerSubsystem* ResourceMgr = GetResourceManager();
    USoldierManagerSubsystem* SoldierMgr = GetSoldierManager();

    if (ResourceMgr) ResourceMgr->AddResources(EFactionType::Enemy, Reward);

    // === SOLDIER WOUNDING (this is the block that was missing/broken) ===
    if (SoldierMgr && SoldiersLost > 0)
    {
        TArray<UStrategySoldier*> AllPassengers;
        for (UStrategyVehicle* Veh : Mission->VehiclesInFleet)
        {
            AllPassengers.Append(Veh->CurrentPassengers);
        }

        UE_LOG(LogTemp, Display, TEXT("[MISSION] Wounding up to %d soldiers (found %d passengers)"), SoldiersLost, AllPassengers.Num());

        for (int32 i = 0; i < SoldiersLost && AllPassengers.Num() > 0; ++i)
        {
            int32 idx = FMath::RandRange(0, AllPassengers.Num() - 1);
            UStrategySoldier* Soldier = AllPassengers[idx];

            int32 WoundDamage = FMath::RandRange(4, 8);
            Soldier->ApplyDamage(WoundDamage);

            UE_LOG(LogTemp, Display, TEXT("[MISSION] Soldier %s wounded (%d damage)"), *Soldier->SoldierName, WoundDamage);
            AllPassengers.RemoveAt(idx);
        }
    }

    int32 VehiclesToDestroy = VehiclesLostThisMission;
    for (UStrategyVehicle* Vehicle : Mission->VehiclesInFleet)
    {
        if (!Vehicle) continue;

        if (VehiclesToDestroy > 0)
        {
            VehiclesToDestroy--;
            if (Vehicle->CurrentHanger) Vehicle->CurrentHanger->ParkedVehicles.Remove(Vehicle);
            Vehicle->CurrentHanger = nullptr;
            Vehicle->CurrentMission = nullptr;
            Vehicle->HomeHanger = nullptr;
            UE_LOG(LogTemp, Warning, TEXT("[MISSION] Vehicle '%s' was DESTROYED — slot freed"), *Vehicle->VehicleDefinition->VehicleName.ToString());
            continue;
        }

        if (BaseDamagePerVehicle > 0)
        {
            Vehicle->ApplyDamage(BaseDamagePerVehicle);
        }

        Vehicle->CurrentMission = nullptr;

        // Force return to owned HomeHanger
        bool Parked = false;
        if (Vehicle->HomeHanger && Vehicle->HomeHanger->FacilityDefinition &&
            Vehicle->HomeHanger->FacilityDefinition->FacilityType == EFacilityType::Hanger)
        {
            if (Vehicle->HomeHanger->ParkedVehicles.Num() < Vehicle->HomeHanger->FacilityDefinition->Capacity)
            {
                Vehicle->HomeHanger->ParkedVehicles.Add(Vehicle);
            }
            else
            {
                if (Vehicle->HomeHanger->ParkedVehicles.Num() > 0)
                {
                    UStrategyVehicle* Evicted = Vehicle->HomeHanger->ParkedVehicles.Last();
                    UE_LOG(LogTemp, Display, TEXT("[MISSION] HomeHanger full — evicting %s to reclaim slot for %s"),
                        *Evicted->VehicleDefinition->VehicleName.ToString(), *Vehicle->VehicleDefinition->VehicleName.ToString());
                    Vehicle->HomeHanger->ParkedVehicles.RemoveAt(Vehicle->HomeHanger->ParkedVehicles.Num() - 1);
                }
                Vehicle->HomeHanger->ParkedVehicles.Add(Vehicle);
            }
            Vehicle->CurrentHanger = Vehicle->HomeHanger;
            Parked = true;
        }

        if (Parked)
        {
            if (Vehicle->NeedsRepair())
            {
                UE_LOG(LogTemp, Display, TEXT("[MISSION] %s returned damaged and parked in its reserved HOME HANGER — repair bays will heal it"),
                    *Vehicle->VehicleDefinition->VehicleName.ToString());
            }
            else
            {
                UE_LOG(LogTemp, Display, TEXT("[MISSION] Vehicle '%s' returned undamaged to its reserved hanger"), *Vehicle->VehicleDefinition->VehicleName.ToString());
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[MISSION] %s returned but NO HANGER SPACE AVAILABLE"), *Vehicle->VehicleDefinition->VehicleName.ToString());
        }
    }

    Mission->Status = EMissionStatus::Completed;

    UE_LOG(LogTemp, Display, TEXT("[MISSION] Mission from '%s' RETURNED → %s | +%d 💰 +%d 📦 | %d soldiers KIA | %d vehicles lost"),
        *Mission->OriginBase->BaseName.ToString(),
        *OutcomeStr,
        Mission->ResourcesGained.Money,
        Mission->ResourcesGained.Supplies,
        Mission->SoldiersKilled,
        Mission->VehiclesLost);

    OnMissionCompleted.Broadcast(Mission);
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
    NewMission->Outcome = EMissionOutcome::Success;

    ActiveMissions.Add(NewMission);

    // === AUTO-ASSIGN SOLDIERS TO VEHICLES (this was missing) ===
    USoldierManagerSubsystem* SoldierMgr = GetSoldierManager();
    if (SoldierMgr)
    {
        TArray<UStrategySoldier*> AvailableSoldiers = SoldierMgr->GetRoster(EFactionType::Enemy);
        int32 SoldierIndex = 0;

        for (UStrategyVehicle* Vehicle : Vehicles)
        {
            Vehicle->CurrentPassengers.Empty(); // clear old passengers
            Vehicle->CurrentMission = NewMission;

            // Assign up to vehicle capacity
            int32 Capacity = Vehicle->VehicleDefinition ? Vehicle->VehicleDefinition->SoldierCapacity : 4;
            for (int32 i = 0; i < Capacity && SoldierIndex < AvailableSoldiers.Num(); ++i)
            {
                UStrategySoldier* Soldier = AvailableSoldiers[SoldierIndex++];
                Vehicle->CurrentPassengers.Add(Soldier);
                Soldier->StationedBase = OriginBase; // ensure they know where they belong
            }

            // Reserve HomeHanger
            if (Vehicle->CurrentHanger && !Vehicle->HomeHanger)
            {
                Vehicle->HomeHanger = Vehicle->CurrentHanger;
            }

            if (Vehicle->CurrentHanger)
            {
                Vehicle->CurrentHanger->ParkedVehicles.Remove(Vehicle);
                Vehicle->CurrentHanger = nullptr;
            }
        }
    }

    UE_LOG(LogTemp, Display, TEXT("[MISSION] Launched mission with %d vehicles from base '%s' (duration: %d days) — slots reserved"),
        Vehicles.Num(), *OriginBase->BaseName.ToString(), DurationDays);

    return NewMission;
}

UMissionGroup* UMissionManagerSubsystem::LaunchMissionFromBase(UStrategyBase* OriginBase, int32 DurationDays)
{
    if (!OriginBase) return nullptr;

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
        // UE_LOG(LogTemp, Warning, TEXT("[MISSION] No vehicles available in base '%s'"), *OriginBase->BaseName.ToString());
        return nullptr;
    }

    return StartMission(OriginBase, AvailableVehicles, DurationDays);
}

UResourceManagerSubsystem* UMissionManagerSubsystem::GetResourceManager() const { return GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>(); }
USoldierManagerSubsystem* UMissionManagerSubsystem::GetSoldierManager() const { return GetGameInstance()->GetSubsystem<USoldierManagerSubsystem>(); }