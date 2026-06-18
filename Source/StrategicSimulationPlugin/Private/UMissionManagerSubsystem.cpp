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
#include "UStrategyCampaignSubsystem.h"
#include "UStrategyEventDispatcher.h"
#include "UFactionIntelSubsystem.h"
#include "URadarContactSubsystem.h"
#include "UAIControllerSubsystem.h"
#include "UExplorationSubsystem.h"
#include "Engine/Engine.h"

/** Initializes the mission manager subsystem. */
void UMissionManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogTemp, Display, TEXT("UMissionManagerSubsystem initialized — vehicle missions ready"));
}

/** Clears missions, intel, contacts, and vehicle state before site-map load. */
void UMissionManagerSubsystem::ClearRuntimeMissionStateForSiteMapLoad()
{
    if (UFactionIntelSubsystem* IntelMgr = GetGameInstance()->GetSubsystem<UFactionIntelSubsystem>())
    {
        IntelMgr->ClearAllIntel();
    }

    if (URadarContactSubsystem* ContactMgr = GetGameInstance()->GetSubsystem<URadarContactSubsystem>())
    {
        ContactMgr->ClearAllContacts();
    }

    RecentCombatSalvageWrecks.Empty();

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
        Vehicle->ActiveSalvageSite = nullptr;
        Vehicle->ActiveExpansionSite = nullptr;
        Vehicle->bExpansionGuardActive = false;

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

/** Cancels stale deferred missions and runs daily mission simulation. */
void UMissionManagerSubsystem::OnDayPassed(int32 NewDay)
{
    UE_LOG(LogTemp, Display, TEXT("[MISSION] Day %d — SimulateOneDay() called (ActiveMissions: %d)"), NewDay, ActiveMissions.Num());
    CancelStaleDeferredMissions(NewDay);
    SimulateOneDay();
}

/** Ticks non-live mission duration and resolves completed missions. */
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

/** Returns elapsed simulation hours from the time manager. */
float UMissionManagerSubsystem::GetCurrentGameHours() const
{
    UTimeManagerSubsystem* TimeMgr = GetGameInstance()->GetSubsystem<UTimeManagerSubsystem>();
    if (!TimeMgr)
    {
        return 0.0f;
    }

    return TimeMgr->GetElapsedSimulationHours();
}

/** Reads logical map width, height, and border padding from campaign. */
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

/** True when location is inside padded map bounds. */
bool UMissionManagerSubsystem::IsValidMapLocation(const FVector2D& Location, float MinX, float MinY, float MaxX, float MaxY)
{
    if (Location.IsNearlyZero(10.f))
    {
        return false;
    }

    return Location.X >= MinX && Location.X <= MaxX && Location.Y >= MinY && Location.Y <= MaxY;
}

/** Delegates to base manager to find nearest site within tolerance. */
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

/** True when an active mission waypoint targets this site. */
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

/** Builds set of sites targeted by active mission waypoints. */
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

/** Picks a random in-bounds patrol point within range budget. */
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

/** True when TryPickMissionTarget finds an enemy base in range. */
bool UMissionManagerSubsystem::HasOffensiveTargetInRange(UStrategyVehicle* Vehicle) const
{
    FVector2D DummyTarget;
    TSet<UStrategySiteDefinition*> DummyReserved;
    return TryPickMissionTarget(Vehicle, EMissionType::Offensive, DummyTarget, DummyReserved, nullptr);
}

/** True when radar has an interceptable contact for the vehicle. */
bool UMissionManagerSubsystem::HasInterceptionTargetFromContacts(UStrategyVehicle* Vehicle) const
{
    if (!Vehicle || !Vehicle->HomeBase)
    {
        return false;
    }

    URadarContactSubsystem* ContactMgr = GetGameInstance()->GetSubsystem<URadarContactSubsystem>();
    if (!ContactMgr)
    {
        return false;
    }

    FRadarContact Contact;
    return ContactMgr->FindBestContactForInterception(Vehicle->HomeBase->OwningFaction, Vehicle->HomeBase,
        Vehicle, Contact);
}

/** Starts an immediate interception mission at a radar contact from the given base and vehicle. */
bool UMissionManagerSubsystem::LaunchInterceptionAtContact(UStrategyBase* OriginBase, UStrategyVehicle* Vehicle,
    FGuid ContactId)
{
    if (!OriginBase || !Vehicle || !ContactId.IsValid())
    {
        return false;
    }

    if (IsVehicleCommittedToAnyMission(Vehicle))
    {
        return false;
    }

    URadarContactSubsystem* ContactMgr = GetGameInstance()->GetSubsystem<URadarContactSubsystem>();
    if (!ContactMgr)
    {
        return false;
    }

    FRadarContact Contact;
    if (!ContactMgr->GetContactById(OriginBase->OwningFaction, ContactId, Contact))
    {
        return false;
    }

    if (ContactMgr->IsContactAlreadyTargeted(ContactId))
    {
        return false;
    }

    UStrategyVehicle* TrackedVehicle = ContactMgr->ResolveTrackedVehicle(Contact, OriginBase->OwningFaction);
    const FVector2D InterceptPos = URadarContactSubsystem::GetContactInterceptPosition(Contact);
    const float RoundTrip = FVector2D::Distance(OriginBase->MapLocation, InterceptPos) * 2.0f;
    if (!Vehicle->HasEnoughRangeForMission(RoundTrip))
    {
        return false;
    }

    ContactMgr->MarkContactTargeted(ContactId);

    UMissionGroup* Mission = StartMission(OriginBase, { Vehicle }, 0, {}, EMissionType::Interception,
        OriginBase->OwningFaction, -1.f);
    if (!Mission)
    {
        ContactMgr->UnmarkContactTargeted(ContactId);
        return false;
    }

    Mission->TargetContactId = ContactId;
    Mission->TargetInterceptVehicle = TrackedVehicle;

    UE_LOG(LogTemp, Display, TEXT("[INTERCEPT] %s launched interception from '%s' → %s at (%.0f, %.0f)"),
        *UEnum::GetValueAsString(OriginBase->OwningFaction),
        *OriginBase->BaseName.ToString(),
        *Contact.TrackedVehicleName,
        InterceptPos.X, InterceptPos.Y);

    return true;
}

/** True when any idle combat vehicle can reach the contact. */
bool UMissionManagerSubsystem::CanFactionInterceptContact(EFactionType Faction, FGuid ContactId) const
{
    if (!ContactId.IsValid())
    {
        return false;
    }

    URadarContactSubsystem* ContactMgr = GetGameInstance()->GetSubsystem<URadarContactSubsystem>();
    UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
    if (!ContactMgr || !BaseMgr)
    {
        return false;
    }

    if (ContactMgr->IsContactAlreadyTargeted(ContactId))
    {
        return false;
    }

    FRadarContact Contact;
    if (!ContactMgr->GetContactById(Faction, ContactId, Contact))
    {
        return false;
    }

    for (UStrategyBase* Base : BaseMgr->GetBases(Faction))
    {
        if (!Base)
        {
            continue;
        }

        for (UStrategyVehicle* Vehicle : GatherIdleVehiclesAtBase(Base))
        {
            if (!Vehicle || !Vehicle->VehicleDefinition)
            {
                continue;
            }

/** True for Gunship and Heavy types. */
            if (!UAIControllerSubsystem::IsCombatVehicleType(Vehicle->VehicleDefinition->VehicleType))
            {
                continue;
            }

            const FVector2D InterceptPos = URadarContactSubsystem::GetContactInterceptPosition(Contact);
            const float RoundTrip = FVector2D::Distance(Base->MapLocation, InterceptPos) * 2.0f;
            if (Vehicle->HasEnoughRangeForMission(RoundTrip))
            {
                return true;
            }
        }
    }

    return false;
}

/** Auto-selects nearest capable vehicle and launches interception at a contact. */
bool UMissionManagerSubsystem::TryLaunchInterceptionAtContactAuto(EFactionType Faction, FGuid ContactId,
    UStrategyBase*& OutOriginBase, UStrategyVehicle*& OutVehicle)
{
    OutOriginBase = nullptr;
    OutVehicle = nullptr;

    if (!ContactId.IsValid() || !CanFactionInterceptContact(Faction, ContactId))
    {
        return false;
    }

    URadarContactSubsystem* ContactMgr = GetGameInstance()->GetSubsystem<URadarContactSubsystem>();
    UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
    if (!ContactMgr || !BaseMgr)
    {
        return false;
    }

    FRadarContact Contact;
    if (!ContactMgr->GetContactById(Faction, ContactId, Contact))
    {
        return false;
    }

    UStrategyBase* BestBase = nullptr;
    UStrategyVehicle* BestVehicle = nullptr;
    float BestScore = -1.0f;

    for (UStrategyBase* Base : BaseMgr->GetBases(Faction))
    {
        if (!Base)
        {
            continue;
        }

        for (UStrategyVehicle* Vehicle : GatherIdleVehiclesAtBase(Base))
        {
            if (!Vehicle || !Vehicle->VehicleDefinition)
            {
                continue;
            }

/** True for Gunship and Heavy types. */
            if (!UAIControllerSubsystem::IsCombatVehicleType(Vehicle->VehicleDefinition->VehicleType))
            {
                continue;
            }

            const FVector2D InterceptPos = URadarContactSubsystem::GetContactInterceptPosition(Contact);
            const float Dist = FVector2D::Distance(Base->MapLocation, InterceptPos);
            const float RoundTrip = Dist * 2.0f;
            if (!Vehicle->HasEnoughRangeForMission(RoundTrip))
            {
                continue;
            }

            float Score = 1000.0f - Dist;
            if (Contact.bIsInboundThreat)
            {
                Score += 250.0f;
            }

            if (Score > BestScore)
            {
                BestScore = Score;
                BestBase = Base;
                BestVehicle = Vehicle;
            }
        }
    }

    if (!BestBase || !BestVehicle)
    {
        return false;
    }

    if (!LaunchInterceptionAtContact(BestBase, BestVehicle, ContactId))
    {
        return false;
    }

    OutOriginBase = BestBase;
    OutVehicle = BestVehicle;
    return true;
}

/** Heuristic score: resource value (bonus for enemy wrecks) divided by distance from Origin. */
float UMissionManagerSubsystem::ComputeSalvageTargetScore(EFactionType Faction, const UStrategySiteDefinition* Site,
    const FVector2D& Origin) const
{
    if (!Site)
    {
        return 0.0f;
    }

    float ResourceValue = static_cast<float>(Site->CurrentResources.Metals + Site->CurrentResources.Chemicals);
    if (Site->WreckOwnerFaction != Faction)
    {
        ResourceValue += 500.0f;
    }

    const float Dist = FVector2D::Distance(Origin, Site->Location);
    return ResourceValue / FMath::Max(Dist, 1.0f);
}

/** Counts non-completed Salvage missions for a faction (active + in-progress). */
int32 UMissionManagerSubsystem::CountActiveSalvageMissions(EFactionType Faction) const
{
    int32 Count = 0;
    for (const UMissionGroup* Mission : ActiveMissions)
    {
        if (Mission && Mission->MissionType == EMissionType::Salvage
            && Mission->AttackingFaction == Faction
            && Mission->Status != EMissionStatus::Completed
            && Mission->Status != EMissionStatus::Failed)
        {
            ++Count;
        }
    }

    return Count;
}

/** Removes RecentCombatSalvageWrecks entries older than SalvageCombatMemoryDays. */
void UMissionManagerSubsystem::PruneOldCombatSalvageRecords(int32 CurrentDay)
{
    UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();
    const int32 MemoryDays = Campaign ? FMath::Max(1, Campaign->SalvageCombatMemoryDays) : 3;

    RecentCombatSalvageWrecks.RemoveAll([CurrentDay, MemoryDays](const FCombatSalvageWreckRecord& Record)
    {
        return Record.CreatedOnDay < CurrentDay - MemoryDays;
    });
}

/** Appends a combat-wreck record after pruning stale entries (used for post-win salvage AI decline). */
void UMissionManagerSubsystem::RecordCombatSalvageWreck(UStrategySiteDefinition* Site, EFactionType WinnerFaction,
    int32 CurrentDay)
{
    if (!Site || WinnerFaction == EFactionType::Neutral)
    {
        return;
    }

    PruneOldCombatSalvageRecords(CurrentDay);

    FCombatSalvageWreckRecord Record;
    Record.SiteId = Site->SiteId;
    Record.WinnerFaction = WinnerFaction;
    Record.CreatedOnDay = CurrentDay;
    RecentCombatSalvageWrecks.Add(Record);
}

/** Checks RecentCombatSalvageWrecks for a matching SiteId, WinnerFaction, and memory window. */
bool UMissionManagerSubsystem::DidFactionWinCombatAtSite(EFactionType Faction, const UStrategySiteDefinition* Site,
    int32 CurrentDay) const
{
    if (!Site)
    {
        return false;
    }

    UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();
    const int32 MemoryDays = Campaign ? FMath::Max(1, Campaign->SalvageCombatMemoryDays) : 3;

    for (const FCombatSalvageWreckRecord& Record : RecentCombatSalvageWrecks)
    {
        if (Record.SiteId == Site->SiteId
            && Record.WinnerFaction == Faction
            && Record.CreatedOnDay >= CurrentDay - MemoryDays)
        {
            return true;
        }
    }

    return false;
}

/** Iterates AllPotentialSites for highest CanSalvageSite score meeting MinSalvageScoreThreshold. */
bool UMissionManagerSubsystem::FindBestSalvageTargetForVehicle(UStrategyVehicle* Vehicle,
    TSet<UStrategySiteDefinition*>& InOutReservedSites, UStrategySiteDefinition*& OutSite, float& OutScore) const
{
    OutSite = nullptr;
    OutScore = -MAX_FLT;

    if (!Vehicle || !Vehicle->HomeBase)
    {
        return false;
    }

    UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
    if (!BaseMgr)
    {
        return false;
    }

    float MapWidth, MapHeight, MapPadding;
    GetMapBounds(MapWidth, MapHeight, MapPadding);
    const float MinX = MapPadding;
    const float MaxX = MapWidth - MapPadding;
    const float MinY = MapPadding;
    const float MaxY = MapHeight - MapPadding;

    const EFactionType Faction = Vehicle->HomeBase->OwningFaction;
    const FVector2D Origin = Vehicle->HomeBase->MapLocation;

    UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();
    const float MinScore = Campaign ? Campaign->MinSalvageScoreThreshold : 15.0f;

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

        const float Score = ComputeSalvageTargetScore(Faction, Site, Origin);
        if (Score > OutScore)
        {
            OutScore = Score;
            OutSite = Site;
        }
    }

    return OutSite != nullptr && OutScore >= MinScore;
}

