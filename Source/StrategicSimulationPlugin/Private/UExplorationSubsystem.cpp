#include "UExplorationSubsystem.h"
#include "MissionPatrolRouteBuilder.h"
#include "StrategicSimulationTypes.h"
#include "UBaseManagerSubsystem.h"
#include "URadarContactSubsystem.h"
#include "UStrategyCampaignSubsystem.h"
#include "UStrategyBase.h"
#include "UStrategyVehicle.h"
#include "UMissionManagerSubsystem.h"
#include "UTimeManagerSubsystem.h"
#include "Engine/Engine.h"

namespace ExplorationPatrolHelpers
{
    static float GetVisionRingDistancePixels(UGameInstance* GameInstance)
    {
        if (UStrategyCampaignSubsystem* Campaign = GameInstance
            ? GameInstance->GetSubsystem<UStrategyCampaignSubsystem>()
            : nullptr)
        {
            return Campaign->BaseRadarRangePixels * UExplorationSubsystem::VisionRingOutsideFactor;
        }

        return 512.0f * UExplorationSubsystem::VisionRingOutsideFactor;
    }

    static bool ValidatePatrolCandidate(UStrategyBase* OriginBase, UStrategyVehicle* Vehicle,
        const FVector2D& Candidate, EPatrolRouteIntent Intent, const FVector2D& BearingDir)
    {
        if (!OriginBase || !Vehicle)
        {
            return false;
        }

        const float CruiseSpeed = Vehicle->CruiseSpeedPixelsPerHour > 0.0f ? Vehicle->CruiseSpeedPixelsPerHour : 180.0f;
        const float SearchHours = MissionPatrolRouteBuilder::GetSearchHoursForPatrolIntent(Intent);
        const FVector2D Bearing = (Intent == EPatrolRouteIntent::VisionRing)
            ? FVector2D(-BearingDir.Y, BearingDir.X)
            : BearingDir;

        if (!MissionPatrolRouteBuilder::CanBuildPatrolRoute(OriginBase, Candidate, Intent, Bearing,
            Vehicle->CurrentRangeLeft, SearchHours, CruiseSpeed))
        {
            return false;
        }

        const float PathDistance = MissionPatrolRouteBuilder::EstimatePatrolRouteDistance(
            OriginBase, Candidate, Intent, Vehicle->CurrentRangeLeft, SearchHours, CruiseSpeed, Bearing);
        return Vehicle->HasEnoughRangeForMission(PathDistance);
    }
}

namespace ExplorationHelpers
{
    /** Maps a direction vector from a base to one of NumSpokes evenly spaced compass indices. */
    int32 BearingToSpokeIndex(const FVector2D& DirectionFromBase)
    {
        if (DirectionFromBase.IsNearlyZero())
        {
            return 0;
        }

        const float BearingRad = FMath::Atan2(DirectionFromBase.Y, DirectionFromBase.X);
        const float Normalized = FMath::Fmod(BearingRad + 2.0f * PI, 2.0f * PI);
        const int32 SpokeIndex = FMath::RoundToInt((Normalized / (2.0f * PI)) * static_cast<float>(UExplorationSubsystem::NumSpokes));
        return SpokeIndex % UExplorationSubsystem::NumSpokes;
    }

    /** True when Contact is inbound and either detected by this base or its entry point is within radar range. */
    bool IsContactRelevantToBase(const FRadarContact& Contact, const UStrategyBase* Base, float RadarRange)
    {
        if (!Base || !Contact.bIsInboundThreat)
        {
            return false;
        }

        if (Contact.DetectingBaseName == Base->BaseName.ToString())
        {
            return true;
        }

        const FVector2D Entry = URadarContactSubsystem::GetContactInterceptPosition(Contact);
        return FVector2D::Distance(Entry, Base->MapLocation) <= RadarRange * 1.1f;
    }
}

/** Subsystem startup — logs readiness; exploration and survey state start empty. */
void UExplorationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogTemp, Display, TEXT("UExplorationSubsystem initialized — spoke patrol exploration ready"));
}

/** Clears ExplorationByBase and both factions' surveyed-site ID sets. */
void UExplorationSubsystem::ClearAllExplorationState()
{
    ExplorationByBase.Empty();
    SurveyedSiteIdsHuman.Empty();
    SurveyedSiteIdsEnemy.Empty();
}

