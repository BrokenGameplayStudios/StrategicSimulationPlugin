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

    // === 1. Calculate fleet effectiveness (soldier loadouts now matter!) ===
    float FleetEffectiveness = CalculateFleetEffectiveness(Mission);

    // === 2. Determine simulation path based on MissionType ===
    bool bHadTransitIntercept = false;
    bool bAirBattleOccurred = false;

    switch (Mission->MissionType)
    {
    case EMissionType::Interception:
        // Pure vehicle-to-vehicle encounter — always air battle
        bAirBattleOccurred = true;
        UE_LOG(LogTemp, Display, TEXT("[MISSION] Interception mission — direct air battle"));
        break;

    case EMissionType::Defensive:
    case EMissionType::Offensive:
        // 30% chance of being intercepted in transit
        if (FMath::RandRange(1, 100) <= 30)
        {
            bHadTransitIntercept = true;
            bAirBattleOccurred = true;
            UE_LOG(LogTemp, Display, TEXT("[MISSION] %s mission — intercepted in transit! Air battle triggered"),
                *UEnum::GetValueAsString(Mission->MissionType));
        }
        else
        {
            UE_LOG(LogTemp, Display, TEXT("[MISSION] %s mission — reached target location (no intercept)"),
                *UEnum::GetValueAsString(Mission->MissionType));
        }
        break;
    }

    // === 3. Simulate combat & determine outcome probabilities (weighted by effectiveness) ===
    const int32 Roll = FMath::RandRange(1, 100);
    float SuccessChance = FMath::Clamp(FleetEffectiveness * 0.8f + 20.0f, 30.0f, 95.0f); // base 20% + effectiveness boost

    EMissionOutcome Outcome;
    if (Roll <= SuccessChance)                  Outcome = EMissionOutcome::Success;
    else if (Roll <= SuccessChance + 25.0f)    Outcome = EMissionOutcome::PartialSuccess;
    else if (Roll <= SuccessChance + 45.0f)    Outcome = EMissionOutcome::Failure;
    else                                        Outcome = EMissionOutcome::CatastrophicFailure;

    Mission->Outcome = Outcome;

    // === 4. Generate rewards with requested resource bias ===
    FResourceStockpile Reward;
    switch (Outcome)
    {
    case EMissionOutcome::Success:
        Reward.Money = FMath::RandRange(1200, 2500);
        Reward.Metals = FMath::RandRange(800, 1600);
        Reward.Biologicals = FMath::RandRange(300, 700);
        Reward.Chemicals = FMath::RandRange(200, 500);
        Reward.Supplies = FMath::RandRange(400, 900);
        break;
    case EMissionOutcome::PartialSuccess:
        Reward.Money = FMath::RandRange(600, 1400);
        Reward.Metals = FMath::RandRange(400, 900);
        Reward.Biologicals = FMath::RandRange(150, 400);
        Reward.Chemicals = FMath::RandRange(100, 300);
        Reward.Supplies = FMath::RandRange(200, 500);
        break;
    case EMissionOutcome::Failure:
        Reward.Money = FMath::RandRange(100, 600);
        Reward.Metals = FMath::RandRange(100, 400);
        Reward.Biologicals = FMath::RandRange(50, 150);
        Reward.Chemicals = FMath::RandRange(30, 100);
        Reward.Supplies = FMath::RandRange(0, 200);
        break;
    case EMissionOutcome::CatastrophicFailure:
        Reward.Money = FMath::RandRange(-800, -200);
        Reward.Metals = FMath::RandRange(-300, -50);
        Reward.Biologicals = FMath::RandRange(-150, -20);
        Reward.Chemicals = FMath::RandRange(-100, -10);
        Reward.Supplies = FMath::RandRange(-400, -100);
        break;
    }

    Mission->ResourcesGained = Reward;

    // === Detailed reward logging (restored + new granular resources) ===
    UE_LOG(LogTemp, Display, TEXT("[MISSION] Rewards Gained — Money:%d | Supplies:%d | Metals:%d | Biologicals:%d | Chemicals:%d | Exotic:%d | Research:%d"),
        Reward.Money, Reward.Supplies, Reward.Metals, Reward.Biologicals, Reward.Chemicals,
        Reward.ExoticMaterial, Reward.ResearchPoints);

    // === 5. Per-vehicle losses (core of your request #1 & #2) ===
    int32 TotalVehiclesLost = 0;
    TArray<UStrategySoldier*> AllLostSoldiers;

    for (UStrategyVehicle* Vehicle : Mission->VehiclesInFleet)
    {
        if (!Vehicle) continue;

        float VehicleSurvivalChance = FleetEffectiveness * 0.7f + (Vehicle->CurrentHealth / 2.0f); // health helps

        if (FMath::RandRange(1, 100) > VehicleSurvivalChance)
        {
            // === VEHICLE LOST ===
            TotalVehiclesLost++;
            UE_LOG(LogTemp, Display, TEXT("[MISSION] Vehicle '%s' DESTROYED — all passengers and equipment lost!"),
                *Vehicle->VehicleDefinition->VehicleName.ToString());

            // All soldiers on this ship are lost
            for (UStrategySoldier* Soldier : Vehicle->CurrentPassengers)
            {
                if (Soldier)
                {
                    AllLostSoldiers.Add(Soldier);
                    UE_LOG(LogTemp, Warning, TEXT("[MISSION]   → Soldier %s KIA with full loadout"), *Soldier->SoldierName);
                }
            }

            // Clear passengers (they're dead)
            Vehicle->CurrentPassengers.Empty();

            // Destroy vehicle
            if (Vehicle->CurrentHanger) Vehicle->CurrentHanger->ParkedVehicles.Remove(Vehicle);
            Vehicle->CurrentHanger = nullptr;
            Vehicle->HomeHanger = nullptr;
            Vehicle->CurrentMission = nullptr;
            Vehicle->CurrentHealth = 0;
            Vehicle->UpdateDamageStateFromHealth(); // assumes this exists
        }
        else
        {
            // Vehicle survived — apply light damage
            int32 Damage = (Outcome == EMissionOutcome::CatastrophicFailure) ? 60 :
                (Outcome == EMissionOutcome::Failure) ? 35 : 15;
            Vehicle->ApplyDamage(Damage);
            Vehicle->CurrentMission = nullptr;
        }
    }

    Mission->VehiclesLost = TotalVehiclesLost;
    Mission->SoldiersKilled = AllLostSoldiers.Num();

    // === 6. Apply rewards ===
    UResourceManagerSubsystem* ResourceMgr = GetResourceManager();
    if (ResourceMgr)
    {
        ResourceMgr->AddResources(EFactionType::Enemy, Reward); // or Human if you expose player faction later
    }

    // === 7. Return surviving soldiers (keeps your excellent existing logic) ===
    for (UStrategyVehicle* Vehicle : Mission->VehiclesInFleet)
    {
        for (UStrategySoldier* Soldier : Vehicle->CurrentPassengers)
        {
            if (!Soldier || AllLostSoldiers.Contains(Soldier)) continue;

            Soldier->StationedBase = Mission->OriginBase;
            Soldier->CurrentMission = nullptr;

            // (Your original HomeBarracks / ParkedSoldiers logic continues here — unchanged)
            if (Soldier->HomeBarracks == nullptr)
            {
                for (UStrategyFacility* Barracks : Mission->OriginBase->Facilities)
                {
                    if (Barracks && Barracks->FacilityDefinition && Barracks->FacilityDefinition->FacilityType == EFacilityType::LivingQuarters)
                    {
                        Soldier->HomeBarracks = Barracks;
                        break;
                    }
                }
            }

            if (Soldier->HomeBarracks)
            {
                TArray<UStrategySoldier*>& List = Soldier->HomeBarracks->ParkedSoldiers;
                List.Remove(Soldier);
                if (List.Num() < Soldier->HomeBarracks->FacilityDefinition->Capacity)
                {
                    List.Add(Soldier);
                }
                else if (List.Num() > 0)
                {
                    UStrategySoldier* Evicted = List.Last();
                    List.RemoveAt(List.Num() - 1);
                    List.Add(Soldier);
                }
            }
        }
        Vehicle->CurrentPassengers.Empty(); // prevent double-processing
    }

    UE_LOG(LogTemp, Display, TEXT("[MISSION] %s resolved as %s — Effectiveness: %.1f%% | Vehicles lost: %d | Soldiers KIA: %d"),
        *UEnum::GetValueAsString(Mission->MissionType), *UEnum::GetValueAsString(Outcome), FleetEffectiveness, TotalVehiclesLost, Mission->SoldiersKilled);

    // Broadcast completion
    OnMissionCompleted.Broadcast(Mission);
}