/** AI gate: salvage-capable vehicle, caps, score thresholds, loser rules, and post-combat decline chance. */
bool UMissionManagerSubsystem::EvaluateAISalvageScheduling(UStrategyVehicle* Vehicle,
    UStrategySiteDefinition*& OutBestSite, float& OutBestScore) const
{
    OutBestSite = nullptr;
    OutBestScore = 0.0f;

    if (!Vehicle || !Vehicle->VehicleDefinition || !Vehicle->HomeBase
        || !UStrategicSimulationDisplayHelpers::IsSalvageCapableVehicleType(Vehicle->VehicleDefinition->VehicleType))
    {
        return false;
    }

    UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();
    if (!Campaign || !Campaign->bSalvageMissionsEnabled || !Campaign->bSalvageSitesEnabled)
    {
        return false;
    }

    const EFactionType Faction = Vehicle->HomeBase->OwningFaction;
    if (CountActiveSalvageMissions(Faction) >= Campaign->MaxActiveSalvageMissionsPerFaction)
    {
        return false;
    }

    TSet<UStrategySiteDefinition*> ReservedSites;
    CollectSitesTargetedByActiveMissions(ReservedSites, nullptr);

    UStrategySiteDefinition* BestSite = nullptr;
    float BestScore = 0.0f;
    if (!FindBestSalvageTargetForVehicle(Vehicle, ReservedSites, BestSite, BestScore) || !BestSite)
    {
        return false;
    }

    const float Dist = FVector2D::Distance(Vehicle->HomeBase->MapLocation, BestSite->Location);
    const bool bOwnWreck = BestSite->WreckOwnerFaction == Faction;
    const float RequiredScore = bOwnWreck
        ? Campaign->MinSalvageScoreThreshold * Campaign->LoserSalvageScoreMultiplier
        : Campaign->MinSalvageScoreThreshold;

    if (BestScore < RequiredScore)
    {
        return false;
    }

    if (bOwnWreck && Dist > Campaign->LoserSalvageMaxDistance
        && BestScore < RequiredScore * 1.25f)
    {
        return false;
    }

    int32 CurrentDay = 1;
    if (UTimeManagerSubsystem* TimeMgr = GetGameInstance()->GetSubsystem<UTimeManagerSubsystem>())
    {
        CurrentDay = TimeMgr->GetSimulationDayNumber();
    }

    if (!bOwnWreck && DidFactionWinCombatAtSite(Faction, BestSite, CurrentDay)
        && FMath::FRand() < Campaign->SalvageDeclineAfterWinChance)
    {
        UE_LOG(LogTemp, Display,
            TEXT("[SALVAGE AI] %s declined salvage at '%s' (score %.1f) — post-combat retaliation risk"),
            *UEnum::GetValueAsString(Faction), *BestSite->SiteName, BestScore);
        return false;
    }

    OutBestSite = BestSite;
    OutBestScore = BestScore;
    return true;
}