/** Returns SurveyedSiteIdsHuman or SurveyedSiteIdsEnemy. */
const TSet<FGuid>& UExplorationSubsystem::GetSurveyedSet(EFactionType Faction) const
{
    return (Faction == EFactionType::Human) ? SurveyedSiteIdsHuman : SurveyedSiteIdsEnemy;
}

/** Mutable variant of GetSurveyedSet for MarkSiteSurveyed. */
TSet<FGuid>& UExplorationSubsystem::GetSurveyedSetMutable(EFactionType Faction)
{
    return (Faction == EFactionType::Human) ? SurveyedSiteIdsHuman : SurveyedSiteIdsEnemy;
}

/** Finds or adds per-base exploration state and ensures SpokeRingDepths has NumSpokes entries. */
FBaseExplorationState& UExplorationSubsystem::GetOrCreateState(UStrategyBase* Base)
{
    FBaseExplorationState& State = ExplorationByBase.FindOrAdd(Base);
    if (State.SpokeRingDepths.Num() != NumSpokes)
    {
        State.SpokeRingDepths.SetNum(NumSpokes);
    }
    return State;
}

/** Derives clamp bounds from campaign LogicalMapWidth/Height and MapBorderPadding. */
bool UExplorationSubsystem::ComputeMapBounds(const UObject* WorldContext, float& MinX, float& MinY, float& MaxX, float& MaxY)
{
    float MapWidth = 1920.0f;
    float MapHeight = 1080.0f;
    float MapPadding = 64.0f;

    if (WorldContext)
    {
        if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull))
        {
            if (UStrategyCampaignSubsystem* Campaign = World->GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>())
            {
                MapWidth = Campaign->LogicalMapWidth;
                MapHeight = Campaign->LogicalMapHeight;
                MapPadding = Campaign->MapBorderPadding;
            }
        }
    }

    MinX = MapPadding;
    MinY = MapPadding;
    MaxX = MapWidth - MapPadding;
    MaxY = MapHeight - MapPadding;
    return MaxX > MinX && MaxY > MinY;
}

/** Clamps InOutPoint to the map interior; returns true if either axis was adjusted. */
bool UExplorationSubsystem::ClampToMap(FVector2D& InOutPoint, float MinX, float MinY, float MaxX, float MaxY)
{
    const FVector2D Before = InOutPoint;
    InOutPoint.X = FMath::Clamp(InOutPoint.X, MinX, MaxX);
    InOutPoint.Y = FMath::Clamp(InOutPoint.Y, MinY, MaxY);
    return !InOutPoint.Equals(Before, 1.0f);
}

namespace ExplorationSurveyHelpers
{
    static bool IsSiteOccupiedByAnyBase(const UBaseManagerSubsystem* BaseMgr, const UStrategySiteDefinition* Site)
    {
        if (!BaseMgr || !Site)
        {
            return false;
        }

        auto HasBaseOnSite = [Site](const TArray<UStrategyBase*>& Bases) -> bool
        {
            for (const UStrategyBase* Base : Bases)
            {
                if (Base && Base->BuiltOnSite == Site)
                {
                    return true;
                }
            }
            return false;
        };

        return HasBaseOnSite(BaseMgr->GetBases(EFactionType::Human))
            || HasBaseOnSite(BaseMgr->GetBases(EFactionType::Enemy));
    }
}

