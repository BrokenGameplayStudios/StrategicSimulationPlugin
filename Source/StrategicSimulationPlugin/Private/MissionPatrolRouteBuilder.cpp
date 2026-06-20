#include "MissionPatrolRouteBuilder.h"
#include "StrategicSimulationTypes.h"
#include "UExplorationSubsystem.h"
#include "UStrategyBase.h"

namespace PatrolRouteHelpers
{
    static FVector2D Perpendicular(const FVector2D& Dir)
    {
        return FVector2D(-Dir.Y, Dir.X);
    }

    static void ClampPoint(FVector2D& Point, float MinX, float MinY, float MaxX, float MaxY)
    {
        Point.X = FMath::Clamp(Point.X, MinX, MaxX);
        Point.Y = FMath::Clamp(Point.Y, MinY, MaxY);
    }

    static void AppendDoglegLeg(const FVector2D& Start, const FVector2D& End, float LateralOffset,
        TArray<FVector2D>& OutPoints, float MinX, float MinY, float MaxX, float MaxY)
    {
        const FVector2D ToEnd = End - Start;
        const float Dist = ToEnd.Size();
        if (Dist <= KINDA_SMALL_NUMBER)
        {
            FVector2D ClampedEnd = End;
            ClampPoint(ClampedEnd, MinX, MinY, MaxX, MaxY);
            if (!OutPoints.Last().Equals(ClampedEnd, 1.0f))
            {
                OutPoints.Add(ClampedEnd);
            }
            return;
        }

        const FVector2D Dir = ToEnd / Dist;
        const FVector2D Perp = Perpendicular(Dir);
        FVector2D Mid = Start + Dir * (Dist * 0.45f) + Perp * LateralOffset;
        ClampPoint(Mid, MinX, MinY, MaxX, MaxY);
        OutPoints.Add(Mid);

        FVector2D ClampedEnd = End;
        ClampPoint(ClampedEnd, MinX, MinY, MaxX, MaxY);
        OutPoints.Add(ClampedEnd);
    }

    static void AppendRacetrackLoop(const FVector2D& Center, const FVector2D& BearingDir, float Length, float Width,
        TArray<FVector2D>& OutLoop, float MinX, float MinY, float MaxX, float MaxY)
    {
        FVector2D Forward = BearingDir.GetSafeNormal();
        if (Forward.IsNearlyZero())
        {
            Forward = FVector2D(1.0f, 0.0f);
        }

        const FVector2D Right = Perpendicular(Forward);
        const float HalfL = Length * 0.5f;
        const float HalfW = Width * 0.5f;

        const TArray<FVector2D> Offsets = {
            Center + Forward * HalfL,
            Center + Forward * (HalfL * 0.35f) + Right * HalfW,
            Center - Forward * HalfL,
            Center - Forward * (HalfL * 0.35f) - Right * HalfW,
            Center + Forward * (HalfL * 0.55f) - Right * (HalfW * 0.45f)
        };

        for (FVector2D Point : Offsets)
        {
            ClampPoint(Point, MinX, MinY, MaxX, MaxY);
            OutLoop.Add(Point);
        }
    }