/** Daily verbose log of known active wrecks, scores, and eligibility for AI tuning (PR-7). */
void UMissionManagerSubsystem::LogSalvageOpportunitiesForFaction(EFactionType Faction, int32 CurrentDay) const
{
    UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();
    UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
    if (!Campaign || !BaseMgr || !Campaign->bSalvageSitesEnabled || !Campaign->bSalvageMissionsEnabled)
    {
        return;
    }

    const TArray<UStrategyBase*>& Bases = BaseMgr->GetBases(Faction);
    if (Bases.Num() == 0)
    {
        return;
    }

    FVector2D NearestBaseLocation = Bases[0]->MapLocation;
    for (UStrategyBase* Base : Bases)
    {
        if (Base)
        {
            NearestBaseLocation = Base->MapLocation;
            break;
        }
    }

    int32 LoggedCount = 0;
    for (UStrategySiteDefinition* Site : BaseMgr->AllPotentialSites)
    {
        if (!Site || Site->SiteType != EStrategySiteType::SalvageSite
            || Site->SalvageState != ESalvageSiteState::Active)
        {
            continue;
        }

        if (!BaseMgr->CanSalvageSite(Faction, Site, nullptr))
        {
            continue;
        }

        float BestDist = MAX_FLT;
        FVector2D BestOrigin = NearestBaseLocation;
        for (UStrategyBase* Base : Bases)
        {
            if (!Base)
            {
                continue;
            }

            const float Dist = FVector2D::Distance(Base->MapLocation, Site->Location);
            if (Dist < BestDist)
            {
                BestDist = Dist;
                BestOrigin = Base->MapLocation;
            }
        }

        const float Score = ComputeSalvageTargetScore(Faction, Site, BestOrigin);
        const bool bEligible = Score >= Campaign->MinSalvageScoreThreshold;
        const bool bEnemyWreck = Site->WreckOwnerFaction != Faction;

        UE_LOG(LogTemp, Display,
            TEXT("[SALVAGE AI] Day %d %s opportunity: site %s score=%.1f owner=%s enemy=%s M=%d Chem=%d dist=%.0f eligible=%s"),
            CurrentDay,
            *UEnum::GetValueAsString(Faction),
            *Site->SiteId.ToString(EGuidFormats::Short),
            Score,
            *UEnum::GetValueAsString(Site->WreckOwnerFaction),
            bEnemyWreck ? TEXT("yes") : TEXT("no"),
            Site->CurrentResources.Metals,
            Site->CurrentResources.Chemicals,
            BestDist,
            bEligible ? TEXT("yes") : TEXT("no"));

        ++LoggedCount;
    }

    if (LoggedCount == 0)
    {
        UE_LOG(LogTemp, Verbose, TEXT("[SALVAGE AI] Day %d %s — no known active wrecks"),
            CurrentDay, *UEnum::GetValueAsString(Faction));
    }
}