float UMissionManagerSubsystem::CalculateFleetEffectiveness(const UMissionGroup* Mission) const
{
    if (!Mission || Mission->VehiclesInFleet.Num() == 0) return 50.0f;

    int32 TotalAim = 0;
    int32 TotalDefense = 0;
    int32 SoldierCount = 0;

    for (const UStrategyVehicle* Vehicle : Mission->VehiclesInFleet)
    {
        if (!Vehicle) continue;

        // Vehicle health bonus
        float VehicleFactor = FMath::Clamp(Vehicle->CurrentHealth / 100.0f, 0.3f, 1.0f);

        for (const UStrategySoldier* Soldier : Vehicle->CurrentPassengers)
        {
            if (!Soldier) continue;
            FSoldierStats Stats = Soldier->GetEffectiveStats();
            TotalAim += Stats.Aim;
            TotalDefense += Stats.Defense;
            SoldierCount++;
        }

        // NEW: Vehicle weapons now boost fleet effectiveness
        int32 WeaponBonus = Vehicle->GetTotalWeaponBonus();
        // Add to the final return value later
    }

    if (SoldierCount == 0) return 40.0f;

    float AvgAim = (float)TotalAim / SoldierCount;
    float AvgDefense = (float)TotalDefense / SoldierCount;

    // Final score now includes vehicle weapons
    return FMath::Clamp(AvgAim * 0.6f + AvgDefense * 0.4f + WeaponBonus * 0.3f + 30.0f, 10.0f, 95.0f);
}

UResourceManagerSubsystem* UMissionManagerSubsystem::GetResourceManager() const { return GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>(); }
USoldierManagerSubsystem* UMissionManagerSubsystem::GetSoldierManager() const { return GetGameInstance()->GetSubsystem<USoldierManagerSubsystem>(); }