/** Picks nearest in-range discovered site not yet in the faction surveyed set. */
bool UExplorationSubsystem::FindSurveyTarget(UStrategyVehicle* Vehicle, UStrategySiteDefinition*& OutSite) const
{
    OutSite = nullptr;
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
    const TArray<UStrategySiteDefinition*>& Discovered =
        (Faction == EFactionType::Human) ? BaseMgr->DiscoveredSitesHuman : BaseMgr->DiscoveredSitesEnemy;
    const TSet<FGuid>& Surveyed = GetSurveyedSet(Faction);
    const FVector2D Origin = Vehicle->HomeBase->MapLocation;
    const float CruiseSpeed = Vehicle->CruiseSpeedPixelsPerHour > 0.0f ? Vehicle->CruiseSpeedPixelsPerHour : 180.0f;
    const float SurveySearchHours = MissionPatrolRouteBuilder::GetSearchHoursForPatrolIntent(EPatrolRouteIntent::SiteSurvey);

    UStrategySiteDefinition* Best = nullptr;
    float BestDist = MAX_FLT;

    for (UStrategySiteDefinition* Site : Discovered)
    {
        if (!Site || !Site->SiteId.IsValid())
        {
            continue;
        }

        if (Surveyed.Contains(Site->SiteId))
        {
            continue;
        }

        if (Site->SiteType != EStrategySiteType::PotentialBase && Site->SiteType != EStrategySiteType::ResourceNode)
        {
            continue;
        }

        if (ExplorationSurveyHelpers::IsSiteOccupiedByAnyBase(BaseMgr, Site))
        {
            continue;
        }

        const float Dist = FVector2D::Distance(Origin, Site->Location);
        if (Dist < SiteMatchTolerance)
        {
            continue;
        }

        const FVector2D Bearing = (Site->Location - Origin).GetSafeNormal();
        if (!MissionPatrolRouteBuilder::CanBuildPatrolRoute(Vehicle->HomeBase, Site->Location,
            EPatrolRouteIntent::SiteSurvey, Bearing, Vehicle->CurrentRangeLeft, SurveySearchHours, CruiseSpeed))
        {
            continue;
        }

        const float PathDistance = MissionPatrolRouteBuilder::EstimatePatrolRouteDistance(
            Vehicle->HomeBase, Site->Location, EPatrolRouteIntent::SiteSurvey, Vehicle->CurrentRangeLeft,
            SurveySearchHours, CruiseSpeed, Bearing);
        if (Dist < BestDist && Vehicle->HasEnoughRangeForMission(PathDistance))
        {
            BestDist = Dist;
            Best = Site;
        }
    }

    if (Best)
    {
        OutSite = Best;
        return true;
    }

    return false;
}

int32 UExplorationSubsystem::SelectExplorationSpoke(const FBaseExplorationState& State)
{
    int32 MinDepth = MAX_int32;
    for (int32 Depth : State.SpokeRingDepths)
    {
        MinDepth = FMath::Min(MinDepth, Depth);
    }

    for (int32 Offset = 0; Offset < NumSpokes; ++Offset)
    {
        const int32 SpokeIndex = (State.NextSpokeIndex + Offset) % NumSpokes;
        if (State.SpokeRingDepths.IsValidIndex(SpokeIndex) && State.SpokeRingDepths[SpokeIndex] == MinDepth)
        {
            return SpokeIndex;
        }
    }

    return State.NextSpokeIndex % NumSpokes;
}

bool UExplorationSubsystem::AreAllSpokesInitiallySwept(const FBaseExplorationState& State)
{
    for (int32 Depth : State.SpokeRingDepths)
    {
        if (Depth < 1)
        {
            return false;
        }
    }

    return State.SpokeRingDepths.Num() == NumSpokes;
}

bool UExplorationSubsystem::ShouldScheduleWatchBorder(const FBaseExplorationState& State)
{
    if (State.WatchSpokeIndices.Num() == 0)
    {
        return false;
    }

    if (!AreAllSpokesInitiallySwept(State))
    {
        return false;
    }

    return State.MissionsSinceWatchPatrol >= MissionsBetweenWatchPatrol;
}

void UExplorationSubsystem::MarkWatchSpoke(FBaseExplorationState& State, int32 SpokeIndex, float CurrentGameHours)
{
    State.WatchSpokeIndices.AddUnique(SpokeIndex);
    State.HotSpokeIndices.AddUnique(SpokeIndex);
    State.HotSpokeUntilGameHours = CurrentGameHours + HotSpokeDurationHours;
}