/** Thin wrapper: returns true when EvaluateAISalvageScheduling finds a viable wreck for Vehicle. */
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

    UStrategySiteDefinition* BestSite = nullptr;
    float BestScore = 0.0f;
    return EvaluateAISalvageScheduling(Vehicle, BestSite, BestScore);
}

/** Picks a mission waypoint target based on type, range, intel, and site reservations. */
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
        UExplorationSubsystem* Exploration = GetGameInstance()->GetSubsystem<UExplorationSubsystem>();

        UStrategySiteDefinition* SurveySite = nullptr;
        auto ValidateReconTarget = [&](FVector2D& Target) -> bool
        {
            if (!IsValidMapLocation(Target, MinX, MinY, MaxX, MaxY))
            {
                return false;
            }

            const float RoundTrip = FVector2D::Distance(Origin, Target) * 2.0f;
            return Vehicle->HasEnoughRangeForMission(RoundTrip);
        };

        if (Exploration && Exploration->PickInboundEntryPatrolTarget(Vehicle->HomeBase, Vehicle, OutTarget)
            && ValidateReconTarget(OutTarget))
        {
            return true;
        }

        if (Exploration && Exploration->FindSurveyTarget(Vehicle, SurveySite) && SurveySite
            && !InOutReservedSites.Contains(SurveySite))
        {
            OutTarget = SurveySite->Location;
            if (ValidateReconTarget(OutTarget))
            {
                InOutReservedSites.Add(SurveySite);
                UE_LOG(LogTemp, Display, TEXT("[MISSION TARGET] %s → survey discovered site '%s' at (%.0f, %.0f)"),
                    Vehicle->VehicleDefinition ? *Vehicle->VehicleDefinition->VehicleName.ToString() : TEXT("Vehicle"),
                    *SurveySite->SiteName, OutTarget.X, OutTarget.Y);
                return true;
            }
        }

        if (Exploration && Exploration->PickSpokePatrolTarget(Vehicle->HomeBase, Vehicle, OutTarget)
            && ValidateReconTarget(OutTarget))
        {
            Exploration->NotifyReconPatrolScheduled(Vehicle->HomeBase, OutTarget);
            return true;
        }

        OutTarget = PickPatrolPointWithinRange(Origin, Vehicle->CurrentRangeLeft, MinX, MinY, MaxX, MaxY);
        if (!ValidateReconTarget(OutTarget))
        {
            UE_LOG(LogTemp, Warning, TEXT("[MISSION TARGET] %s — recon spoke patrol out of range"),
                Vehicle->VehicleDefinition ? *Vehicle->VehicleDefinition->VehicleName.ToString() : TEXT("Vehicle"));
            return false;
        }

        UE_LOG(LogTemp, Verbose, TEXT("[MISSION TARGET] %s → fallback patrol (%.0f, %.0f)"),
            Vehicle->VehicleDefinition ? *Vehicle->VehicleDefinition->VehicleName.ToString() : TEXT("Vehicle"),
            OutTarget.X, OutTarget.Y);
        return true;
    }

    case EMissionType::Defensive:
    {
        UExplorationSubsystem* Exploration = GetGameInstance()->GetSubsystem<UExplorationSubsystem>();
        if (Exploration && Exploration->PickThreatBearingPatrolTarget(Vehicle->HomeBase, Vehicle, OutTarget))
        {
            return true;
        }

        const float GuardRoundTrip = FMath::Min(Vehicle->CurrentRangeLeft, 500.0f);
        OutTarget = PickPatrolPointWithinRange(Origin, GuardRoundTrip, MinX, MinY, MaxX, MaxY);

        UE_LOG(LogTemp, Verbose, TEXT("[MISSION TARGET] %s → guard patrol near '%s' at (%.0f, %.0f)"),
            Vehicle->VehicleDefinition ? *Vehicle->VehicleDefinition->VehicleName.ToString() : TEXT("Vehicle"),
            Vehicle->HomeBase ? *Vehicle->HomeBase->BaseName.ToString() : TEXT("Unknown"),
            OutTarget.X, OutTarget.Y);
        return true;
    }

    case EMissionType::Offensive:
    {
        TArray<UStrategyBase*> InRangeEnemyBases;
        for (UStrategyBase* EnemyBase : BaseMgr->GetBases(EnemyFaction))
        {
            if (!EnemyBase || !IsValidMapLocation(EnemyBase->MapLocation, MinX, MinY, MaxX, MaxY))
            {
                continue;
            }

            if (EnemyBase->BuiltOnSite && !BaseMgr->IsSiteKnownToFaction(Faction, EnemyBase->BuiltOnSite))
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
        if (URadarContactSubsystem* ContactMgr = GetGameInstance()->GetSubsystem<URadarContactSubsystem>())
        {
            FRadarContact Contact;
            if (ContactMgr->FindBestContactForInterception(Faction, Vehicle->HomeBase, Vehicle, Contact))
            {
                OutTarget = URadarContactSubsystem::GetContactInterceptPosition(Contact);
                UE_LOG(LogTemp, Verbose, TEXT("[MISSION TARGET] %s → radar contact '%s' at (%.0f, %.0f)%s"),
                    Vehicle->VehicleDefinition ? *Vehicle->VehicleDefinition->VehicleName.ToString() : TEXT("Vehicle"),
                    *Contact.TrackedVehicleName, OutTarget.X, OutTarget.Y,
                    Contact.bIsInboundThreat ? TEXT(" (inbound)") : TEXT(""));
                return true;
            }
        }

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
        float BestScore = 0.0f;
        if (FindBestSalvageTargetForVehicle(Vehicle, InOutReservedSites, BestSite, BestScore) && BestSite)
        {
            OutTarget = BestSite->Location;
            InOutReservedSites.Add(BestSite);
            UE_LOG(LogTemp, Verbose, TEXT("[MISSION TARGET] %s → salvage wreck '%s' (score %.1f) at (%.0f, %.0f)"),
                Vehicle->VehicleDefinition ? *Vehicle->VehicleDefinition->VehicleName.ToString() : TEXT("Vehicle"),
                *BestSite->SiteName, BestScore, OutTarget.X, OutTarget.Y);
            return true;
        }
        break;
    }

    case EMissionType::BaseExpansion:
    {
        if (Vehicle->CurrentMission && Vehicle->CurrentMission->TargetExpansionSite)
        {
            OutTarget = Vehicle->CurrentMission->TargetExpansionSite->Location;
            UE_LOG(LogTemp, Verbose, TEXT("[MISSION TARGET] %s → expansion site '%s' at (%.0f, %.0f)"),
                Vehicle->VehicleDefinition ? *Vehicle->VehicleDefinition->VehicleName.ToString() : TEXT("Vehicle"),
                *Vehicle->CurrentMission->TargetExpansionSite->SiteName, OutTarget.X, OutTarget.Y);
            return true;
        }
        break;
    }

    default:
        break;
    }

    return false;
}

