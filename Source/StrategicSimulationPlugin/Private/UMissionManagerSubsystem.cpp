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
#include "StrategicSiteDefinition.h"
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
    TArray<UMissionGroup*> ToRemove;

    for (UMissionGroup* Mission : ActiveMissions)
    {
        if (!Mission || Mission->Status != EMissionStatus::InProgress) continue;

        if (Mission->bIsLiveMovement)
            continue;

        Mission->DurationDays--;

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

float UMissionManagerSubsystem::GetCurrentGameHours() const
{
    UTimeManagerSubsystem* TimeMgr = GetGameInstance()->GetSubsystem<UTimeManagerSubsystem>();
    if (!TimeMgr)
        return 0.0f;

    FDateTime CurrentDate = TimeMgr->GetCurrentGameDate();

    float Hours = CurrentDate.GetHour();
    float Minutes = CurrentDate.GetMinute() / 60.0f;
    float Seconds = CurrentDate.GetSecond() / 3600.0f;

    float PreciseHours = Hours + Minutes + Seconds;

    return (TimeMgr->GetTotalSimulationDays() * 24.0f) + PreciseHours;
}

void UMissionManagerSubsystem::GetMapBounds(float& OutWidth, float& OutHeight, float& OutPadding) const
{
    OutWidth = 1920.0f;
    OutHeight = 1080.0f;
    OutPadding = 100.0f;
}

FVector2D UMissionManagerSubsystem::PickMissionTarget(UStrategyVehicle* Vehicle, EMissionType MissionType) const
{
    float MapWidth, MapHeight, MapPadding;
    GetMapBounds(MapWidth, MapHeight, MapPadding);

    const float MinX = MapPadding;
    const float MaxX = MapWidth - MapPadding;
    const float MinY = MapPadding;
    const float MaxY = MapHeight - MapPadding;

    auto RandomMapPoint = [&]()
    {
        return FVector2D(FMath::RandRange(MinX, MaxX), FMath::RandRange(MinY, MaxY));
    };

    if (!Vehicle || !Vehicle->HomeBase)
    {
        return RandomMapPoint();
    }

    UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
    if (!BaseMgr)
    {
        return RandomMapPoint();
    }

    const EFactionType Faction = Vehicle->HomeBase->OwningFaction;
    const EFactionType EnemyFaction = (Faction == EFactionType::Human) ? EFactionType::Enemy : EFactionType::Human;
    const FVector2D Origin = Vehicle->HomeBase->MapLocation;

    switch (MissionType)
    {
    case EMissionType::Recon:
    {
        UStrategySiteDefinition* BestSite = nullptr;
        float BestDist = MAX_FLT;

        for (UStrategySiteDefinition* Site : BaseMgr->AllPotentialSites)
        {
            if (!Site || Site->bHasBeenUsed) continue;

            const TArray<UStrategySiteDefinition*>& Discovered =
                (Faction == EFactionType::Human) ? BaseMgr->DiscoveredSitesHuman : BaseMgr->DiscoveredSitesEnemy;

            if (Discovered.Contains(Site)) continue;

            const float Dist = FVector2D::Distance(Origin, Site->Location);
            const float RoundTrip = Dist * 2.0f;
            if (Dist < BestDist && Vehicle->HasEnoughRangeForMission(RoundTrip))
            {
                BestDist = Dist;
                BestSite = Site;
            }
        }

        if (BestSite)
        {
            return BestSite->Location;
        }
        break;
    }

    case EMissionType::Offensive:
    case EMissionType::Defensive:
    {
        const TArray<UStrategyBase*>& EnemyBases = BaseMgr->GetBases(EnemyFaction);
        if (EnemyBases.Num() > 0)
        {
            UStrategyBase* TargetBase = EnemyBases[FMath::RandRange(0, EnemyBases.Num() - 1)];
            if (TargetBase)
            {
                return TargetBase->MapLocation;
            }
        }
        break;
    }

    case EMissionType::Interception:
    {
        UStrategyVehicle* NearestEnemy = nullptr;
        float NearestDist = MAX_FLT;

        for (UMissionGroup* Mission : ActiveMissions)
        {
            if (!Mission || !Mission->OriginBase) continue;
            if (Mission->OriginBase->OwningFaction != EnemyFaction) continue;

            for (UStrategyVehicle* EnemyVehicle : Mission->VehiclesInFleet)
            {
                if (!EnemyVehicle) continue;
                if (EnemyVehicle->CurrentPhase == EVehicleMissionPhase::Docked) continue;

                const float Dist = FVector2D::Distance(Origin, EnemyVehicle->CurrentPosition);
                if (Dist < NearestDist)
                {
                    NearestDist = Dist;
                    NearestEnemy = EnemyVehicle;
                }
            }
        }

        if (NearestEnemy)
        {
            return NearestEnemy->CurrentPosition;
        }

        const TArray<UStrategyBase*>& EnemyBases = BaseMgr->GetBases(EnemyFaction);
        if (EnemyBases.Num() > 0 && EnemyBases[0])
        {
            return EnemyBases[0]->MapLocation;
        }
        break;
    }

    default:
        break;
    }

    return RandomMapPoint();
}

void UMissionManagerSubsystem::ActivateLiveMovementForVehicles(UMissionGroup* Mission, EMissionType MissionType)
{
    if (!Mission) return;

    float CurrentHours = GetCurrentGameHours();

    float SearchHours = 3.0f;
    switch (MissionType)
    {
    case EMissionType::Recon:
        SearchHours = 3.0f;
        break;
    case EMissionType::Interception:
        SearchHours = 0.5f;
        break;
    case EMissionType::Offensive:
    case EMissionType::Defensive:
        SearchHours = 1.5f;
        break;
    default:
        SearchHours = 2.0f;
        break;
    }

    TArray<UStrategyVehicle*> LaunchedVehicles;

    for (UStrategyVehicle* Vehicle : Mission->VehiclesInFleet)
    {
        if (!Vehicle || !Vehicle->HomeBase) continue;

        const FVector2D TargetLocation = PickMissionTarget(Vehicle, MissionType);
        const float OutboundDist = FVector2D::Distance(Vehicle->HomeBase->MapLocation, TargetLocation);
        const float RoundTripDist = OutboundDist * 2.0f;

        if (!Vehicle->HasEnoughRangeForMission(RoundTripDist))
        {
            UE_LOG(LogTemp, Warning, TEXT("[LIVE MISSION] %s skipped — insufficient range (need %.0f, have %.0f)"),
                Vehicle->VehicleDefinition ? *Vehicle->VehicleDefinition->VehicleName.ToString() : TEXT("Vehicle"),
                RoundTripDist, Vehicle->CurrentRangeLeft);

            for (UStrategySoldier* Soldier : Vehicle->CurrentPassengers)
            {
                if (Soldier)
                {
                    Soldier->CurrentMission = nullptr;
                }
            }
            Vehicle->CurrentPassengers.Empty();
            Vehicle->CurrentMission = nullptr;
            if (Vehicle->HomeHanger)
            {
                Vehicle->CurrentHanger = Vehicle->HomeHanger;
                Vehicle->HomeHanger->ParkedVehicles.AddUnique(Vehicle);
            }
            continue;
        }

        Vehicle->CurrentRangeLeft = FMath::Max(0.0f, Vehicle->CurrentRangeLeft - RoundTripDist);
        Vehicle->BeginMissionMovement(TargetLocation, CurrentHours, SearchHours, MissionType);
        LaunchedVehicles.Add(Vehicle);

        UE_LOG(LogTemp, Display, TEXT("[LIVE MISSION] Activated %s for %s (target: %.0f,%.0f, range left: %.0f)"),
            *UEnum::GetValueAsString(MissionType),
            Vehicle->VehicleDefinition ? *Vehicle->VehicleDefinition->VehicleName.ToString() : TEXT("Vehicle"),
            TargetLocation.X, TargetLocation.Y, Vehicle->CurrentRangeLeft);
    }

    Mission->VehiclesInFleet = LaunchedVehicles;

    if (Mission->VehiclesInFleet.Num() == 0)
    {
        ActiveMissions.Remove(Mission);
        UE_LOG(LogTemp, Warning, TEXT("[LIVE MISSION] No vehicles launched — mission cancelled"));
    }
}

UMissionGroup* UMissionManagerSubsystem::StartMission(UStrategyBase* OriginBase, TArray<UStrategyVehicle*> Vehicles, int32 DurationDays, const TArray<UStrategySoldier*>& SoldiersToAssign, EMissionType MissionType, EFactionType AttackingFaction)
{
    if (!OriginBase || Vehicles.Num() == 0) return nullptr;

    UMissionGroup* NewMission = NewObject<UMissionGroup>();
    NewMission->OriginBase = OriginBase;
    NewMission->VehiclesInFleet = Vehicles;
    NewMission->StartDay = GetGameInstance()->GetSubsystem<UTimeManagerSubsystem>()->GetCurrentDay();
    NewMission->bIsLiveMovement = true;
    NewMission->DurationDays = 0;
    NewMission->Status = EMissionStatus::InProgress;
    NewMission->Outcome = EMissionOutcome::Success;
    NewMission->MissionType = MissionType;
    NewMission->AttackingFaction = AttackingFaction;

    ActiveMissions.Add(NewMission);

    UStrategyFacility* OriginHangar = OriginBase->FindFirstOperationalHangar();

    for (UStrategyVehicle* Vehicle : Vehicles)
    {
        if (!Vehicle) continue;

        Vehicle->CurrentPassengers.Empty();
        Vehicle->CurrentMission = NewMission;

        UStrategyFacility* PreviousHomeHanger = Vehicle->HomeHanger;

        Vehicle->HomeBase = OriginBase;
        if (OriginHangar)
        {
            Vehicle->HomeHanger = OriginHangar;
        }
        else if (Vehicle->CurrentHanger && !Vehicle->HomeHanger)
        {
            Vehicle->HomeHanger = Vehicle->CurrentHanger;
        }

        if (PreviousHomeHanger && PreviousHomeHanger != Vehicle->HomeHanger)
        {
            PreviousHomeHanger->ParkedVehicles.Remove(Vehicle);
        }

        if (Vehicle->CurrentHanger)
        {
            Vehicle->CurrentHanger->ParkedVehicles.Remove(Vehicle);
            Vehicle->CurrentHanger = nullptr;
        }
    }

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
            if (!Vehicle) continue;

            int32 Capacity = Vehicle->VehicleDefinition ? Vehicle->VehicleDefinition->SoldierCapacity : 4;
            for (int32 i = 0; i < Capacity && SoldierIndex < SoldiersToUse.Num(); ++i)
            {
                UStrategySoldier* Soldier = SoldiersToUse[SoldierIndex++];
                Vehicle->CurrentPassengers.Add(Soldier);
                Soldier->CurrentMission = NewMission;

                if (Soldier->HomeBarracks == nullptr)
                {
                    for (UStrategyFacility* Barracks : OriginBase->Facilities)
                    {
                        if (Barracks && Barracks->FacilityDefinition && Barracks->FacilityDefinition->FacilityType == EFacilityType::LivingQuarters)
                        {
                            Soldier->HomeBarracks = Barracks;
                            break;
                        }
                    }
                }
            }
        }
    }

    UE_LOG(LogTemp, Display, TEXT("[MISSION] Launched live %s mission for faction %s with %d vehicles from base '%s'"),
        *UEnum::GetValueAsString(MissionType), *UEnum::GetValueAsString(AttackingFaction), Vehicles.Num(), *OriginBase->BaseName.ToString());

    ActivateLiveMovementForVehicles(NewMission, MissionType);

    if (!ActiveMissions.Contains(NewMission))
    {
        return nullptr;
    }

    return NewMission;
}