/** Sweeps each spoke at the shallowest ring before deepening; ring 0 reaches as far as range allows. */
bool UExplorationSubsystem::PickSpokeExplorationTarget(UStrategyBase* OriginBase, UStrategyVehicle* Vehicle,
    FVector2D& OutTarget, EPatrolRouteIntent& OutIntent, FVector2D& OutPatrolBearing, int32& OutSpokeIndex)
{
    OutTarget = FVector2D::ZeroVector;
    OutIntent = EPatrolRouteIntent::SpokeSweep;
    OutPatrolBearing = FVector2D::ZeroVector;
    OutSpokeIndex = 0;

    if (!OriginBase || !Vehicle)
    {
        return false;
    }

    float MinX, MinY, MaxX, MaxY;
    if (!ComputeMapBounds(OriginBase, MinX, MinY, MaxX, MaxY))
    {
        return false;
    }

    const FVector2D Origin = OriginBase->MapLocation;
    FBaseExplorationState& State = GetOrCreateState(OriginBase);

    float CurrentGameHours = 0.0f;
    if (UTimeManagerSubsystem* TimeMgr = GetGameInstance()->GetSubsystem<UTimeManagerSubsystem>())
    {
        CurrentGameHours = TimeMgr->GetElapsedSimulationHours();
    }

    if (CurrentGameHours > State.HotSpokeUntilGameHours)
    {
        State.HotSpokeIndices.Empty();
        State.HotSpokeUntilGameHours = 0.0f;
    }

    const int32 SpokeIndex = SelectExplorationSpoke(State);
    OutSpokeIndex = SpokeIndex;

    const int32 RingDepth = State.SpokeRingDepths.IsValidIndex(SpokeIndex) ? State.SpokeRingDepths[SpokeIndex] : 0;
    const float BearingRad = (static_cast<float>(SpokeIndex) / static_cast<float>(NumSpokes)) * 2.0f * PI;
    const FVector2D BearingDir(FMath::Cos(BearingRad), FMath::Sin(BearingRad));

    const float VisionRingDist = ExplorationPatrolHelpers::GetVisionRingDistancePixels(GetGameInstance());
    const float MaxOutbound = FMath::Max(VisionRingDist + static_cast<float>(RingDepth + 1) * SpokeStepPixels,
        Vehicle->CurrentRangeLeft * 0.45f);

    float Dist = (RingDepth == 0)
        ? VisionRingDist
        : FMath::Min(VisionRingDist + static_cast<float>(RingDepth) * SpokeStepPixels, MaxOutbound);

    OutIntent = (RingDepth == 0) ? EPatrolRouteIntent::VisionRing : EPatrolRouteIntent::SpokeSweep;
    const float MinDist = FMath::Max(SpokeStepPixels * 2.0f, VisionRingDist * 0.5f);
    FVector2D Candidate = FVector2D::ZeroVector;
    bool bFoundRoute = false;

    for (int32 Attempt = 0; Attempt < 12; ++Attempt)
    {
        Candidate = Origin + BearingDir * Dist;
        ClampToMap(Candidate, MinX, MinY, MaxX, MaxY);

        if (ExplorationPatrolHelpers::ValidatePatrolCandidate(OriginBase, Vehicle, Candidate, OutIntent, BearingDir))
        {
            bFoundRoute = true;
            break;
        }

        Dist *= 0.9f;
        if (Dist < MinDist)
        {
            break;
        }
    }

    if (!bFoundRoute)
    {
        return false;
    }

    OutTarget = Candidate;
    OutPatrolBearing = (OutIntent == EPatrolRouteIntent::VisionRing)
        ? FVector2D(-BearingDir.Y, BearingDir.X)
        : BearingDir;

    UE_LOG(LogTemp, Display, TEXT("[EXPLORATION] %s spoke %d ring %d (%s) → (%.0f, %.0f) from '%s'"),
        *UEnum::GetValueAsString(OriginBase->OwningFaction),
        SpokeIndex, RingDepth + 1,
        OutIntent == EPatrolRouteIntent::VisionRing ? TEXT("first sweep") : TEXT("expand"),
        OutTarget.X, OutTarget.Y, *OriginBase->BaseName.ToString());

    return true;
}

