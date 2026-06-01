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
    UE_LOG(LogTemp, Display, TEXT("[MISSION] Day %d — SimulateOneDay() called (ActiveMissions: %d)"), NewDay, ActiveMissions.Num());
    SimulateOneDay();
}

void UMissionManagerSubsystem::SimulateOneDay()
{
    UE_LOG(LogTemp, Display, TEXT("[MISSION] SimulateOneDay running — %d active missions"), ActiveMissions.Num());

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

UMissionGroup* UMissionManagerSubsystem::StartMission(UStrategyBase* OriginBase, TArray<UStrategyVehicle*> Vehicles, int32 DurationDays, const TArray<UStrategySoldier*>& SoldiersToAssign, EMissionType MissionType)
{
    if (!OriginBase || Vehicles.Num() == 0) return nullptr;

    UMissionGroup* NewMission = NewObject<UMissionGroup>();
    NewMission->OriginBase = OriginBase;
    NewMission->VehiclesInFleet = Vehicles;
    NewMission->StartDay = GetGameInstance()->GetSubsystem<UTimeManagerSubsystem>()->GetCurrentDay();
    NewMission->DurationDays = DurationDays;
    NewMission->Status = EMissionStatus::InProgress;
    NewMission->Outcome = EMissionOutcome::Success;
    NewMission->MissionType = MissionType;   // ← now uses the passed type

    ActiveMissions.Add(NewMission);

    USoldierManagerSubsystem* SoldierMgr = GetSoldierManager();
    if (SoldierMgr)
    {
        TArray<UStrategySoldier*> SoldiersToUse = SoldiersToAssign;
        if (SoldiersToUse.Num() == 0)
        {
            SoldiersToUse = SoldierMgr->GetRoster(EFactionType::Enemy);
        }

        int32 SoldierIndex = 0;

        for (UStrategyVehicle* Vehicle : Vehicles)
        {
            Vehicle->CurrentPassengers.Empty();
            Vehicle->CurrentMission = NewMission;

            int32 Capacity = Vehicle->VehicleDefinition ? Vehicle->VehicleDefinition->SoldierCapacity : 4;
            for (int32 i = 0; i < Capacity && SoldierIndex < SoldiersToUse.Num(); ++i)
            {
                UStrategySoldier* Soldier = SoldiersToUse[SoldierIndex++];
                Vehicle->CurrentPassengers.Add(Soldier);

                if (Soldier->HomeBarracks == nullptr)
                {
                    for (UStrategyFacility* Barracks : OriginBase->Facilities)
                    {
                        if (Barracks && Barracks->FacilityDefinition && Barracks->FacilityDefinition->FacilityType == EFacilityType::LivingQuarters)
                        {
                            Soldier->HomeBarracks = Barracks;
                            UE_LOG(LogTemp, Verbose, TEXT("[MISSION] Soldier %s → HomeBarracks assigned to %s"), *Soldier->SoldierName, *Barracks->FacilityDefinition->FacilityName.ToString());
                            break;
                        }
                    }
                }
            }

            if (Vehicle->CurrentHanger && !Vehicle->HomeHanger)
                Vehicle->HomeHanger = Vehicle->CurrentHanger;

            if (Vehicle->CurrentHanger)
            {
                Vehicle->CurrentHanger->ParkedVehicles.Remove(Vehicle);
                Vehicle->CurrentHanger = nullptr;
            }
        }
    }

    UE_LOG(LogTemp, Display, TEXT("[MISSION] Launched %s mission with %d vehicles from base '%s' (duration: %d days)"),
        *UEnum::GetValueAsString(MissionType), Vehicles.Num(), *OriginBase->BaseName.ToString(), DurationDays);

    return NewMission;
}

UMissionGroup* UMissionManagerSubsystem::LaunchMissionFromBase(UStrategyBase* OriginBase, int32 DurationDays, EMissionType MissionType)
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
        UE_LOG(LogTemp, Verbose, TEXT("[MISSION] No vehicles available in base '%s'"), *OriginBase->BaseName.ToString());
        return nullptr;
    }

    // Pass the chosen MissionType down
    return StartMission(OriginBase, AvailableVehicles, DurationDays, TArray<UStrategySoldier*>(), MissionType);
}