/** Assigns targets and begins live movement for mission fleet. */
bool UMissionManagerSubsystem::ActivateLiveMovementForVehicles(UMissionGroup* Mission, EMissionType MissionType)
{
    if (!Mission)
    {
        return false;
    }

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
    case EMissionType::BaseExpansion:
        SearchHours = 0.5f;
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
        if (MissionType == EMissionType::Interception)
        {
            if (URadarContactSubsystem* ContactMgr = GetGameInstance()->GetSubsystem<URadarContactSubsystem>())
            {
                FRadarContact Contact;
                if (ContactMgr->FindBestContactForInterception(Mission->AttackingFaction, Vehicle->HomeBase,
                    Vehicle, Contact))
                {
                    Mission->TargetContactId = Contact.ContactId;
                    Mission->TargetInterceptVehicle = ContactMgr->ResolveTrackedVehicle(Contact, Mission->AttackingFaction);
                    ContactMgr->MarkContactTargeted(Contact.ContactId);
                }
            }
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
        UE_LOG(LogTemp, Warning, TEXT("[LIVE MISSION] No vehicles launched — mission cancelled"));
        return false;
    }

    return true;
}

/** Computes staggered launch hour across the 24h day. */
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

/** True if vehicle is on any in-progress mission. */
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

/** Removes deferred missions from prior days that never launched. */
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

