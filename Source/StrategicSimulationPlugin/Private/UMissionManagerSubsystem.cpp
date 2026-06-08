#include "UMissionManagerSubsystem.h"
#include "UStrategyVehicle.h"
#include "UStrategyBase.h"
#include "UStrategyFacility.h"
#include "UTimeManagerSubsystem.h"
#include "UResourceManagerSubsystem.h"
#include "USoldierManagerSubsystem.h"
#include "UStrategySoldier.h"
#include "UStrategyCampaignSubsystem.h"
#include "UBaseManagerSubsystem.h"
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

// ===========================================================================
// NEW HELPER: GetCurrentGameHours (used by live movement)
// ===========================================================================
float UMissionManagerSubsystem::GetCurrentGameHours() const
{
    UTimeManagerSubsystem* TimeMgr = GetGameInstance()->GetSubsystem<UTimeManagerSubsystem>();
    if (!TimeMgr)
        return 0.0f;

    // Simple but accurate enough for our live system: total days * 24 + current hour
    FDateTime CurrentDate = TimeMgr->GetCurrentGameDate();
    return (TimeMgr->GetTotalSimulationDays() * 24.0f) + CurrentDate.GetHour();
}

// ===========================================================================
// NEW: Activate live movement on every vehicle in the fleet
// ===========================================================================
void UMissionManagerSubsystem::ActivateLiveMovementForVehicles(const TArray<UStrategyVehicle*>& Vehicles, EMissionType MissionType)
{
    float CurrentHours = GetCurrentGameHours();

    for (UStrategyVehicle* Vehicle : Vehicles)
    {
        if (!Vehicle || !Vehicle->HomeBase) continue;

        // For now we pick a random target somewhere on the 1920x1080 map
        // (later we'll replace this with zone-based targets)
        FVector2D TargetLocation(
            FMath::RandRange(200.0f, 1720.0f),
            FMath::RandRange(200.0f, 880.0f)
        );

        // Launch the live scouting system (3-hour search time at target is a good default)
        Vehicle->LaunchScoutingMission(TargetLocation, CurrentHours, 3.0f);

        UE_LOG(LogTemp, Display, TEXT("[LIVE MISSION] Activated live movement for %s (target: %.0f,%.0f)"),
            *Vehicle->VehicleDefinition->VehicleName.ToString(), TargetLocation.X, TargetLocation.Y);
    }
}