/** Tangential patrol along the vision ring on a watch-sector spoke after enemy contact was seen there. */
bool UExplorationSubsystem::PickWatchSectorBorderPatrol(UStrategyBase* OriginBase, UStrategyVehicle* Vehicle,
    FVector2D& OutTarget, FVector2D& OutPatrolBearing, int32& OutSpokeIndex) const
{
    OutTarget = FVector2D::ZeroVector;
    OutPatrolBearing = FVector2D::ZeroVector;
    OutSpokeIndex = 0;

    if (!OriginBase || !Vehicle)
    {
        return false;
    }

    const FBaseExplorationState* StatePtr = ExplorationByBase.Find(OriginBase);
    if (!StatePtr || !ShouldScheduleWatchBorder(*StatePtr))
    {
        return false;
    }

    const FBaseExplorationState& State = *StatePtr;

    float MinX, MinY, MaxX, MaxY;
    if (!ComputeMapBounds(OriginBase, MinX, MinY, MaxX, MaxY))
    {
        return false;
    }

    const FVector2D Origin = OriginBase->MapLocation;
    const int32 WatchListIndex = State.NextWatchSpokeIndex % State.WatchSpokeIndices.Num();
    const int32 SpokeIndex = State.WatchSpokeIndices[WatchListIndex];
    OutSpokeIndex = SpokeIndex;

    const float BearingRad = (static_cast<float>(SpokeIndex) / static_cast<float>(NumSpokes)) * 2.0f * PI;
    const FVector2D BearingDir(FMath::Cos(BearingRad), FMath::Sin(BearingRad));

    float Dist = ExplorationPatrolHelpers::GetVisionRingDistancePixels(GetGameInstance());
    const float MinDist = FMath::Max(SpokeStepPixels * 2.0f, Dist * 0.55f);
    FVector2D Candidate = FVector2D::ZeroVector;
    bool bFoundRoute = false;

    for (int32 Attempt = 0; Attempt < 10; ++Attempt)
    {
        Candidate = Origin + BearingDir * Dist;
        ClampToMap(Candidate, MinX, MinY, MaxX, MaxY);

        if (ExplorationPatrolHelpers::ValidatePatrolCandidate(OriginBase, Vehicle, Candidate,
            EPatrolRouteIntent::VisionRing, BearingDir))
        {
            bFoundRoute = true;
            break;
        }

        Dist *= 0.9f;
        if (Dist < MinDist)
        {
            break;
        }
    }

    if (!bFoundRoute)
    {
        return false;
    }

    OutTarget = Candidate;
    OutPatrolBearing = FVector2D(-BearingDir.Y, BearingDir.X);

    UE_LOG(LogTemp, Display, TEXT("[EXPLORATION] %s watch-sector spoke %d → border patrol (%.0f, %.0f) from '%s'"),
        *UEnum::GetValueAsString(OriginBase->OwningFaction),
        SpokeIndex, OutTarget.X, OutTarget.Y, *OriginBase->BaseName.ToString());

    return true;
}

/**
 * Border-guard patrol toward the best inbound contact's radar entry lane.
 * Uses GetContactInterceptPosition for the entry point and offsets along threat velocity for ambush positioning.
 */
bool UExplorationSubsystem::PickInboundEntryPatrolTarget(UStrategyBase* OriginBase, UStrategyVehicle* Vehicle,
    FVector2D& OutTarget, FVector2D* OutPatrolBearing) const
{
    OutTarget = FVector2D::ZeroVector;
    if (!OriginBase || !Vehicle)
    {
        return false;
    }

    URadarContactSubsystem* ContactMgr = GetGameInstance()->GetSubsystem<URadarContactSubsystem>();
    UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();
    if (!ContactMgr)
    {
        return false;
    }

    const EFactionType Faction = OriginBase->OwningFaction;
    const FVector2D Origin = OriginBase->MapLocation;
    const float RadarRange = Campaign ? Campaign->BaseRadarRangePixels : 512.0f;

    float CurrentGameHours = 0.0f;
    if (UTimeManagerSubsystem* TimeMgr = GetGameInstance()->GetSubsystem<UTimeManagerSubsystem>())
    {
        CurrentGameHours = TimeMgr->GetElapsedSimulationHours();
    }

    const FRadarContact* BestContact = nullptr;
    float BestScore = -MAX_FLT;

    for (const FRadarContact& Contact : ContactMgr->GetContactsForFaction(Faction))
    {
        if (!ExplorationHelpers::IsContactRelevantToBase(Contact, OriginBase, RadarRange))
        {
            continue;
        }

        const float AgeHours = FMath::Max(0.0f, CurrentGameHours - Contact.LastSeenGameHours);
        float Score = 1000.0f - AgeHours;
        if (Contact.DetectingBaseName == OriginBase->BaseName.ToString())
        {
            Score += 250.0f;
        }

        if (Score > BestScore)
        {
            BestScore = Score;
            BestContact = &Contact;
        }
    }

    if (!BestContact)
    {
        return false;
    }

    float MinX, MinY, MaxX, MaxY;
    if (!ComputeMapBounds(OriginBase, MinX, MinY, MaxX, MaxY))
    {
        return false;
    }

    const FVector2D Entry = URadarContactSubsystem::GetContactInterceptPosition(*BestContact);
    FVector2D AmbushDir = BestContact->EstimatedVelocity.GetSafeNormal();
    if (AmbushDir.IsNearlyZero())
    {
        AmbushDir = (Origin - Entry).GetSafeNormal();
    }

    OutTarget = Entry + AmbushDir * AmbushLaneOffsetPixels;
    ClampToMap(OutTarget, MinX, MinY, MaxX, MaxY);

    if (OutPatrolBearing)
    {
        *OutPatrolBearing = AmbushDir;
    }

    const float RoundTrip = FVector2D::Distance(Origin, OutTarget) * 2.0f;
    if (!Vehicle->HasEnoughRangeForMission(RoundTrip))
    {
        OutTarget = Entry;
        ClampToMap(OutTarget, MinX, MinY, MaxX, MaxY);
        if (!Vehicle->HasEnoughRangeForMission(FVector2D::Distance(Origin, OutTarget) * 2.0f))
        {
            return false;
        }
    }

    UE_LOG(LogTemp, Display, TEXT("[PATROL] %s border guard from '%s' → entry lane (%.0f, %.0f) watching %s"),
        *UEnum::GetValueAsString(Faction),
        *OriginBase->BaseName.ToString(),
        OutTarget.X, OutTarget.Y,
        *BestContact->TrackedVehicleName);

    return true;
}