/** True when vehicle is docked and assigned to this mission. */
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

/** Returns mission-ready parked vehicles at a base. */
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

/** Unparks vehicles and sets home hangar before launch. */
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

/** Activates deferred missions whose launch hour has arrived. */
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
        if (!ActivateLiveMovementForVehicles(Mission, Mission->MissionType))
        {
            ToCancel.Add(Mission);
            continue;
        }

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

/** Schedules the same mission type for all idle vehicles at a base. */
int32 UMissionManagerSubsystem::ScheduleVehicleMissionsForBase(UStrategyBase* Base, EFactionType Faction, EMissionType MissionType)
{
    const TArray<UStrategyVehicle*> IdleVehicles = GatherIdleVehiclesAtBase(Base);
    TArray<EMissionType> MissionTypes;
    MissionTypes.Init(MissionType, IdleVehicles.Num());
    return ScheduleVehicleMissionsForBase(Base, Faction, MissionTypes);
}

/** Schedules per-vehicle mission types for all idle vehicles at a base. */
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

/** Logs placeholder when offensive mission reaches enemy base. */
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

/** Creates mission, assigns crew, and launches or defers movement. */
UMissionGroup* UMissionManagerSubsystem::StartMission(UStrategyBase* OriginBase, TArray<UStrategyVehicle*> Vehicles, int32 DurationDays, const TArray<UStrategySoldier*>& SoldiersToAssign, EMissionType MissionType, EFactionType AttackingFaction, float ScheduledLaunchGameHours,
    UStrategySiteDefinition* ExpansionSite, const FText& ExpansionBaseName)
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

    if (MissionType == EMissionType::BaseExpansion && ExpansionSite)
    {
        NewMission->TargetExpansionSite = ExpansionSite;
        NewMission->PendingExpansionBaseName = ExpansionBaseName;
    }

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
        if (!ActivateLiveMovementForVehicles(NewMission, MissionType))
        {
            ActiveMissions.Remove(NewMission);
            return nullptr;
        }

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

/** Launches a single-vehicle BaseExpansion mission to race for and guard a site. */
bool UMissionManagerSubsystem::StartBaseExpansionMission(UStrategyBase* OriginBase, UStrategyVehicle* Vehicle,
    UStrategySiteDefinition* TargetSite, FText BaseName, EFactionType Faction)
{
    if (!OriginBase || !Vehicle || !TargetSite)
    {
        return false;
    }

    UMissionGroup* Mission = StartMission(OriginBase, { Vehicle }, 0, {}, EMissionType::BaseExpansion,
        Faction, -1.f, TargetSite, BaseName);

    return Mission != nullptr && Mission->bMovementActivated;
}

/** Active + in-flight BaseExpansion missions for a faction. */
int32 UMissionManagerSubsystem::CountActiveExpansionMissions(EFactionType Faction) const
{
    int32 Count = 0;
    for (const UMissionGroup* Mission : ActiveMissions)
    {
        if (Mission && Mission->MissionType == EMissionType::BaseExpansion
            && Mission->AttackingFaction == Faction
            && Mission->Status == EMissionStatus::InProgress)
        {
            Count++;
        }
    }
    return Count;
}

/** Parked + deferred-launch vehicles that may be reassigned to guard an expansion site. */
TArray<UStrategyVehicle*> UMissionManagerSubsystem::GatherExpansionCandidateVehicles(UStrategyBase* Base) const
{
    TArray<UStrategyVehicle*> Candidates;

    if (!Base)
    {
        return Candidates;
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
            if (!Vehicle || Vehicle->IsDestroyed())
            {
                continue;
            }

            if (Vehicle->GetMissionPhase() != EVehicleMissionPhase::Docked)
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

            Candidates.Add(Vehicle);
        }
    }

    return Candidates;
}

/** Removes a docked vehicle from a deferred mission so expansion can launch immediately. */
bool UMissionManagerSubsystem::UnassignVehicleFromDeferredMission(UStrategyVehicle* Vehicle, bool bAllowDefensivePreempt)
{
    if (!Vehicle || Vehicle->IsDestroyed() || Vehicle->GetMissionPhase() != EVehicleMissionPhase::Docked)
    {
        return false;
    }

    UMissionGroup* Mission = Vehicle->CurrentMission;
    if (!Mission || Mission->bMovementActivated || Mission->Status != EMissionStatus::InProgress)
    {
        return Vehicle->CurrentMission == nullptr;
    }

    const EMissionType MissionType = Mission->MissionType;
    if (MissionType == EMissionType::Interception
        || MissionType == EMissionType::Defensive)
    {
        if (!bAllowDefensivePreempt)
        {
            return false;
        }
    }

    Mission->VehiclesInFleet.Remove(Vehicle);
    Vehicle->CurrentMission = nullptr;

    for (UStrategySoldier* Soldier : Vehicle->CurrentPassengers)
    {
        if (Soldier && Soldier->CurrentMission == Mission)
        {
            Soldier->CurrentMission = nullptr;
        }
    }
    Vehicle->CurrentPassengers.Empty();

    if (Mission->TargetContactId.IsValid())
    {
        if (URadarContactSubsystem* ContactMgr = GetGameInstance()->GetSubsystem<URadarContactSubsystem>())
        {
            ContactMgr->UnmarkContactTargeted(Mission->TargetContactId);
        }
    }

    if (Mission->VehiclesInFleet.Num() == 0)
    {
        ActiveMissions.Remove(Mission);
    }

    UE_LOG(LogTemp, Display, TEXT("[MISSION] Preempted deferred %s for %s — reassigned to base expansion"),
        *UEnum::GetValueAsString(MissionType),
        Vehicle->VehicleDefinition ? *Vehicle->VehicleDefinition->VehicleName.ToString() : TEXT("Vehicle"));

    return true;
}

