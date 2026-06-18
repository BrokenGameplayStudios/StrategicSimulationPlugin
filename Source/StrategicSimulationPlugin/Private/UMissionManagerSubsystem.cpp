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
#include "USoldierManagerSubsystem.h"
#include "UStrategicSimulationDisplayHelpers.h"
#include "Engine/Engine.h"

void UMissionManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogTemp, Display, TEXT("UMissionManagerSubsystem initialized — vehicle missions ready"));
}

void UMissionManagerSubsystem::ClearRuntimeMissionStateForSiteMapLoad()
{
    const TArray<UMissionGroup*> MissionsToClear = ActiveMissions;
    const int32 ClearedCount = MissionsToClear.Num();
    TSet<UMissionGroup*> MissionSet(MissionsToClear);

    auto ResetVehicleForSiteMapLoad = [](UStrategyVehicle* Vehicle)
    {
        if (!Vehicle)
        {
            return;
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
        Vehicle->CurrentPhase = EVehicleMissionPhase::Docked;
        Vehicle->CurrentBehavior = EVehicleBehavior::Idle;
        Vehicle->CurrentTargetVehicle = nullptr;
        Vehicle->CombatBehaviorStartTime = -1.0f;
        Vehicle->CurrentWaypoints.Empty();
        Vehicle->ReturningWaypoints.Empty();
        Vehicle->ReturningDistanceTraveled = 0.0f;
        Vehicle->ReturningPathLength = 0.0f;
        Vehicle->TotalTravelTimeHours = 0.0f;
        Vehicle->OutboundTravelTime = 0.0f;
        Vehicle->ReturnTravelTime = 0.0f;
        Vehicle->SearchTimeAtTarget = 0.0f;
        Vehicle->PlannedRoundTripRange = 0.0f;
        Vehicle->RangeTraveledThisMission = 0.0f;

        if (Vehicle->CurrentHanger)
        {
            Vehicle->CurrentHanger->ParkedVehicles.Remove(Vehicle);
        }
        Vehicle->CurrentHanger = nullptr;
    };

    for (UMissionGroup* Mission : MissionsToClear)
    {
        if (!Mission)
        {
            continue;
        }

        for (UStrategyVehicle* Vehicle : Mission->VehiclesInFleet)
        {
            ResetVehicleForSiteMapLoad(Vehicle);
        }
    }

    if (USoldierManagerSubsystem* SoldierMgr = GetSoldierManager())
    {
        for (const EFactionType Faction : { EFactionType::Human, EFactionType::Enemy })
        {
            for (UStrategySoldier* Soldier : SoldierMgr->GetRoster(Faction))
            {
                if (Soldier && Soldier->CurrentMission && MissionSet.Contains(Soldier->CurrentMission))
                {
                    Soldier->CurrentMission = nullptr;
                }
            }
        }
    }

    ActiveMissions.Empty();

    if (ClearedCount > 0)
    {
        UE_LOG(LogTemp, Display, TEXT("[SAVE] Cleared %d stale mission(s) from pre-load session"), ClearedCount);
    }
}

void UMissionManagerSubsystem::OnDayPassed(int32 NewDay)
{
    UE_LOG(LogTemp, Display, TEXT("[MISSION] Day %d — SimulateOneDay() called (ActiveMissions: %d)"), NewDay, ActiveMissions.Num());
    CancelStaleDeferredMissions(NewDay);
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
    {
        return 0.0f;
    }

    return TimeMgr->GetElapsedSimulationHours();
}

void UMissionManagerSubsystem::GetMapBounds(float& OutWidth, float& OutHeight, float& OutPadding) const
{
    OutWidth = 1920.0f;
    OutHeight = 1080.0f;
    OutPadding = 100.0f;

    if (UGameInstance* GI = GetGameInstance())
    {
        if (UStrategyCampaignSubsystem* Campaign = GI->GetSubsystem<UStrategyCampaignSubsystem>())
        {
            OutWidth = Campaign->LogicalMapWidth;
            OutHeight = Campaign->LogicalMapHeight;
            OutPadding = Campaign->MapBorderPadding;
        }
    }
}

bool UMissionManagerSubsystem::IsValidMapLocation(const FVector2D& Location, float MinX, float MinY, float MaxX, float MaxY)
{
    if (Location.IsNearlyZero(10.f))
    {
        return false;
    }

    return Location.X >= MinX && Location.X <= MaxX && Location.Y >= MinY && Location.Y <= MaxY;
}

UStrategySiteDefinition* UMissionManagerSubsystem::FindSiteAtLocation(const FVector2D& Location, float Tolerance) const
{
    UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
    if (!BaseMgr)
    {
        return nullptr;
    }

    UStrategySiteDefinition* BestMatch = nullptr;
    float BestDist = Tolerance;

    for (UStrategySiteDefinition* Site : BaseMgr->AllPotentialSites)
    {
        if (!Site)
        {
            continue;
        }

        const float Dist = FVector2D::Distance(Site->Location, Location);
        if (Dist <= BestDist)
        {
            BestDist = Dist;
            BestMatch = Site;
        }
    }

    return BestMatch;
}

bool UMissionManagerSubsystem::IsSiteTargetedByActiveMissions(const UStrategySiteDefinition* Site, const UMissionGroup* IgnoreMission) const
{
    if (!Site)
    {
        return false;
    }

    TSet<UStrategySiteDefinition*> ReservedSites;
    CollectSitesTargetedByActiveMissions(ReservedSites, IgnoreMission);
    return ReservedSites.Contains(const_cast<UStrategySiteDefinition*>(Site));
}

void UMissionManagerSubsystem::CollectSitesTargetedByActiveMissions(TSet<UStrategySiteDefinition*>& OutSites, const UMissionGroup* IgnoreMission) const
{
    for (const UMissionGroup* Mission : ActiveMissions)
    {
        if (!Mission || Mission == IgnoreMission || !Mission->bMovementActivated)
        {
            continue;
        }

        for (UStrategyVehicle* Vehicle : Mission->VehiclesInFleet)
        {
            if (!Vehicle || Vehicle->CurrentWaypoints.Num() < 2)
            {
                continue;
            }

            if (UStrategySiteDefinition* Site = FindSiteAtLocation(Vehicle->CurrentWaypoints[1]))
            {
                OutSites.Add(Site);
            }
        }
    }
}

FVector2D UMissionManagerSubsystem::PickPatrolPointWithinRange(const FVector2D& Origin, float MaxRoundTripRange, float MinX, float MinY, float MaxX, float MaxY) const
{
    const float MaxOutbound = FMath::Max(50.f, MaxRoundTripRange * 0.45f);
    const float MinOutbound = FMath::Min(MaxOutbound * 0.35f, 150.f);

    for (int32 Attempt = 0; Attempt < 12; ++Attempt)
    {
        const float Dist = FMath::RandRange(MinOutbound, MaxOutbound);
        const float Angle = FMath::RandRange(0.f, 2.f * PI);
        FVector2D Candidate = Origin + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Dist;

        Candidate.X = FMath::Clamp(Candidate.X, MinX, MaxX);
        Candidate.Y = FMath::Clamp(Candidate.Y, MinY, MaxY);

        if (IsValidMapLocation(Candidate, MinX, MinY, MaxX, MaxY))
        {
            return Candidate;
        }
    }

    return Origin;
}

bool UMissionManagerSubsystem::HasOffensiveTargetInRange(UStrategyVehicle* Vehicle) const
{
    FVector2D DummyTarget;
    TSet<UStrategySiteDefinition*> DummyReserved;
    return TryPickMissionTarget(Vehicle, EMissionType::Offensive, DummyTarget, DummyReserved, nullptr);
}

bool UMissionManagerSubsystem::HasSalvageTargetInRange(UStrategyVehicle* Vehicle) const
{
    if (!Vehicle || !Vehicle->VehicleDefinition
        || !UStrategicSimulationDisplayHelpers::IsSalvageCapableVehicleType(Vehicle->VehicleDefinition->VehicleType))
    {
        return false;
    }

    if (UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>())
    {
        if (!Campaign->bSalvageMissionsEnabled || !Campaign->bSalvageSitesEnabled)
        {
            return false;
        }
    }

    FVector2D DummyTarget;
    TSet<UStrategySiteDefinition*> DummyReserved;
    return TryPickMissionTarget(Vehicle, EMissionType::Salvage, DummyTarget, DummyReserved, nullptr);
}

bool UMissionManagerSubsystem::TryPickMissionTarget(UStrategyVehicle* Vehicle, EMissionType MissionType, FVector2D& OutTarget,
    TSet<UStrategySiteDefinition*>& InOutReservedSites, UStrategyBase** OutTargetBase) const
{
    float MapWidth, MapHeight, MapPadding;
    GetMapBounds(MapWidth, MapHeight, MapPadding);

    const float MinX = MapPadding;
    const float MaxX = MapWidth - MapPadding;
    const float MinY = MapPadding;
    const float MaxY = MapHeight - MapPadding;

    if (!Vehicle || !Vehicle->HomeBase)
    {
        return false;
    }

    UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
    if (!BaseMgr)
    {
        return false;
    }

    const EFactionType Faction = Vehicle->HomeBase->OwningFaction;
    const EFactionType EnemyFaction = (Faction == EFactionType::Human) ? EFactionType::Enemy : EFactionType::Human;
    const FVector2D Origin = Vehicle->HomeBase->MapLocation;
    if (!IsValidMapLocation(Origin, MinX, MinY, MaxX, MaxY))
    {
        UE_LOG(LogTemp, Warning, TEXT("[MISSION TARGET] %s — invalid home base location (%.0f, %.0f)"),
            Vehicle->VehicleDefinition ? *Vehicle->VehicleDefinition->VehicleName.ToString() : TEXT("Vehicle"),
            Origin.X, Origin.Y);
        return false;
    }

    auto IsReconCandidateSite = [](UStrategySiteDefinition* Site) -> bool
    {
        return Site->SiteType == EStrategySiteType::PotentialBase || Site->SiteType == EStrategySiteType::ResourceNode;
    };

    switch (MissionType)
    {
    case EMissionType::Recon:
    {
        UStrategySiteDefinition* BestSite = nullptr;
        float BestDist = MAX_FLT;

        for (UStrategySiteDefinition* Site : BaseMgr->AllPotentialSites)
        {
            if (!Site || Site->bHasBeenUsed || !IsReconCandidateSite(Site))
            {
                continue;
            }

            if (!IsValidMapLocation(Site->Location, MinX, MinY, MaxX, MaxY))
            {
                continue;
            }

            if (InOutReservedSites.Contains(Site))
            {
                continue;
            }

            const TArray<UStrategySiteDefinition*>& Discovered =
                (Faction == EFactionType::Human) ? BaseMgr->DiscoveredSitesHuman : BaseMgr->DiscoveredSitesEnemy;

            if (Discovered.Contains(Site))
            {
                continue;
            }

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
            OutTarget = BestSite->Location;
            InOutReservedSites.Add(BestSite);
            UE_LOG(LogTemp, Verbose, TEXT("[MISSION TARGET] %s → recon site '%s' at (%.0f, %.0f)"),
                Vehicle->VehicleDefinition ? *Vehicle->VehicleDefinition->VehicleName.ToString() : TEXT("Vehicle"),
                *BestSite->SiteName, OutTarget.X, OutTarget.Y);
            return true;
        }

        OutTarget = PickPatrolPointWithinRange(Origin, Vehicle->CurrentRangeLeft, MinX, MinY, MaxX, MaxY);
        const float PatrolRoundTrip = FVector2D::Distance(Origin, OutTarget) * 2.0f;
        if (!Vehicle->HasEnoughRangeForMission(PatrolRoundTrip))
        {
            UE_LOG(LogTemp, Warning, TEXT("[MISSION TARGET] %s — no valid recon site and patrol point out of range"),
                Vehicle->VehicleDefinition ? *Vehicle->VehicleDefinition->VehicleName.ToString() : TEXT("Vehicle"));
            return false;
        }

        UE_LOG(LogTemp, Verbose, TEXT("[MISSION TARGET] %s → patrol point (%.0f, %.0f) — no undiscovered sites in range"),
            Vehicle->VehicleDefinition ? *Vehicle->VehicleDefinition->VehicleName.ToString() : TEXT("Vehicle"),
            OutTarget.X, OutTarget.Y);
        return true;
    }

    case EMissionType::Offensive:
    case EMissionType::Defensive:
    {
        TArray<UStrategyBase*> InRangeEnemyBases;
        for (UStrategyBase* EnemyBase : BaseMgr->GetBases(EnemyFaction))
        {
            if (!EnemyBase || !IsValidMapLocation(EnemyBase->MapLocation, MinX, MinY, MaxX, MaxY))
            {
                continue;
            }

            const float RoundTrip = FVector2D::Distance(Origin, EnemyBase->MapLocation) * 2.0f;
            if (Vehicle->HasEnoughRangeForMission(RoundTrip))
            {
                InRangeEnemyBases.Add(EnemyBase);
            }
        }

        if (InRangeEnemyBases.Num() > 0)
        {
            UStrategyBase* TargetBase = InRangeEnemyBases[FMath::RandRange(0, InRangeEnemyBases.Num() - 1)];
            OutTarget = TargetBase->MapLocation;
            if (OutTargetBase)
            {
                *OutTargetBase = TargetBase;
            }

            UE_LOG(LogTemp, Verbose, TEXT("[BASE ATTACK EVENT] %s from '%s' → enemy base '%s' at (%.0f, %.0f)"),
                Vehicle->VehicleDefinition ? *Vehicle->VehicleDefinition->VehicleName.ToString() : TEXT("Vehicle"),
                Vehicle->HomeBase ? *Vehicle->HomeBase->BaseName.ToString() : TEXT("Unknown"),
                *TargetBase->BaseName.ToString(), OutTarget.X, OutTarget.Y);
            return true;
        }
        break;
    }

    case EMissionType::Interception:
    {
        UStrategyVehicle* NearestEnemy = nullptr;
        float NearestDist = MAX_FLT;

        for (UMissionGroup* Mission : ActiveMissions)
        {
            if (!Mission || !Mission->OriginBase || !Mission->bMovementActivated)
            {
                continue;
            }

            if (Mission->OriginBase->OwningFaction != EnemyFaction)
            {
                continue;
            }

            for (UStrategyVehicle* EnemyVehicle : Mission->VehiclesInFleet)
            {
                if (!EnemyVehicle || EnemyVehicle->CurrentPhase == EVehicleMissionPhase::Docked)
                {
                    continue;
                }

                const float Dist = FVector2D::Distance(Origin, EnemyVehicle->CurrentPosition);
                const float RoundTrip = Dist * 2.0f;
                if (Dist < NearestDist && Vehicle->HasEnoughRangeForMission(RoundTrip))
                {
                    NearestDist = Dist;
                    NearestEnemy = EnemyVehicle;
                }
            }
        }

        if (NearestEnemy)
        {
            OutTarget = NearestEnemy->CurrentPosition;
            return true;
        }

        for (UStrategyBase* EnemyBase : BaseMgr->GetBases(EnemyFaction))
        {
            if (!EnemyBase)
            {
                continue;
            }

            const float RoundTrip = FVector2D::Distance(Origin, EnemyBase->MapLocation) * 2.0f;
            if (Vehicle->HasEnoughRangeForMission(RoundTrip))
            {
                OutTarget = EnemyBase->MapLocation;
                return true;
            }
        }
        break;
    }

    case EMissionType::Salvage:
    {
        UStrategySiteDefinition* BestSite = nullptr;
        float BestScore = -MAX_FLT;

        for (UStrategySiteDefinition* Site : BaseMgr->AllPotentialSites)
        {
            if (!Site || !BaseMgr->CanSalvageSite(Faction, Site, Vehicle))
            {
                continue;
            }

            if (InOutReservedSites.Contains(Site))
            {
                continue;
            }

            if (!IsValidMapLocation(Site->Location, MinX, MinY, MaxX, MaxY))
            {
                continue;
            }

            float Score = static_cast<float>(Site->CurrentResources.Metals + Site->CurrentResources.Chemicals);
            if (Site->WreckOwnerFaction != Faction)
            {
                Score += 500.0f;
            }

            const float Dist = FVector2D::Distance(Origin, Site->Location);
            Score -= Dist * 0.1f;

            if (Score > BestScore)
            {
                BestScore = Score;
                BestSite = Site;
            }
        }

        if (BestSite)
        {
            OutTarget = BestSite->Location;
            InOutReservedSites.Add(BestSite);
            UE_LOG(LogTemp, Verbose, TEXT("[MISSION TARGET] %s → salvage wreck '%s' at (%.0f, %.0f)"),
                Vehicle->VehicleDefinition ? *Vehicle->VehicleDefinition->VehicleName.ToString() : TEXT("Vehicle"),
                *BestSite->SiteName, OutTarget.X, OutTarget.Y);
            return true;
        }
        break;
    }

    default:
        break;
    }

    return false;
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
    case EMissionType::Salvage:
        if (UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>())
        {
            SearchHours = FMath::Max(0.5f, Campaign->SalvageOnStationHours);
        }
        else
        {
            SearchHours = 4.0f;
        }
        break;
    default:
        SearchHours = 2.0f;
        break;
    }

    TArray<UStrategyVehicle*> LaunchedVehicles;

    TSet<UStrategySiteDefinition*> ReservedSites;
    CollectSitesTargetedByActiveMissions(ReservedSites, Mission);

    for (UStrategyVehicle* Vehicle : Mission->VehiclesInFleet)
    {
        if (!Vehicle || !Vehicle->HomeBase) continue;

        UStrategyBase* TargetEnemyBase = nullptr;
        FVector2D TargetLocation;
        if (!TryPickMissionTarget(Vehicle, MissionType, TargetLocation, ReservedSites, &TargetEnemyBase))
        {
            UE_LOG(LogTemp, Warning, TEXT("[LIVE MISSION] %s skipped — no valid in-range target found"),
                Vehicle->VehicleDefinition ? *Vehicle->VehicleDefinition->VehicleName.ToString() : TEXT("Vehicle"));

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

        float MapWidth, MapHeight, MapPadding;
        GetMapBounds(MapWidth, MapHeight, MapPadding);
        const float MinX = MapPadding;
        const float MaxX = MapWidth - MapPadding;
        const float MinY = MapPadding;
        const float MaxY = MapHeight - MapPadding;

        if (!IsValidMapLocation(TargetLocation, MinX, MinY, MaxX, MaxY))
        {
            UE_LOG(LogTemp, Warning, TEXT("[LIVE MISSION] %s skipped — invalid target (%.0f, %.0f)"),
                Vehicle->VehicleDefinition ? *Vehicle->VehicleDefinition->VehicleName.ToString() : TEXT("Vehicle"),
                TargetLocation.X, TargetLocation.Y);
            for (UStrategySoldier* Soldier : Vehicle->CurrentPassengers)
            {
                if (Soldier) Soldier->CurrentMission = nullptr;
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

        const FVector2D OriginLocation = Vehicle->HomeBase->MapLocation;
        const float OutboundDist = FVector2D::Distance(OriginLocation, TargetLocation);
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
        if (TargetEnemyBase)
        {
            Mission->TargetEnemyBase = TargetEnemyBase;
        }
        if (MissionType == EMissionType::Salvage)
        {
            if (UStrategySiteDefinition* WreckSite = FindSiteAtLocation(TargetLocation))
            {
                Mission->TargetSalvageSite = WreckSite;
            }
        }
        LaunchedVehicles.Add(Vehicle);

        UE_LOG(LogTemp, Verbose, TEXT("[LIVE MISSION] Activated %s for %s from (%.0f,%.0f) → (%.0f,%.0f) round-trip %.0f, range left: %.0f"),
            *UEnum::GetValueAsString(MissionType),
            Vehicle->VehicleDefinition ? *Vehicle->VehicleDefinition->VehicleName.ToString() : TEXT("Vehicle"),
            OriginLocation.X, OriginLocation.Y, TargetLocation.X, TargetLocation.Y,
            RoundTripDist, Vehicle->CurrentRangeLeft);
    }

    Mission->VehiclesInFleet = LaunchedVehicles;

    if (Mission->VehiclesInFleet.Num() == 0)
    {
        ActiveMissions.Remove(Mission);
        UE_LOG(LogTemp, Warning, TEXT("[LIVE MISSION] No vehicles launched — mission cancelled"));
    }
}

float UMissionManagerSubsystem::ComputeEvenlySpacedLaunchHour(int32 SlotIndex, int32 TotalSlots) const
{
    const float CurrentHours = GetCurrentGameHours();
    const float DayStart = FMath::Floor(CurrentHours / 24.f) * 24.f;
    const int32 SafeTotal = FMath::Max(1, TotalSlots);
    const float SlotOffset = (static_cast<float>(SlotIndex) + 0.5f) * (24.f / static_cast<float>(SafeTotal));
    const float SlotHour = DayStart + SlotOffset;

    if (SlotHour <= CurrentHours + KINDA_SMALL_NUMBER)
    {
        return DayStart + 24.f + SlotOffset;
    }

    return SlotHour;
}

bool UMissionManagerSubsystem::IsVehicleCommittedToAnyMission(UStrategyVehicle* Vehicle, const UMissionGroup* IgnoreMission) const
{
    if (!Vehicle)
    {
        return false;
    }

    for (const UMissionGroup* Mission : ActiveMissions)
    {
        if (!Mission || Mission == IgnoreMission || Mission->Status != EMissionStatus::InProgress)
        {
            continue;
        }

        if (Mission->VehiclesInFleet.Contains(Vehicle))
        {
            return true;
        }
    }

    if (IgnoreMission && Vehicle->CurrentMission == IgnoreMission)
    {
        return false;
    }

    return Vehicle->CurrentMission != nullptr;
}

void UMissionManagerSubsystem::CancelStaleDeferredMissions(int32 CurrentSimulationDay)
{
    TArray<UMissionGroup*> ToCancel;

    for (UMissionGroup* Mission : ActiveMissions)
    {
        if (!Mission || Mission->bMovementActivated || Mission->Status != EMissionStatus::InProgress)
        {
            continue;
        }

        if (Mission->StartDay >= CurrentSimulationDay)
        {
            continue;
        }

        ToCancel.Add(Mission);
    }

    for (UMissionGroup* Mission : ToCancel)
    {
        for (UStrategyVehicle* Vehicle : Mission->VehiclesInFleet)
        {
            if (Vehicle && Vehicle->CurrentMission == Mission)
            {
                Vehicle->CurrentMission = nullptr;
            }
        }

        ActiveMissions.Remove(Mission);
        UE_LOG(LogTemp, Display, TEXT("[MISSION] Cancelled stale deferred mission from Day %d (today is Day %d)"),
            Mission->StartDay, CurrentSimulationDay);
    }
}

bool UMissionManagerSubsystem::IsVehicleReadyForMissionLaunch(UStrategyVehicle* Vehicle, const UMissionGroup* Mission) const
{
    if (!Vehicle || !Mission || Vehicle->IsDestroyed())
    {
        return false;
    }

    if (Vehicle->CurrentMission != Mission)
    {
        return false;
    }

    if (Vehicle->GetMissionPhase() != EVehicleMissionPhase::Docked)
    {
        return false;
    }

    if (IsVehicleCommittedToAnyMission(Vehicle, Mission))
    {
        for (const UMissionGroup* OtherMission : ActiveMissions)
        {
            if (!OtherMission || OtherMission == Mission || OtherMission->Status != EMissionStatus::InProgress)
            {
                continue;
            }

            if (OtherMission->VehiclesInFleet.Contains(Vehicle) && OtherMission->bMovementActivated)
            {
                return false;
            }
        }
    }

    return true;
}

TArray<UStrategyVehicle*> UMissionManagerSubsystem::GatherIdleVehiclesAtBase(UStrategyBase* Base) const
{
    TArray<UStrategyVehicle*> IdleVehicles;

    if (!Base)
    {
        return IdleVehicles;
    }

    for (UStrategyFacility* Facility : Base->Facilities)
    {
        if (!Facility || !Facility->bIsOperational || !Facility->FacilityDefinition ||
            Facility->FacilityDefinition->FacilityType != EFacilityType::Hanger)
        {
            continue;
        }

        for (UStrategyVehicle* Vehicle : Facility->ParkedVehicles)
        {
            if (!Vehicle || Vehicle->IsDestroyed() || IsVehicleCommittedToAnyMission(Vehicle))
            {
                continue;
            }

            if (Vehicle->CurrentMission && Vehicle->CurrentMission->bMovementActivated)
            {
                continue;
            }

            if (Vehicle->CurrentRangeLeft <= 0.0f || Vehicle->NeedsRepair())
            {
                continue;
            }

            const int32 MaxHealth = Vehicle->VehicleDefinition ? Vehicle->VehicleDefinition->MaxHealth : 100;
            if (Vehicle->CurrentHealth < MaxHealth * 0.95f)
            {
                continue;
            }

            IdleVehicles.Add(Vehicle);
        }
    }

    return IdleVehicles;
}

void UMissionManagerSubsystem::PrepareVehiclesForDeparture(UMissionGroup* Mission)
{
    if (!Mission || !Mission->OriginBase)
    {
        return;
    }

    UStrategyFacility* OriginHangar = Mission->OriginBase->FindFirstOperationalHangar();

    for (UStrategyVehicle* Vehicle : Mission->VehiclesInFleet)
    {
        if (!Vehicle)
        {
            continue;
        }

        if (OriginHangar)
        {
            Vehicle->HomeHanger = OriginHangar;
        }

        if (Vehicle->CurrentHanger)
        {
            Vehicle->CurrentHanger->ParkedVehicles.Remove(Vehicle);
            Vehicle->CurrentHanger = nullptr;
        }
        else if (Vehicle->HomeHanger)
        {
            Vehicle->HomeHanger->ParkedVehicles.Remove(Vehicle);
        }
    }
}

void UMissionManagerSubsystem::ProcessPendingMissionLaunches(float CurrentHours)
{
    TArray<UMissionGroup*> ToCancel;
    static constexpr int32 MaxMissionLaunchesPerTick = 8;
    int32 LaunchesThisTick = 0;

    for (UMissionGroup* Mission : ActiveMissions)
    {
        if (LaunchesThisTick >= MaxMissionLaunchesPerTick)
        {
            break;
        }

        if (!Mission || !Mission->bIsLiveMovement || Mission->bMovementActivated)
        {
            continue;
        }

        if (CurrentHours + KINDA_SMALL_NUMBER < Mission->ScheduledLaunchGameHours)
        {
            continue;
        }

        TArray<UStrategyVehicle*> ReadyVehicles;
        for (UStrategyVehicle* Vehicle : Mission->VehiclesInFleet)
        {
            if (IsVehicleReadyForMissionLaunch(Vehicle, Mission))
            {
                ReadyVehicles.Add(Vehicle);
            }
            else if (Vehicle)
            {
                UE_LOG(LogTemp, Warning, TEXT("[MISSION] %s not ready for scheduled launch (phase: %s, current mission: %s)"),
                    Vehicle->VehicleDefinition ? *Vehicle->VehicleDefinition->VehicleName.ToString() : TEXT("Vehicle"),
                    *UEnum::GetValueAsString(Vehicle->GetMissionPhase()),
                    Vehicle->CurrentMission ? TEXT("set") : TEXT("null"));
            }
        }

        if (ReadyVehicles.Num() == 0)
        {
            ToCancel.Add(Mission);
            continue;
        }

        Mission->VehiclesInFleet = ReadyVehicles;
        PrepareVehiclesForDeparture(Mission);
        ActivateLiveMovementForVehicles(Mission, Mission->MissionType);
        Mission->bMovementActivated = true;
        ++LaunchesThisTick;

        const float HourOfDay = FMath::Fmod(CurrentHours, 24.f);
        UE_LOG(LogTemp, Verbose, TEXT("[MISSION] Departing %s mission from '%s' at %.1fh (%d vehicles)"),
            *UEnum::GetValueAsString(Mission->MissionType),
            Mission->OriginBase ? *Mission->OriginBase->BaseName.ToString() : TEXT("Unknown"),
            HourOfDay,
            Mission->VehiclesInFleet.Num());
    }

    for (UMissionGroup* Mission : ToCancel)
    {
        for (UStrategyVehicle* Vehicle : Mission->VehiclesInFleet)
        {
            if (Vehicle && Vehicle->CurrentMission == Mission)
            {
                Vehicle->CurrentMission = nullptr;
            }
        }
        ActiveMissions.Remove(Mission);
        UE_LOG(LogTemp, Warning, TEXT("[MISSION] Cancelled deferred mission — no vehicles ready at launch time"));
    }
}

int32 UMissionManagerSubsystem::ScheduleVehicleMissionsForBase(UStrategyBase* Base, EFactionType Faction, EMissionType MissionType)
{
    const TArray<UStrategyVehicle*> IdleVehicles = GatherIdleVehiclesAtBase(Base);
    TArray<EMissionType> MissionTypes;
    MissionTypes.Init(MissionType, IdleVehicles.Num());
    return ScheduleVehicleMissionsForBase(Base, Faction, MissionTypes);
}

int32 UMissionManagerSubsystem::ScheduleVehicleMissionsForBase(UStrategyBase* Base, EFactionType Faction, const TArray<EMissionType>& PerVehicleMissionTypes)
{
    if (!Base)
    {
        return 0;
    }

    const TArray<UStrategyVehicle*> IdleVehicles = GatherIdleVehiclesAtBase(Base);
    if (IdleVehicles.Num() == 0)
    {
        return 0;
    }

    bool bStagger = true;
    if (UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>())
    {
        bStagger = Campaign->bStaggerMissionLaunches;
    }

    int32 ScheduledCount = 0;
    const int32 TotalSlots = IdleVehicles.Num();

    for (int32 SlotIndex = 0; SlotIndex < TotalSlots; ++SlotIndex)
    {
        UStrategyVehicle* Vehicle = IdleVehicles[SlotIndex];
        if (!Vehicle)
        {
            continue;
        }

        const EMissionType MissionType = PerVehicleMissionTypes.IsValidIndex(SlotIndex)
            ? PerVehicleMissionTypes[SlotIndex]
            : EMissionType::Recon;

        const float LaunchHour = bStagger
            ? ComputeEvenlySpacedLaunchHour(SlotIndex, TotalSlots)
            : -1.f;

        if (StartMission(Base, { Vehicle }, 1, {}, MissionType, Faction, LaunchHour))
        {
            ScheduledCount++;

            if (bStagger)
            {
                const float HourOfDay = FMath::Fmod(LaunchHour, 24.f);
                UE_LOG(LogTemp, Display, TEXT("[MISSION] Scheduled %s for '%s' at base '%s' — slot %d/%d, depart ~%.1fh"),
                    Vehicle->VehicleDefinition ? *Vehicle->VehicleDefinition->VehicleName.ToString() : TEXT("Vehicle"),
                    *UEnum::GetValueAsString(MissionType),
                    *Base->BaseName.ToString(),
                    SlotIndex + 1, TotalSlots, HourOfDay);
            }
        }
    }

    return ScheduledCount;
}

void UMissionManagerSubsystem::HandleBaseAttackArrival(UStrategyVehicle* Vehicle, UMissionGroup* Mission)
{
    if (!Vehicle || !Mission || Mission->bBaseAttackArrivalLogged || Mission->MissionType != EMissionType::Offensive)
    {
        return;
    }

    Mission->bBaseAttackArrivalLogged = true;

    const FString AttackerName = Vehicle->VehicleDefinition
        ? Vehicle->VehicleDefinition->VehicleName.ToString()
        : GetNameSafe(Vehicle);
    const FString OriginName = Mission->OriginBase ? Mission->OriginBase->BaseName.ToString() : TEXT("Unknown");
    const FString TargetName = Mission->TargetEnemyBase
        ? Mission->TargetEnemyBase->BaseName.ToString()
        : TEXT("enemy base");

    UE_LOG(LogTemp, Verbose, TEXT("[BASE ATTACK EVENT] %s from '%s' arrived at '%s' — base attack event here"),
        *AttackerName, *OriginName, *TargetName);
}

UMissionGroup* UMissionManagerSubsystem::StartMission(UStrategyBase* OriginBase, TArray<UStrategyVehicle*> Vehicles, int32 DurationDays, const TArray<UStrategySoldier*>& SoldiersToAssign, EMissionType MissionType, EFactionType AttackingFaction, float ScheduledLaunchGameHours)
{
    if (!OriginBase || Vehicles.Num() == 0) return nullptr;

    for (UStrategyVehicle* Vehicle : Vehicles)
    {
        if (!Vehicle)
        {
            continue;
        }

        if (IsVehicleCommittedToAnyMission(Vehicle))
        {
            UE_LOG(LogTemp, Warning, TEXT("[MISSION] Cannot start mission — %s is already assigned to an active mission"),
                Vehicle->VehicleDefinition ? *Vehicle->VehicleDefinition->VehicleName.ToString() : TEXT("Vehicle"));
            return nullptr;
        }
    }

    const float CurrentHours = GetCurrentGameHours();
    const bool bLaunchImmediately = (ScheduledLaunchGameHours < 0.f);
    const float EffectiveLaunchHour = bLaunchImmediately ? CurrentHours : ScheduledLaunchGameHours;
    const bool bDeferLaunch = !bLaunchImmediately && (EffectiveLaunchHour > CurrentHours + KINDA_SMALL_NUMBER);

    UMissionGroup* NewMission = NewObject<UMissionGroup>();
    NewMission->OriginBase = OriginBase;
    NewMission->VehiclesInFleet = Vehicles;
    if (UTimeManagerSubsystem* TimeMgr = GetGameInstance()->GetSubsystem<UTimeManagerSubsystem>())
    {
        NewMission->StartDay = TimeMgr->GetSimulationDayNumber();
    }
    NewMission->bIsLiveMovement = true;
    NewMission->DurationDays = 0;
    NewMission->Status = EMissionStatus::InProgress;
    NewMission->Outcome = EMissionOutcome::Success;
    NewMission->MissionType = MissionType;
    NewMission->AttackingFaction = AttackingFaction;
    NewMission->ScheduledLaunchGameHours = EffectiveLaunchHour;
    NewMission->bMovementActivated = false;

    ActiveMissions.Add(NewMission);

    UStrategyFacility* OriginHangar = OriginBase->FindFirstOperationalHangar();

    for (UStrategyVehicle* Vehicle : Vehicles)
    {
        if (!Vehicle) continue;

        Vehicle->CurrentPassengers.Empty();
        Vehicle->CurrentMission = NewMission;

        Vehicle->HomeBase = OriginBase;
        if (OriginHangar)
        {
            Vehicle->HomeHanger = OriginHangar;
        }
        else if (Vehicle->CurrentHanger && !Vehicle->HomeHanger)
        {
            Vehicle->HomeHanger = Vehicle->CurrentHanger;
        }

        if (bDeferLaunch)
        {
            Vehicle->InitializeParkedAtBase();
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

    if (bDeferLaunch)
    {
        UE_LOG(LogTemp, Display, TEXT("[MISSION] Queued %s mission for faction %s with %d vehicles from base '%s' (departs at hour %.1f)"),
            *UEnum::GetValueAsString(MissionType), *UEnum::GetValueAsString(AttackingFaction), Vehicles.Num(),
            *OriginBase->BaseName.ToString(), FMath::Fmod(EffectiveLaunchHour, 24.f));
    }
    else
    {
        PrepareVehiclesForDeparture(NewMission);
        ActivateLiveMovementForVehicles(NewMission, MissionType);
        NewMission->bMovementActivated = true;

        UE_LOG(LogTemp, Display, TEXT("[MISSION] Launched live %s mission for faction %s with %d vehicles from base '%s'"),
            *UEnum::GetValueAsString(MissionType), *UEnum::GetValueAsString(AttackingFaction), Vehicles.Num(), *OriginBase->BaseName.ToString());
    }

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
    ProcessPendingMissionLaunches(CurrentHours);

    for (int32 i = ActiveMissions.Num() - 1; i >= 0; --i)
    {
        UMissionGroup* Mission = ActiveMissions[i];
        if (!Mission || Mission->Status != EMissionStatus::InProgress || !Mission->bIsLiveMovement) continue;

        if (!Mission->bMovementActivated)
        {
            continue;
        }

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
            UE_LOG(LogTemp, Verbose, TEXT("[LIVE MISSION] Mission fully completed — all vehicles docked or destroyed"));
        }
    }
}

void UMissionManagerSubsystem::HandleVehicleDestroyedInCombat(UStrategyVehicle* Vehicle)
{
    if (!Vehicle || !Vehicle->IsDestroyed() || Vehicle->bWreckSalvageProcessed)
    {
        return;
    }

    if (UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>())
    {
        if (!Campaign->bSalvageSitesEnabled)
        {
            return;
        }
    }

    Vehicle->bWreckSalvageProcessed = true;

    UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
    UStrategySiteDefinition* WreckSite = nullptr;
    if (BaseMgr)
    {
        WreckSite = BaseMgr->CreateSalvageSite(Vehicle->CurrentPosition, Vehicle);
    }

    if (WreckSite)
    {
        if (USoldierManagerSubsystem* SoldierMgr = GetGameInstance()->GetSubsystem<USoldierManagerSubsystem>())
        {
            SoldierMgr->ProcessCrewOnVehicleDestruction(Vehicle, WreckSite);
        }
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
    const bool bIsSalvage = (Mission->MissionType == EMissionType::Salvage);

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
        Vehicle->ActiveSalvageSite = nullptr;
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
    if (bIsSalvage)
    {
        for (UStrategyVehicle* Vehicle : Mission->VehiclesInFleet)
        {
            if (Vehicle)
            {
                Reward.Add(Vehicle->SalvageExtractedThisMission);
                Vehicle->SalvageExtractedThisMission = FResourceStockpile();
            }
        }

        UE_LOG(LogTemp, Display, TEXT("[SALVAGE] Mission complete — recovered M:%d Mt:%d Chem:%d Exo:%d"),
            Reward.Money, Reward.Metals, Reward.Chemicals, Reward.ExoticMaterial);
    }
    else if (bIsRecon)
    {
        Reward.ResearchPoints = FMath::RandRange(200, 500);
        UE_LOG(LogTemp, Verbose, TEXT("[RECON] Mission complete — intel gathered via live radar pings"));
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
        if (!bIsSalvage)
        {
            ResourceMgr->AddResources(Mission->AttackingFaction, Reward);
        }
    }

    UE_LOG(LogTemp, Verbose, TEXT("[MISSION] %s resolved as %s — Survived: %d | Combat losses: %d (no abstract casualties)"),
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