/** Delegates to PickInboundEntryPatrolTarget, then centroid-of-inbound-entry fallback patrol bearing. */
bool UExplorationSubsystem::PickThreatBearingPatrolTarget(UStrategyBase* OriginBase, UStrategyVehicle* Vehicle,
    FVector2D& OutTarget) const
{
    if (PickInboundEntryPatrolTarget(OriginBase, Vehicle, OutTarget, nullptr))
    {
        return true;
    }

    OutTarget = FVector2D::ZeroVector;
    if (!OriginBase || !Vehicle)
    {
        return false;
    }

    URadarContactSubsystem* ContactMgr = GetGameInstance()->GetSubsystem<URadarContactSubsystem>();
    if (!ContactMgr)
    {
        return false;
    }

    const EFactionType Faction = OriginBase->OwningFaction;
    const FVector2D Origin = OriginBase->MapLocation;
    FVector2D ThreatCentroid = FVector2D::ZeroVector;
    int32 ThreatCount = 0;

    for (const FRadarContact& Contact : ContactMgr->GetContactsForFaction(Faction))
    {
        if (!Contact.bIsInboundThreat)
        {
            continue;
        }

        ThreatCentroid += URadarContactSubsystem::GetContactInterceptPosition(Contact);
        ++ThreatCount;
    }

    if (ThreatCount == 0)
    {
        return false;
    }

    ThreatCentroid /= static_cast<float>(ThreatCount);
    FVector2D ThreatDir = (ThreatCentroid - Origin).GetSafeNormal();
    if (ThreatDir.IsNearlyZero())
    {
        return false;
    }

    float MinX, MinY, MaxX, MaxY;
    if (!ComputeMapBounds(OriginBase, MinX, MinY, MaxX, MaxY))
    {
        return false;
    }

    const float PatrolDist = FMath::Min(FMath::Max(120.0f, Vehicle->CurrentRangeLeft * 0.35f), 450.0f);
    OutTarget = Origin + ThreatDir * PatrolDist;
    ClampToMap(OutTarget, MinX, MinY, MaxX, MaxY);

    const float RoundTrip = FVector2D::Distance(Origin, OutTarget) * 2.0f;
    if (!Vehicle->HasEnoughRangeForMission(RoundTrip))
    {
        return false;
    }

    UE_LOG(LogTemp, Display, TEXT("[PATROL AI] %s threat-bearing guard from '%s' → (%.0f, %.0f)"),
        *UEnum::GetValueAsString(Faction), *OriginBase->BaseName.ToString(), OutTarget.X, OutTarget.Y);

    return true;
}

/** Scans faction contacts for any inbound track relevant to OriginBase via IsContactRelevantToBase. */
bool UExplorationSubsystem::HasInboundThreatsNearBase(const UStrategyBase* OriginBase) const
{
    if (!OriginBase)
    {
        return false;
    }

    URadarContactSubsystem* ContactMgr = GetGameInstance()->GetSubsystem<URadarContactSubsystem>();
    UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();
    if (!ContactMgr)
    {
        return false;
    }

    const float RadarRange = Campaign ? Campaign->BaseRadarRangePixels : 512.0f;
    for (const FRadarContact& Contact : ContactMgr->GetContactsForFaction(OriginBase->OwningFaction))
    {
        if (ExplorationHelpers::IsContactRelevantToBase(Contact, OriginBase, RadarRange))
        {
            return true;
        }
    }

    return false;
}