/** Resolves wreck site from explicit mission target, lead ActiveSalvageSite, or waypoint/position lookup. */
UStrategySiteDefinition* UMissionManagerSubsystem::GetSalvageTargetSite(const UMissionGroup* Mission) const
{
    if (!Mission || Mission->MissionType != EMissionType::Salvage)
    {
        return nullptr;
    }

    if (Mission->TargetSalvageSite)
    {
        return Mission->TargetSalvageSite;
    }

    if (Mission->VehiclesInFleet.Num() > 0 && Mission->VehiclesInFleet[0])
    {
        const UStrategyVehicle* Lead = Mission->VehiclesInFleet[0];
        if (Lead->ActiveSalvageSite)
        {
            return Lead->ActiveSalvageSite;
        }

        if (Lead->CurrentWaypoints.Num() >= 2)
        {
            return FindSiteAtLocation(Lead->CurrentWaypoints[1]);
        }

        return FindSiteAtLocation(Lead->CurrentPosition);
    }

    return nullptr;
}

/** Collects faction, origin base, fleet vehicles, and unique passengers for contest presentation. */
FSalvageContestForceSnapshot UMissionManagerSubsystem::BuildSalvageContestSnapshot(const UMissionGroup* Mission) const
{
    FSalvageContestForceSnapshot Snapshot;
    if (!Mission)
    {
        return Snapshot;
    }

    Snapshot.Faction = Mission->AttackingFaction;
    Snapshot.OriginBase = Mission->OriginBase;
    Snapshot.Vehicles = Mission->VehiclesInFleet;

    TSet<UStrategySoldier*> UniqueSoldiers;
    for (UStrategyVehicle* Vehicle : Mission->VehiclesInFleet)
    {
        if (!Vehicle)
        {
            continue;
        }

        for (UStrategySoldier* Soldier : Vehicle->CurrentPassengers)
        {
            if (Soldier)
            {
                UniqueSoldiers.Add(Soldier);
            }
        }
    }

    Snapshot.Soldiers = UniqueSoldiers.Array();
    Snapshot.EstimatedSalvageCapacity = 0;
    return Snapshot;
}

/** Activates campaign salvage contest, pauses clock, and broadcasts contest start with force snapshots. */
void UMissionManagerSubsystem::BeginSalvageContest(UStrategySiteDefinition* Site, UMissionGroup* HumanMission,
    UMissionGroup* EnemyMission)
{
    if (!Site || !HumanMission || !EnemyMission)
    {
        return;
    }

    UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();
    if (!Campaign || Campaign->IsSalvageContestActive())
    {
        return;
    }

    const FSalvageContestForceSnapshot HumanSnapshot = BuildSalvageContestSnapshot(HumanMission);
    const FSalvageContestForceSnapshot EnemySnapshot = BuildSalvageContestSnapshot(EnemyMission);

    Campaign->ActivateSalvageContest(Site, HumanMission, EnemyMission, HumanSnapshot, EnemySnapshot);
    Campaign->PauseStrategicClock();

    if (UStrategyEventDispatcher* EventDisp = GetGameInstance()->GetSubsystem<UStrategyEventDispatcher>())
    {
        EventDisp->OnSalvageContestStarted.Broadcast(Site, HumanSnapshot, EnemySnapshot);
    }

    UE_LOG(LogTemp, Display, TEXT("[SALVAGE CONTEST] Contest started at '%s' — Human vs Enemy salvage fleets (clock paused)"),
        *Site->SiteName);
}

/** Clears ActiveSalvageSite on fleet vehicles and optionally sets Returning behavior (contest loser / abort). */
void UMissionManagerSubsystem::AbortSalvageMission(UMissionGroup* Mission, bool bReturnVehiclesHome)
{
    if (!Mission || Mission->MissionType != EMissionType::Salvage)
    {
        return;
    }

    for (UStrategyVehicle* Vehicle : Mission->VehiclesInFleet)
    {
        if (!Vehicle || Vehicle->IsDestroyed())
        {
            continue;
        }

        Vehicle->ActiveSalvageSite = nullptr;

        if (bReturnVehiclesHome && Vehicle->GetMissionPhase() != EVehicleMissionPhase::Docked)
        {
            Vehicle->SetBehavior(EVehicleBehavior::Returning);
        }
    }

    UE_LOG(LogTemp, Display, TEXT("[SALVAGE CONTEST] Salvage mission aborted for %s (%d vehicle(s))"),
        *UEnum::GetValueAsString(Mission->AttackingFaction), Mission->VehiclesInFleet.Num());
}

/** Pairs Human/Enemy salvage missions on the same wreck SiteId and starts the first contested salvage found. */
void UMissionManagerSubsystem::DetectSalvageContests()
{
    UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();
    if (!Campaign || !Campaign->bSalvageMissionsEnabled || !Campaign->bSalvageSitesEnabled || Campaign->IsSalvageContestActive())
    {
        return;
    }

    TMap<FGuid, UMissionGroup*> HumanMissionBySite;
    TMap<FGuid, UMissionGroup*> EnemyMissionBySite;

    for (UMissionGroup* Mission : ActiveMissions)
    {
        if (!Mission || Mission->MissionType != EMissionType::Salvage || !Mission->bMovementActivated)
        {
            continue;
        }

        UStrategySiteDefinition* Site = GetSalvageTargetSite(Mission);
        if (!Site || Site->SalvageState != ESalvageSiteState::Active)
        {
            continue;
        }

        const FGuid SiteKey = Site->SiteId;
        if (Mission->AttackingFaction == EFactionType::Human)
        {
            HumanMissionBySite.Add(SiteKey, Mission);
        }
        else if (Mission->AttackingFaction == EFactionType::Enemy)
        {
            EnemyMissionBySite.Add(SiteKey, Mission);
        }
    }

    for (const TPair<FGuid, UMissionGroup*>& HumanPair : HumanMissionBySite)
    {
        UMissionGroup* const* EnemyMission = EnemyMissionBySite.Find(HumanPair.Key);
        if (!EnemyMission || !*EnemyMission)
        {
            continue;
        }

        UStrategySiteDefinition* Site = GetSalvageTargetSite(HumanPair.Value);
        if (!Site)
        {
            Site = GetSalvageTargetSite(*EnemyMission);
        }

        BeginSalvageContest(Site, HumanPair.Value, *EnemyMission);
        return;
    }
}