// Updated StartMission — now also activates the new live movement system
UMissionGroup* UMissionManagerSubsystem::StartMission(UStrategyBase* OriginBase, TArray<UStrategyVehicle*> Vehicles, int32 DurationDays, const TArray<UStrategySoldier*>& SoldiersToAssign, EMissionType MissionType, EFactionType AttackingFaction /*= EFactionType::Enemy*/)
{
    if (!OriginBase || Vehicles.Num() == 0) return nullptr;

    UMissionGroup* NewMission = NewObject<UMissionGroup>();
    NewMission->OriginBase = OriginBase;
    NewMission->VehiclesInFleet = Vehicles;
    NewMission->StartDay = GetGameInstance()->GetSubsystem<UTimeManagerSubsystem>()->GetCurrentDay();
    NewMission->DurationDays = DurationDays;
    NewMission->Status = EMissionStatus::InProgress;
    NewMission->Outcome = EMissionOutcome::Success;
    NewMission->MissionType = MissionType;
    NewMission->AttackingFaction = AttackingFaction;

    ActiveMissions.Add(NewMission);

    USoldierManagerSubsystem* SoldierMgr = GetSoldierManager();
    if (SoldierMgr)
    {
        TArray<UStrategySoldier*> SoldiersToUse = SoldiersToAssign;
        if (SoldiersToUse.Num() == 0)
        {
            SoldiersToUse = SoldierMgr->GetRoster(AttackingFaction);
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

    UE_LOG(LogTemp, Display, TEXT("[MISSION] Launched %s mission for faction %s with %d vehicles from base '%s' (duration: %d days)"),
        *UEnum::GetValueAsString(MissionType), *UEnum::GetValueAsString(AttackingFaction), Vehicles.Num(), *OriginBase->BaseName.ToString(), DurationDays);

    // === NEW: Activate live movement + radar pings for every vehicle ===
    ActivateLiveMovementForVehicles(Vehicles, MissionType);

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

    // AI missions default to Enemy faction
    return StartMission(OriginBase, AvailableVehicles, DurationDays, TArray<UStrategySoldier*>(), MissionType, EFactionType::Enemy);
}

void UMissionManagerSubsystem::ResolveMissionOutcome(UMissionGroup* Mission)
{
    if (!Mission || !Mission->OriginBase || Mission->VehiclesInFleet.Num() == 0)
        return;

    UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();
    USoldierManagerSubsystem* SoldierMgr = GetGameInstance()->GetSubsystem<USoldierManagerSubsystem>();

    if (!Campaign || !SoldierMgr)
        return;

    float FleetEffectiveness = CalculateFleetEffectiveness(Mission);

    bool bIsRecon = (Mission->MissionType == EMissionType::Recon);

    // === Mission type logging ===
    if (bIsRecon)
    {
        UE_LOG(LogTemp, Display, TEXT("[RECON] Recon mission launched from base '%s'"), *Mission->OriginBase->BaseName.ToString());
    }
    else
    {
        switch (Mission->MissionType)
        {
        case EMissionType::Interception:
            UE_LOG(LogTemp, Display, TEXT("[MISSION] Interception mission — direct air battle"));
            break;
        case EMissionType::Defensive:
        case EMissionType::Offensive:
            if (FMath::RandRange(1, 100) <= 30)
            {
                UE_LOG(LogTemp, Display, TEXT("[MISSION] %s mission — intercepted in transit!"), *UEnum::GetValueAsString(Mission->MissionType));
            }
            else
            {
                UE_LOG(LogTemp, Display, TEXT("[MISSION] %s mission — reached target location"), *UEnum::GetValueAsString(Mission->MissionType));
            }
            break;
        }
    }

    // === Outcome roll (Recon is intentionally safer) ===
    const int32 Roll = FMath::RandRange(1, 100);
    float SuccessChance = bIsRecon ? 75.0f : FMath::Clamp(FleetEffectiveness * 0.8f + 20.0f, 40.0f, 85.0f);

    EMissionOutcome Outcome;
    if (Roll <= SuccessChance)
        Outcome = EMissionOutcome::Success;
    else if (Roll <= SuccessChance + 25.0f)
        Outcome = EMissionOutcome::PartialSuccess;
    else if (Roll <= SuccessChance + 45.0f)
        Outcome = EMissionOutcome::Failure;
    else
        Outcome = EMissionOutcome::CatastrophicFailure;

    Mission->Outcome = Outcome;

    // === Rewards ===
    FResourceStockpile Reward;
    if (bIsRecon)
    {
        // Recon gives intel instead of combat loot
        Reward.ResearchPoints = FMath::RandRange(400, 900);
        Reward.Money = FMath::RandRange(300, 700);
        UE_LOG(LogTemp, Display, TEXT("[RECON] Success — gained intel and discovered new map location"));
    }
    else
    {
        // Your original reward logic
        switch (Outcome)
        {
        case EMissionOutcome::Success:
            Reward.Money = FMath::RandRange(1200, 2500);
            Reward.Metals = FMath::RandRange(800, 1600);
            Reward.Biologicals = FMath::RandRange(300, 700);
            Reward.Chemicals = FMath::RandRange(200, 500);
            break;
        case EMissionOutcome::PartialSuccess:
            Reward.Money = FMath::RandRange(600, 1400);
            Reward.Metals = FMath::RandRange(400, 900);
            Reward.Biologicals = FMath::RandRange(150, 400);
            Reward.Chemicals = FMath::RandRange(100, 300);
            break;
        case EMissionOutcome::Failure:
            Reward.Money = FMath::RandRange(100, 600);
            Reward.Metals = FMath::RandRange(100, 400);
            Reward.Biologicals = FMath::RandRange(50, 150);
            Reward.Chemicals = FMath::RandRange(30, 100);
            break;
        case EMissionOutcome::CatastrophicFailure:
            Reward.Money = FMath::RandRange(-800, -200);
            Reward.Metals = FMath::RandRange(-300, -50);
            Reward.Biologicals = FMath::RandRange(-150, -20);
            Reward.Chemicals = FMath::RandRange(-100, -10);
            break;
        }
    }

    Mission->ResourcesGained = Reward;

    EFactionType Attacker = Mission->AttackingFaction;
    EFactionType Defender = (Attacker == EFactionType::Human) ? EFactionType::Enemy : EFactionType::Human;

    // === Vehicle & Soldier losses + return logic (unchanged except Recon safety) ===
    int32 TotalVehiclesLost = 0;
    TArray<UStrategySoldier*> AllLostSoldiers;
    int32 TotalCaptured = 0;

    for (UStrategyVehicle* Vehicle : Mission->VehiclesInFleet)
    {
        if (!Vehicle) continue;

        float SurvivalChance = bIsRecon ? 90.0f : (FleetEffectiveness * 0.7f + (Vehicle->CurrentHealth / 2.0f));

        if (FMath::RandRange(1, 100) > SurvivalChance)
        {
            TotalVehiclesLost++;
            UE_LOG(LogTemp, Display, TEXT("[MISSION] Vehicle '%s' DESTROYED"), *Vehicle->VehicleDefinition->VehicleName.ToString());

            for (UStrategySoldier* Soldier : Vehicle->CurrentPassengers)
            {
                if (!Soldier) continue;

                bool bCaptured = false;
                if (Outcome == EMissionOutcome::Failure || Outcome == EMissionOutcome::CatastrophicFailure)
                    bCaptured = (FMath::RandRange(0.0f, 1.0f) <= Campaign->EnemyPOWCaptureChanceOnDefeat);
                else if (Outcome == EMissionOutcome::PartialSuccess)
                    bCaptured = FMath::RandRange(0.0f, 1.0f) <= 0.40f;
                else
                    bCaptured = FMath::RandRange(0.0f, 1.0f) <= 0.10f;

                if (bCaptured)
                {
                    SoldierMgr->CaptureAsPOW(Defender, Soldier);
                    TotalCaptured++;
                }
                else
                {
                    SoldierMgr->MarkAsKIA(Attacker, Soldier);
                    AllLostSoldiers.Add(Soldier);
                }
            }

            Vehicle->CurrentPassengers.Empty();
            if (Vehicle->CurrentHanger)
                Vehicle->CurrentHanger->ParkedVehicles.Remove(Vehicle);

            Vehicle->CurrentHanger = nullptr;
            Vehicle->HomeHanger = nullptr;
            Vehicle->CurrentMission = nullptr;
            Vehicle->CurrentHealth = 0;
            Vehicle->UpdateDamageStateFromHealth();
        }
        else
        {
            // Surviving vehicle returns home
            int32 Damage = (Outcome == EMissionOutcome::CatastrophicFailure) ? 60 :
                (Outcome == EMissionOutcome::Failure) ? 35 : (bIsRecon ? 5 : 15);
            Vehicle->ApplyDamage(Damage);

            if (Vehicle->HomeHanger)
            {
                Vehicle->CurrentHanger = Vehicle->HomeHanger;
                Vehicle->HomeHanger->ParkedVehicles.AddUnique(Vehicle);
            }
            Vehicle->CurrentMission = nullptr;
        }
    }

    Mission->VehiclesLost = TotalVehiclesLost;
    Mission->SoldiersKilled = AllLostSoldiers.Num();

    // === Victory-side POW/KIA (Recon has lower chance) ===
    if (Outcome == EMissionOutcome::Success || Outcome == EMissionOutcome::PartialSuccess)
    {
        int32 NumToProcess = bIsRecon ? FMath::RandRange(0, 2) : FMath::RandRange(1, 4);
        for (int32 i = 0; i < NumToProcess; ++i)
        {
            if (FMath::RandRange(0.0f, 1.0f) <= Campaign->POWCaptureChanceOnVictory)
            {
                const TArray<UStrategySoldier*>& DefenderRoster = SoldierMgr->GetRoster(Defender);
                if (!DefenderRoster.IsEmpty())
                {
                    UStrategySoldier* Victim = DefenderRoster[FMath::RandRange(0, DefenderRoster.Num() - 1)];
                    if (Victim && !Victim->bIsPOW)
                    {
                        SoldierMgr->CaptureAsPOW(Attacker, Victim);
                        if (Mission->OriginBase)
                            Mission->OriginBase->AddPOW(Victim);
                    }
                }
            }

            if (FMath::RandRange(0.0f, 1.0f) <= Campaign->KIAChanceOnVictory)
            {
                const TArray<UStrategySoldier*>& DefenderRoster = SoldierMgr->GetRoster(Defender);
                if (!DefenderRoster.IsEmpty())
                {
                    UStrategySoldier* Victim = DefenderRoster[FMath::RandRange(0, DefenderRoster.Num() - 1)];
                    if (Victim)
                    {
                        SoldierMgr->MarkAsKIA(Attacker, Victim);
                        if (Mission->OriginBase)
                            Mission->OriginBase->AddKIABody(Victim);
                    }
                }
            }
        }
    }

    // === Apply rewards ===
    UResourceManagerSubsystem* ResourceMgr = GetResourceManager();
    if (ResourceMgr)
    {
        ResourceMgr->AddResources(Mission->AttackingFaction, Reward);
    }

    // Clean up
    for (UStrategyVehicle* Vehicle : Mission->VehiclesInFleet)
    {
        Vehicle->CurrentPassengers.Empty();
    }

    UE_LOG(LogTemp, Display, TEXT("[MISSION] %s resolved as %s — Effectiveness: %.1f%% | Vehicles lost: %d | KIA: %d | Captured: %d"),
        *UEnum::GetValueAsString(Mission->MissionType), *UEnum::GetValueAsString(Outcome), FleetEffectiveness,
        TotalVehiclesLost, Mission->SoldiersKilled, TotalCaptured);

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