    static bool BuildRouteInternal(
        UStrategyBase* OriginBase,
        FVector2D FocalPoint,
        EPatrolRouteIntent Intent,
        FVector2D PatrolBearing,
        float MaxRangeBudget,
        float SearchHoursAtTarget,
        float CruiseSpeed,
        float RacetrackLength,
        float RacetrackWidth,
        FMissionPatrolRoute& OutRoute)
    {
        OutRoute = FMissionPatrolRoute();
        if (!OriginBase || FocalPoint.IsNearlyZero(10.0f))
        {
            return false;
        }

        const FVector2D Origin = OriginBase->MapLocation;
        if (Origin.IsNearlyZero(10.0f))
        {
            return false;
        }

        float MinX = 0.0f;
        float MinY = 0.0f;
        float MaxX = 1920.0f;
        float MaxY = 1080.0f;
        if (!UExplorationSubsystem::ComputeMapBounds(OriginBase, MinX, MinY, MaxX, MaxY))
        {
            return false;
        }

        FVector2D Bearing = PatrolBearing.GetSafeNormal();
        if (Bearing.IsNearlyZero())
        {
            Bearing = (FocalPoint - Origin).GetSafeNormal();
        }
        if (Bearing.IsNearlyZero())
        {
            Bearing = FVector2D(1.0f, 0.0f);
        }

        const float OutboundDist = FVector2D::Distance(Origin, FocalPoint);
        if (OutboundDist < SiteMatchTolerance)
        {
            return false;
        }

        float LateralOffset = FMath::Min(60.0f, OutboundDist * 0.25f);
        if (Intent == EPatrolRouteIntent::BorderGuard)
        {
            LateralOffset = FMath::Min(72.0f, OutboundDist * 0.3f);
        }
        else if (Intent == EPatrolRouteIntent::VisionRing)
        {
            Bearing = Perpendicular(Bearing);
            LateralOffset = FMath::Min(50.0f, OutboundDist * 0.15f);
        }

        const bool bStationaryLoiter = (Intent == EPatrolRouteIntent::SiteSurvey);
        const bool bDirectLegs = (Intent == EPatrolRouteIntent::VisionRing);

        TArray<FVector2D> Waypoints;
        Waypoints.Add(Origin);

        if (bStationaryLoiter && OutboundDist < 200.0f)
        {
            FVector2D ClampedFocal = FocalPoint;
            ClampPoint(ClampedFocal, MinX, MinY, MaxX, MaxY);
            Waypoints.Add(ClampedFocal);
        }
        else if (bDirectLegs)
        {
            FVector2D ClampedFocal = FocalPoint;
            ClampPoint(ClampedFocal, MinX, MinY, MaxX, MaxY);
            Waypoints.Add(ClampedFocal);
        }
        else
        {
            AppendDoglegLeg(Origin, FocalPoint, LateralOffset, Waypoints, MinX, MinY, MaxX, MaxY);
        }

        const int32 LoiterStart = Waypoints.Num() - 1;
        int32 LoiterEnd = LoiterStart + 1;

        if (!bStationaryLoiter)
        {
            TArray<FVector2D> LoopPoints;
            AppendRacetrackLoop(FocalPoint, Bearing, RacetrackLength, RacetrackWidth, LoopPoints,
                MinX, MinY, MaxX, MaxY);

            for (const FVector2D& LoopPoint : LoopPoints)
            {
                if (!Waypoints.Last().Equals(LoopPoint, 2.0f))
                {
                    Waypoints.Add(LoopPoint);
                }
            }
            LoiterEnd = Waypoints.Num();
        }

        const FVector2D ReturnStart = Waypoints[LoiterEnd - 1];
        if (bDirectLegs)
        {
            FVector2D ClampedOrigin = Origin;
            ClampPoint(ClampedOrigin, MinX, MinY, MaxX, MaxY);
            if (!Waypoints.Last().Equals(ClampedOrigin, 2.0f))
            {
                Waypoints.Add(ClampedOrigin);
            }
        }
        else
        {
            const float ReturnLateral = -LateralOffset;
            AppendDoglegLeg(ReturnStart, Origin, ReturnLateral, Waypoints, MinX, MinY, MaxX, MaxY);
        }

        if (Waypoints.Num() < 3)
        {
            return false;
        }

        const float OutboundLength = MissionPatrolRouteBuilder::ComputePolylineLength(Waypoints, 0, LoiterStart + 1);
        const float ReturnLength = MissionPatrolRouteBuilder::ComputePolylineLength(Waypoints, LoiterEnd - 1, Waypoints.Num());
        const float LoiterLoopLength = bStationaryLoiter
            ? 0.0f
            : MissionPatrolRouteBuilder::ComputeLoiterLoopLength(Waypoints, LoiterStart, LoiterEnd);
        const float LoiterBudgetFraction = (Intent == EPatrolRouteIntent::VisionRing) ? 0.15f : 0.35f;
        const float MaxLoiterTravel = MaxRangeBudget * LoiterBudgetFraction;
        const float LoiterTravelDistance = bStationaryLoiter
            ? 0.0f
            : FMath::Min(SearchHoursAtTarget * FMath::Max(1.0f, CruiseSpeed), MaxLoiterTravel);
        const float TotalDistance = OutboundLength + LoiterTravelDistance + ReturnLength;

        OutRoute.Intent = Intent;
        OutRoute.FocalPoint = FocalPoint;
        OutRoute.TravelWaypoints = Waypoints;
        OutRoute.LoiterStartIndex = LoiterStart;
        OutRoute.LoiterEndIndex = LoiterEnd;
        OutRoute.LoiterPathLength = LoiterLoopLength;
        OutRoute.TotalTravelDistance = TotalDistance;

        return TotalDistance > 0.0f && Waypoints.Num() >= 3;
    }
}

float MissionPatrolRouteBuilder::ComputePolylineLength(const TArray<FVector2D>& Points, int32 StartIndex, int32 EndIndexExclusive)
{
    if (StartIndex < 0 || EndIndexExclusive > Points.Num() || EndIndexExclusive <= StartIndex + 1)
    {
        return 0.0f;
    }

    float TotalLength = 0.0f;
    for (int32 Index = StartIndex; Index < EndIndexExclusive - 1; ++Index)
    {
        TotalLength += FVector2D::Distance(Points[Index], Points[Index + 1]);
    }
    return TotalLength;
}