/** Ticks live movement, launches, radar, and mission completion. */
void UMissionManagerSubsystem::UpdateAllLiveVehicles(float DeltaGameHours)
{
    if (DeltaGameHours <= 0.0f)
    {
        return;
    }

    DetectSalvageContests();

    if (UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>())
    {
        if (Campaign->IsSalvageContestActive())
        {
            return;
        }
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

    if (UFactionIntelSubsystem* IntelMgr = GetGameInstance()->GetSubsystem<UFactionIntelSubsystem>())
    {
        IntelMgr->ClearFreshIntelFlags();
    }

    if (URadarContactSubsystem* ContactMgr = GetGameInstance()->GetSubsystem<URadarContactSubsystem>())
    {
        ContactMgr->TickBaseRadar(CurrentHours, DeltaGameHours);
    }
}

/** On vehicle destruction: creates salvage wreck via BaseManager when enabled, records combat winner, processes crew. */
void UMissionManagerSubsystem::HandleVehicleDestroyed(UStrategyVehicle* Vehicle, UStrategyVehicle* DestroyedBy)
{
    if (!Vehicle || !Vehicle->IsDestroyed() || Vehicle->bWreckSalvageProcessed)
    {
        return;
    }

    UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();

    if (Vehicle->CurrentMission && Vehicle->CurrentMission->MissionType == EMissionType::BaseExpansion && BaseMgr)
    {
        UStrategyBase* ExpansionBase = Vehicle->CurrentMission->ExpansionBaseUnderConstruction.Get();
        if (!ExpansionBase && Vehicle->CurrentMission->TargetExpansionSite)
        {
            ExpansionBase = BaseMgr->FindExpansionBaseAtSite(Vehicle->CurrentMission->TargetExpansionSite);
        }

        if (ExpansionBase && !BaseMgr->IsCommandCenterOperational(ExpansionBase))
        {
            BaseMgr->CancelExpansionConstruction(ExpansionBase, Vehicle->CurrentMission->TargetExpansionSite);
            Vehicle->CurrentMission->ExpansionBaseUnderConstruction = nullptr;
        }
    }

    UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();
    const bool bSalvageSitesEnabled = !Campaign || Campaign->bSalvageSitesEnabled;

    UStrategySiteDefinition* WreckSite = nullptr;
    if (bSalvageSitesEnabled && BaseMgr)
    {
        WreckSite = BaseMgr->CreateSalvageSite(Vehicle->CurrentPosition, Vehicle);
    }

    if (WreckSite)
    {
        EFactionType WinnerFaction = EFactionType::Neutral;
        if (DestroyedBy && DestroyedBy->HomeBase)
        {
            WinnerFaction = DestroyedBy->HomeBase->OwningFaction;
        }

        int32 CurrentDay = 1;
        if (UTimeManagerSubsystem* TimeMgr = GetGameInstance()->GetSubsystem<UTimeManagerSubsystem>())
        {
            CurrentDay = TimeMgr->GetSimulationDayNumber();
        }

        RecordCombatSalvageWreck(WreckSite, WinnerFaction, CurrentDay);

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

    const FString DestroyContext = (DestroyedBy && DestroyedBy->CurrentPhase == EVehicleMissionPhase::Combat)
        ? TEXT("combat")
        : TEXT("destroyed");

    if (WreckSite)
    {
        UE_LOG(LogTemp, Display, TEXT("[WRECK] Vehicle '%s' %s — salvage site created at (%.0f, %.0f)"),
            Vehicle->VehicleDefinition ? *Vehicle->VehicleDefinition->VehicleName.ToString() : TEXT("Unknown"),
            *DestroyContext,
            Vehicle->CurrentPosition.X, Vehicle->CurrentPosition.Y);
    }

    Vehicle->bWreckSalvageProcessed = true;
}

/** Launches mission with optional vehicle subset override. */
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

/** Docks survivors, grants rewards, and completes the mission. */
void UMissionManagerSubsystem::ResolveMissionOutcome(UMissionGroup* Mission)
{
    if (!Mission || !Mission->OriginBase || Mission->VehiclesInFleet.Num() == 0)
        return;

    if (Mission->TargetContactId.IsValid())
    {
        if (URadarContactSubsystem* ContactMgr = GetGameInstance()->GetSubsystem<URadarContactSubsystem>())
        {
            ContactMgr->UnmarkContactTargeted(Mission->TargetContactId);
        }
    }

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
        Vehicle->ActiveExpansionSite = nullptr;
        Vehicle->bExpansionGuardActive = false;
    }

    Mission->ExpansionBaseUnderConstruction = nullptr;
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

/** Estimates fleet combat rating from soldiers and vehicles. */
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

/** Returns game instance resource manager subsystem. */
UResourceManagerSubsystem* UMissionManagerSubsystem::GetResourceManager() const { return GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>(); }
/** Returns game instance soldier manager subsystem. */
USoldierManagerSubsystem* UMissionManagerSubsystem::GetSoldierManager() const { return GetGameInstance()->GetSubsystem<USoldierManagerSubsystem>(); }