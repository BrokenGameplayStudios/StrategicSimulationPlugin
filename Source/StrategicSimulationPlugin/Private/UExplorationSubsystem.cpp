#include "UExplorationSubsystem.h"
#include "UBaseManagerSubsystem.h"
#include "URadarContactSubsystem.h"
#include "UStrategyCampaignSubsystem.h"
#include "UStrategyBase.h"
#include "UStrategyVehicle.h"
#include "UMissionManagerSubsystem.h"
#include "Engine/Engine.h"

void UExplorationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogTemp, Display, TEXT("UExplorationSubsystem initialized — spoke patrol exploration ready"));
}

void UExplorationSubsystem::ClearAllExplorationState()
{
    ExplorationByBase.Empty();
    SurveyedSiteIdsHuman.Empty();
    SurveyedSiteIdsEnemy.Empty();
}

const TSet<FGuid>& UExplorationSubsystem::GetSurveyedSet(EFactionType Faction) const
{
    return (Faction == EFactionType::Human) ? SurveyedSiteIdsHuman : SurveyedSiteIdsEnemy;
}

TSet<FGuid>& UExplorationSubsystem::GetSurveyedSetMutable(EFactionType Faction)
{
    return (Faction == EFactionType::Human) ? SurveyedSiteIdsHuman : SurveyedSiteIdsEnemy;
}

FBaseExplorationState& UExplorationSubsystem::GetOrCreateState(UStrategyBase* Base)
{
    FBaseExplorationState& State = ExplorationByBase.FindOrAdd(Base);
    if (State.SpokeRingDepths.Num() != NumSpokes)
    {
        State.SpokeRingDepths.SetNum(NumSpokes);
    }
    return State;
}

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

bool UExplorationSubsystem::ClampToMap(FVector2D& InOutPoint, float MinX, float MinY, float MaxX, float MaxY)
{
    const FVector2D Before = InOutPoint;
    InOutPoint.X = FMath::Clamp(InOutPoint.X, MinX, MaxX);
    InOutPoint.Y = FMath::Clamp(InOutPoint.Y, MinY, MaxY);
    return !InOutPoint.Equals(Before, 1.0f);
}

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

        const float Dist = FVector2D::Distance(Origin, Site->Location);
        const float RoundTrip = Dist * 2.0f;
        if (Dist < BestDist && Vehicle->HasEnoughRangeForMission(RoundTrip))
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

bool UExplorationSubsystem::PickSpokePatrolTarget(UStrategyBase* OriginBase, UStrategyVehicle* Vehicle, FVector2D& OutTarget)
{
    OutTarget = FVector2D::ZeroVector;
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
    const float MaxOutbound = FMath::Max(80.0f, Vehicle->CurrentRangeLeft * 0.45f);

    FBaseExplorationState& State = GetOrCreateState(OriginBase);
    const int32 SpokeIndex = State.NextSpokeIndex % NumSpokes;
    const float BearingRad = (static_cast<float>(SpokeIndex) / static_cast<float>(NumSpokes)) * 2.0f * PI;
    const FVector2D BearingDir(FMath::Cos(BearingRad), FMath::Sin(BearingRad));

    const int32 RingDepth = State.SpokeRingDepths.IsValidIndex(SpokeIndex)
        ? State.SpokeRingDepths[SpokeIndex] + 1
        : 1;

    float Dist = FMath::Min(static_cast<float>(RingDepth) * SpokeStepPixels, MaxOutbound);
    FVector2D Candidate = Origin + BearingDir * Dist;
    ClampToMap(Candidate, MinX, MinY, MaxX, MaxY);

    const float RoundTrip = FVector2D::Distance(Origin, Candidate) * 2.0f;
    if (!Vehicle->HasEnoughRangeForMission(RoundTrip))
    {
        Dist = MaxOutbound * 0.65f;
        Candidate = Origin + BearingDir * Dist;
        ClampToMap(Candidate, MinX, MinY, MaxX, MaxY);

        if (!Vehicle->HasEnoughRangeForMission(FVector2D::Distance(Origin, Candidate) * 2.0f))
        {
            return false;
        }
    }

    ClampToMap(Candidate, MinX, MinY, MaxX, MaxY);

    const float FinalRoundTrip = FVector2D::Distance(Origin, Candidate) * 2.0f;
    if (!Vehicle->HasEnoughRangeForMission(FinalRoundTrip))
    {
        return false;
    }

    OutTarget = Candidate;

    UE_LOG(LogTemp, Display, TEXT("[EXPLORATION] %s spoke %d ring %d → patrol (%.0f, %.0f) from '%s'"),
        *UEnum::GetValueAsString(OriginBase->OwningFaction),
        SpokeIndex, RingDepth, OutTarget.X, OutTarget.Y, *OriginBase->BaseName.ToString());

    return true;
}

bool UExplorationSubsystem::PickThreatBearingPatrolTarget(UStrategyBase* OriginBase, UStrategyVehicle* Vehicle,
    FVector2D& OutTarget) const
{
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

        ThreatCentroid += Contact.LastPosition;
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

void UExplorationSubsystem::NotifyReconPatrolScheduled(UStrategyBase* OriginBase, const FVector2D& PatrolTarget)
{
    if (!OriginBase)
    {
        return;
    }

    FBaseExplorationState& State = GetOrCreateState(OriginBase);
    const int32 SpokeIndex = State.NextSpokeIndex % NumSpokes;
    if (State.SpokeRingDepths.IsValidIndex(SpokeIndex))
    {
        State.SpokeRingDepths[SpokeIndex]++;
    }

    State.NextSpokeIndex = (State.NextSpokeIndex + 1) % NumSpokes;
    (void)PatrolTarget;
}

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