/** Marks the spoke toward GetContactInterceptPosition(Contact) as hot for HotSpokeDurationHours. */
void UExplorationSubsystem::NotifyInboundThreatContact(UStrategyBase* DetectingBase, const FRadarContact& Contact,
    float CurrentGameHours)
{
    if (!DetectingBase || !Contact.bIsInboundThreat)
    {
        return;
    }

    const FVector2D Entry = URadarContactSubsystem::GetContactInterceptPosition(Contact);
    const FVector2D DirFromBase = (Entry - DetectingBase->MapLocation).GetSafeNormal();
    const int32 SpokeIndex = ExplorationHelpers::BearingToSpokeIndex(DirFromBase);

    FBaseExplorationState& State = GetOrCreateState(DetectingBase);
    MarkWatchSpoke(State, SpokeIndex, CurrentGameHours);

    UE_LOG(LogTemp, Display, TEXT("[PATROL] %s marked spoke %d hot+watch after inbound %s at entry (%.0f, %.0f)"),
        *UEnum::GetValueAsString(DetectingBase->OwningFaction),
        SpokeIndex,
        *Contact.TrackedVehicleName,
        Entry.X, Entry.Y);
}

void UExplorationSubsystem::NotifyReconContactSeen(UStrategyBase* OriginBase, FVector2D ContactPosition,
    float CurrentGameHours)
{
    if (!OriginBase || ContactPosition.IsNearlyZero(10.0f))
    {
        return;
    }

    const FVector2D DirFromBase = (ContactPosition - OriginBase->MapLocation).GetSafeNormal();
    if (DirFromBase.IsNearlyZero())
    {
        return;
    }

    const int32 SpokeIndex = ExplorationHelpers::BearingToSpokeIndex(DirFromBase);
    FBaseExplorationState& State = GetOrCreateState(OriginBase);
    MarkWatchSpoke(State, SpokeIndex, CurrentGameHours);

    UE_LOG(LogTemp, Display, TEXT("[EXPLORATION] %s recon saw contact toward spoke %d at (%.0f, %.0f) from '%s'"),
        *UEnum::GetValueAsString(OriginBase->OwningFaction),
        SpokeIndex, ContactPosition.X, ContactPosition.Y, *OriginBase->BaseName.ToString());
}

void UExplorationSubsystem::NotifySpokeExplorationScheduled(UStrategyBase* OriginBase, int32 SpokeIndex)
{
    if (!OriginBase)
    {
        return;
    }

    FBaseExplorationState& State = GetOrCreateState(OriginBase);
    if (State.SpokeRingDepths.IsValidIndex(SpokeIndex))
    {
        State.SpokeRingDepths[SpokeIndex]++;
    }

    State.NextSpokeIndex = (SpokeIndex + 1) % NumSpokes;
    State.MissionsSinceWatchPatrol++;
}

void UExplorationSubsystem::NotifyWatchBorderPatrolScheduled(UStrategyBase* OriginBase)
{
    if (!OriginBase)
    {
        return;
    }

    FBaseExplorationState& State = GetOrCreateState(OriginBase);
    if (State.WatchSpokeIndices.Num() > 0)
    {
        State.NextWatchSpokeIndex = (State.NextWatchSpokeIndex + 1) % State.WatchSpokeIndices.Num();
    }

    State.MissionsSinceWatchPatrol = 0;
}

/** Adds Site->SiteId to the faction surveyed set so FindSurveyTarget skips it. */
void UExplorationSubsystem::MarkSiteSurveyed(EFactionType Faction, const UStrategySiteDefinition* Site)
{
    if (!Site || !Site->SiteId.IsValid())
    {
        return;
    }

    GetSurveyedSetMutable(Faction).Add(Site->SiteId);

    UE_LOG(LogTemp, Verbose, TEXT("[EXPLORATION] %s surveyed site '%s'"),
        *UEnum::GetValueAsString(Faction), *Site->SiteName);
}