void UMissionManagerSubsystem::ResolveMissionOutcome(UMissionGroup* Mission)
{
    if (!Mission || !Mission->OriginBase || Mission->VehiclesInFleet.Num() == 0) return;

    float FleetEffectiveness = CalculateFleetEffectiveness(Mission);

    UE_LOG(LogTemp, Display, TEXT("[MISSION DEBUG] === MISSION RESOLVE START === Effectiveness: %.1f%% Type: %s"),
        FleetEffectiveness, *UEnum::GetValueAsString(Mission->MissionType));

    // === Force some failures for testing POWs ===
    EMissionOutcome Outcome = EMissionOutcome::Failure;
    if (FMath::RandRange(1, 100) <= 20) Outcome = EMissionOutcome::CatastrophicFailure; // occasional big loss

    Mission->Outcome = Outcome;

    // === Rewards (simplified for debug) ===
    FResourceStockpile Reward;
    Reward.Money = 800;
    Reward.Metals = 300;
    Mission->ResourcesGained = Reward;

    // === FORCE LOSSES + CAPTURES ===
    int32 TotalVehiclesLost = 0;
    TArray<UStrategySoldier*> AllLostSoldiers;
    TArray<UStrategySoldier*> CapturedSoldiers;

    UE_LOG(LogTemp, Warning, TEXT("[MISSION DEBUG] Forcing vehicle losses for POW testing"));

    for (UStrategyVehicle* Vehicle : Mission->VehiclesInFleet)
    {
        if (!Vehicle) continue;

        TotalVehiclesLost++;
        UE_LOG(LogTemp, Warning, TEXT("[MISSION] Vehicle '%s' DESTROYED (forced for testing)"),
            *Vehicle->VehicleDefinition->VehicleName.ToString());

        for (UStrategySoldier* Soldier : Vehicle->CurrentPassengers)
        {
            if (Soldier)
            {
                // 100% capture for testing
                CapturedSoldiers.Add(Soldier);
                UE_LOG(LogTemp, Warning, TEXT("[MISSION]   → Soldier %s CAPTURED! (forced)"), *Soldier->SoldierName);
            }
        }

        Vehicle->CurrentPassengers.Empty();

        if (Vehicle->CurrentHanger) Vehicle->CurrentHanger->ParkedVehicles.Remove(Vehicle);
        Vehicle->CurrentHanger = nullptr;
        Vehicle->HomeHanger = nullptr;
        Vehicle->CurrentMission = nullptr;
        Vehicle->CurrentHealth = 0;
        Vehicle->UpdateDamageStateFromHealth();
    }

    Mission->VehiclesLost = TotalVehiclesLost;
    Mission->SoldiersKilled = AllLostSoldiers.Num();

    // === POW Transfer ===
    if (CapturedSoldiers.Num() > 0 && Mission->OriginBase)
    {
        UStrategyBase* EnemyBase = Mission->OriginBase;
        EnemyBase->CapturedPrisoners.Append(CapturedSoldiers);

        UE_LOG(LogTemp, Display, TEXT("[POW] %d soldiers captured and moved to base '%s'"),
            CapturedSoldiers.Num(), *EnemyBase->BaseName.ToString());
    }

    // === Apply rewards ===
    UResourceManagerSubsystem* ResourceMgr = GetResourceManager();
    if (ResourceMgr)
    {
        ResourceMgr->AddResources(EFactionType::Enemy, Reward);
    }

    // === Return surviving soldiers (should be none in forced loss mode) ===
    for (UStrategyVehicle* Vehicle : Mission->VehiclesInFleet)
    {
        Vehicle->CurrentPassengers.Empty();
    }

    UE_LOG(LogTemp, Display, TEXT("[MISSION] %s resolved as %s — Vehicles lost: %d | Captured: %d"),
        *UEnum::GetValueAsString(Mission->MissionType), *UEnum::GetValueAsString(Outcome), TotalVehiclesLost, CapturedSoldiers.Num());

    OnMissionCompleted.Broadcast(Mission);
}

float UMissionManagerSubsystem::CalculateFleetEffectiveness(const UMissionGroup* Mission) const
{
    if (!Mission || Mission->VehiclesInFleet.Num() == 0) return 50.0f;

    int32 TotalAim = 0;
    int32 TotalDefense = 0;
    int32 SoldierCount = 0;
    int32 TotalVehicleOffense = 0;
    int32 TotalVehicleDefense = 0;

    for (const UStrategyVehicle* Vehicle : Mission->VehiclesInFleet)
    {
        if (!Vehicle) continue;

        for (const UStrategySoldier* Soldier : Vehicle->CurrentPassengers)
        {
            if (!Soldier) continue;
            FSoldierStats Stats = Soldier->GetEffectiveStats();
            TotalAim += Stats.Aim;
            TotalDefense += Stats.Defense;
            SoldierCount++;
        }

        TotalVehicleOffense += Vehicle->GetVehicleOffensiveRating();
        TotalVehicleDefense += Vehicle->GetVehicleDefensiveRating();
    }

    if (SoldierCount == 0) return 40.0f;

    float AvgAim = (float)TotalAim / SoldierCount;
    float AvgDefense = (float)TotalDefense / SoldierCount;

    return FMath::Clamp(AvgAim * 0.6f + AvgDefense * 0.4f + TotalVehicleOffense * 0.4f + TotalVehicleDefense * 0.3f + 30.0f, 10.0f, 95.0f);
}

UResourceManagerSubsystem* UMissionManagerSubsystem::GetResourceManager() const { return GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>(); }
USoldierManagerSubsystem* UMissionManagerSubsystem::GetSoldierManager() const { return GetGameInstance()->GetSubsystem<USoldierManagerSubsystem>(); }