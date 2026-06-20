#pragma once

#include "CoreMinimal.h"
#include "MissionPatrolRouteBuilder.generated.h"

class UStrategyBase;

/** Patrol route intent — selects dogleg / loiter geometry when building multi-waypoint paths. */
UENUM(BlueprintType)
enum class EPatrolRouteIntent : uint8
{
    BorderGuard   UMETA(DisplayName = "Border Guard"),
    VisionRing    UMETA(DisplayName = "Vision Ring"),
    SpokeSweep    UMETA(DisplayName = "Spoke Sweep"),
    SiteSurvey    UMETA(DisplayName = "Site Survey"),
    RandomPatrol  UMETA(DisplayName = "Random Patrol")
};

/** Multi-waypoint patrol route for recon and defensive missions. */
USTRUCT(BlueprintType)
struct FMissionPatrolRoute
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Patrol Route")
    EPatrolRouteIntent Intent = EPatrolRouteIntent::RandomPatrol;

    UPROPERTY(BlueprintReadOnly, Category = "Patrol Route")
    FVector2D FocalPoint = FVector2D::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Patrol Route")
    TArray<FVector2D> TravelWaypoints;

    /** First waypoint index used during on-station loiter. */
    UPROPERTY(BlueprintReadOnly, Category = "Patrol Route")
    int32 LoiterStartIndex = 0;

    /** One past the last loiter waypoint index (exclusive). */
    UPROPERTY(BlueprintReadOnly, Category = "Patrol Route")
    int32 LoiterEndIndex = 0;

    /** Closed-loop perimeter length for moving loiter (0 when stationary). */
    UPROPERTY(BlueprintReadOnly, Category = "Patrol Route")
    float LoiterPathLength = 0.0f;

    /** Total distance flown: outbound + loiter travel + return. */
    UPROPERTY(BlueprintReadOnly, Category = "Patrol Route")
    float TotalTravelDistance = 0.0f;
};

/** Builds intent-driven patrol routes with dogleg legs and racetrack or stationary loiter. */
class STRATEGICSIMULATIONPLUGIN_API MissionPatrolRouteBuilder
{
public:
    static bool BuildPatrolRoute(
        UStrategyBase* OriginBase,
        FVector2D FocalPoint,
        EPatrolRouteIntent Intent,
        FVector2D PatrolBearing,
        float MaxRangeBudget,
        float SearchHoursAtTarget,
        float CruiseSpeed,
        FMissionPatrolRoute& OutRoute);

    /** Returns true when a routable patrol path fits within the range budget. */
    static bool CanBuildPatrolRoute(
        UStrategyBase* OriginBase,
        FVector2D FocalPoint,
        EPatrolRouteIntent Intent,
        FVector2D PatrolBearing,
        float MaxRangeBudget,
        float SearchHoursAtTarget,
        float CruiseSpeed);

    /** Returns built path length, or a large value when no valid route exists. */
    static float EstimatePatrolRouteDistance(
        UStrategyBase* OriginBase,
        FVector2D FocalPoint,
        EPatrolRouteIntent Intent,
        float MaxRangeBudget,
        float SearchHoursAtTarget,
        float CruiseSpeed,
        FVector2D PatrolBearing = FVector2D::ZeroVector);

    static float ComputePolylineLength(const TArray<FVector2D>& Points, int32 StartIndex, int32 EndIndexExclusive);
    static float ComputeLoiterLoopLength(const TArray<FVector2D>& Points, int32 LoiterStart, int32 LoiterEndExclusive);

    /** On-station loiter duration varies by patrol intent so routes fit vehicle range budgets. */
    static float GetSearchHoursForPatrolIntent(EPatrolRouteIntent Intent);
};