float MissionPatrolRouteBuilder::ComputeLoiterLoopLength(const TArray<FVector2D>& Points, int32 LoiterStart, int32 LoiterEndExclusive)
{
    if (LoiterEndExclusive - LoiterStart < 2)
    {
        return 0.0f;
    }

    float LoopLength = ComputePolylineLength(Points, LoiterStart, LoiterEndExclusive);
    LoopLength += FVector2D::Distance(Points[LoiterEndExclusive - 1], Points[LoiterStart]);
    return LoopLength;
}

bool MissionPatrolRouteBuilder::BuildPatrolRoute(
    UStrategyBase* OriginBase,
    FVector2D FocalPoint,
    EPatrolRouteIntent Intent,
    FVector2D PatrolBearing,
    float MaxRangeBudget,
    float SearchHoursAtTarget,
    float CruiseSpeed,
    FMissionPatrolRoute& OutRoute)
{
    if (!OriginBase || MaxRangeBudget <= 0.0f || SearchHoursAtTarget <= 0.0f || CruiseSpeed <= 0.0f)
    {
        return false;
    }

    float RacetrackLength = FMath::Min(180.0f, MaxRangeBudget * 0.12f);
    float RacetrackWidth = FMath::Min(80.0f, RacetrackLength * 0.4f);
    if (Intent == EPatrolRouteIntent::VisionRing)
    {
        RacetrackLength = FMath::Min(120.0f, MaxRangeBudget * 0.08f);
        RacetrackWidth = FMath::Min(55.0f, RacetrackLength * 0.45f);
    }
    else if (Intent == EPatrolRouteIntent::RandomPatrol)
    {
        RacetrackLength *= 0.75f;
        RacetrackWidth *= 0.75f;
    }

    FMissionPatrolRoute Candidate;
    for (int32 Attempt = 0; Attempt < 8; ++Attempt)
    {
        if (PatrolRouteHelpers::BuildRouteInternal(OriginBase, FocalPoint, Intent, PatrolBearing,
            MaxRangeBudget, SearchHoursAtTarget, CruiseSpeed, RacetrackLength, RacetrackWidth, Candidate)
            && Candidate.TotalTravelDistance <= MaxRangeBudget)
        {
            OutRoute = Candidate;
            return true;
        }

        RacetrackLength *= 0.82f;
        RacetrackWidth *= 0.82f;
    }

    return false;
}

bool MissionPatrolRouteBuilder::CanBuildPatrolRoute(
    UStrategyBase* OriginBase,
    FVector2D FocalPoint,
    EPatrolRouteIntent Intent,
    FVector2D PatrolBearing,
    float MaxRangeBudget,
    float SearchHoursAtTarget,
    float CruiseSpeed)
{
    FMissionPatrolRoute Route;
    return BuildPatrolRoute(OriginBase, FocalPoint, Intent, PatrolBearing, MaxRangeBudget,
        SearchHoursAtTarget, CruiseSpeed, Route);
}

float MissionPatrolRouteBuilder::EstimatePatrolRouteDistance(
    UStrategyBase* OriginBase,
    FVector2D FocalPoint,
    EPatrolRouteIntent Intent,
    float MaxRangeBudget,
    float SearchHoursAtTarget,
    float CruiseSpeed,
    FVector2D PatrolBearing)
{
    FMissionPatrolRoute Route;
    FVector2D Bearing = PatrolBearing.GetSafeNormal();
    if (Bearing.IsNearlyZero())
    {
        Bearing = (FocalPoint - (OriginBase ? OriginBase->MapLocation : FVector2D::ZeroVector)).GetSafeNormal();
    }

    if (BuildPatrolRoute(OriginBase, FocalPoint, Intent, Bearing, MaxRangeBudget, SearchHoursAtTarget, CruiseSpeed, Route))
    {
        return Route.TotalTravelDistance;
    }

    return MAX_FLT;
}

float MissionPatrolRouteBuilder::GetSearchHoursForPatrolIntent(EPatrolRouteIntent Intent)
{
    switch (Intent)
    {
    case EPatrolRouteIntent::SiteSurvey:
        return 3.0f;
    case EPatrolRouteIntent::VisionRing:
        return 0.75f;
    case EPatrolRouteIntent::BorderGuard:
    case EPatrolRouteIntent::SpokeSweep:
        return 1.25f;
    case EPatrolRouteIntent::RandomPatrol:
        return 1.0f;
    default:
        return 1.5f;
    }
}