void UMissionManagerSubsystem::UpdateAllLiveVehicles(float DeltaGameHours)
{
    if (DeltaGameHours <= 0.0f)
    {
        return;
    }

    float CurrentHours = GetCurrentGameHours();

    for (int32 i = ActiveMissions.Num() - 1; i >= 0; --i)
    {
        UMissionGroup* Mission = ActiveMissions[i];
        if (!Mission || Mission->Status != EMissionStatus::InProgress || !Mission->bIsLiveMovement) continue;

        bool bAllFinished = Mission->VehiclesInFleet.Num() > 0;

        for (UStrategyVehicle* Vehicle : Mission->VehiclesInFleet)
        {
            if (!Vehicle)
            {
                continue;
            }

            if (!Vehicle->IsDestroyed())
            {
                Vehicle->UpdatePositionAndPings(CurrentHours, DeltaGameHours);
            }

            if (!Vehicle->IsMissionFinished())
            {
                bAllFinished = false;
            }
        }

        if (bAllFinished)
        {
            ResolveMissionOutcome(Mission);
            ActiveMissions.RemoveAt(i);
            UE_LOG(LogTemp, Display, TEXT("[LIVE MISSION] Mission fully completed — all vehicles docked or destroyed"));
        }
    }
}

void UMissionManagerSubsystem::HandleVehicleDestroyedInCombat(UStrategyVehicle* Vehicle)
{
    if (!Vehicle || !Vehicle->IsDestroyed() || Vehicle->bWreckSalvageProcessed)
    {
        return;
    }

    Vehicle->bWreckSalvageProcessed = true;

    UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
    if (BaseMgr)
    {
        BaseMgr->CreateSalvageSite(Vehicle->CurrentPosition, Vehicle);
    }

    for (UStrategySoldier* Soldier : Vehicle->CurrentPassengers)
    {
        if (Soldier)
        {
            Soldier->CurrentMission = nullptr;
        }
    }
    Vehicle->CurrentPassengers.Empty();

    if (Vehicle->CurrentHanger)
    {
        Vehicle->CurrentHanger->ParkedVehicles.Remove(Vehicle);
    }
    Vehicle->CurrentHanger = nullptr;
    Vehicle->HomeHanger = nullptr;
    Vehicle->CurrentTargetVehicle = nullptr;
    Vehicle->CurrentBehavior = EVehicleBehavior::Idle;

    UE_LOG(LogTemp, Display, TEXT("[COMBAT] Vehicle '%s' destroyed in vehicular combat — salvage site created at (%.0f, %.0f)"),
        Vehicle->VehicleDefinition ? *Vehicle->VehicleDefinition->VehicleName.ToString() : TEXT("Unknown"),
        Vehicle->CurrentPosition.X, Vehicle->CurrentPosition.Y);
}

UMissionGroup* UMissionManagerSubsystem::LaunchMissionFromBase(UStrategyBase* OriginBase, int32 DurationDays, EMissionType MissionType, const TArray<UStrategyVehicle*>& VehiclesOverride)
{
    if (!OriginBase) return nullptr;

    TArray<UStrategyVehicle*> VehiclesToLaunch;

    if (VehiclesOverride.Num() > 0)
    {
        VehiclesToLaunch = VehiclesOverride;
    }
    else
    {
        for (UStrategyFacility* Facility : OriginBase->Facilities)
        {
            if (Facility && Facility->bIsOperational && Facility->FacilityDefinition &&
                Facility->FacilityDefinition->FacilityType == EFacilityType::Hanger)
            {
                VehiclesToLaunch.Append(Facility->ParkedVehicles);
            }
        }
    }

    if (VehiclesToLaunch.Num() == 0) return nullptr;

    return StartMission(OriginBase, VehiclesToLaunch, DurationDays, {}, MissionType, OriginBase->OwningFaction);
}

void UMissionManagerSubsystem::ResolveMissionOutcome(UMissionGroup* Mission)
{
    if (!Mission || !Mission->OriginBase || Mission->VehiclesInFleet.Num() == 0)
        return;

    const bool bIsRecon = (Mission->MissionType == EMissionType::Recon);

    int32 SurvivingVehicles = 0;
    int32 DestroyedVehicles = 0;

    for (UStrategyVehicle* Vehicle : Mission->VehiclesInFleet)
    {
        if (!Vehicle) continue;

        if (Vehicle->IsDestroyed())
        {
            DestroyedVehicles++;
            Vehicle->CurrentMission = nullptr;
            continue;
        }

        SurvivingVehicles++;

        if (Vehicle->GetMissionPhase() != EVehicleMissionPhase::Docked)
        {
            Vehicle->DockAtHomeHangar();
        }
        else if (Vehicle->HomeHanger)
        {
            Vehicle->CurrentHanger = Vehicle->HomeHanger;
            Vehicle->HomeHanger->ParkedVehicles.AddUnique(Vehicle);
        }

        for (UStrategySoldier* Soldier : Vehicle->CurrentPassengers)
        {
            if (Soldier)
            {
                Soldier->CurrentMission = nullptr;
            }
        }
        Vehicle->CurrentPassengers.Empty();
        Vehicle->CurrentMission = nullptr;
    }

    Mission->VehiclesLost = DestroyedVehicles;
    Mission->SoldiersKilled = 0;

    EMissionOutcome Outcome = EMissionOutcome::Success;
    if (DestroyedVehicles > 0 && SurvivingVehicles == 0)
    {
        Outcome = EMissionOutcome::CatastrophicFailure;
    }
    else if (DestroyedVehicles > 0)
    {
        Outcome = EMissionOutcome::PartialSuccess;
    }
    Mission->Outcome = Outcome;

    FResourceStockpile Reward;
    if (bIsRecon)
    {
        Reward.ResearchPoints = FMath::RandRange(200, 500);
        UE_LOG(LogTemp, Display, TEXT("[RECON] Mission complete — intel gathered via live radar pings"));
    }
    else
    {
        switch (Outcome)
        {
        case EMissionOutcome::Success:
            Reward.Money = FMath::RandRange(800, 1500);
            Reward.Metals = FMath::RandRange(400, 900);
            break;
        case EMissionOutcome::PartialSuccess:
            Reward.Money = FMath::RandRange(400, 900);
            Reward.Metals = FMath::RandRange(200, 500);
            break;
        case EMissionOutcome::CatastrophicFailure:
            Reward.Money = FMath::RandRange(-200, 100);
            break;
        default:
            break;
        }
    }

    Mission->ResourcesGained = Reward;

    if (UResourceManagerSubsystem* ResourceMgr = GetResourceManager())
    {
        ResourceMgr->AddResources(Mission->AttackingFaction, Reward);
    }

    UE_LOG(LogTemp, Display, TEXT("[MISSION] %s resolved as %s — Survived: %d | Combat losses: %d (no abstract casualties)"),
        *UEnum::GetValueAsString(Mission->MissionType), *UEnum::GetValueAsString(Outcome),
        SurvivingVehicles, DestroyedVehicles);

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