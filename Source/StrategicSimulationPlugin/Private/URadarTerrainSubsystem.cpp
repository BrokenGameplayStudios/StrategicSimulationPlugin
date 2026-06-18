#include "URadarTerrainSubsystem.h"
#include "UStrategyCampaignSubsystem.h"
#include "Engine/Engine.h"

void URadarTerrainSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogTemp, Display, TEXT("URadarTerrainSubsystem initialized — radar LOS blocker zones ready"));
}

void URadarTerrainSubsystem::SetBlockerZones(const TArray<FRadarBlockerZone>& InZones)
{
    BlockerZones = InZones;
    UE_LOG(LogTemp, Display, TEXT("[RADAR LOS] Loaded %d blocker zone(s)"), BlockerZones.Num());
}

bool URadarTerrainSubsystem::IsRadarLOSEnabled() const
{
    if (UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>())
    {
        return Campaign->bRadarLOSEnabled;
    }
    return true;
}

bool URadarTerrainSubsystem::SegmentIntersectsCircle(FVector2D From, FVector2D To, FVector2D Center, float Radius)
{
    if (Radius <= KINDA_SMALL_NUMBER)
    {
        return false;
    }

    const FVector2D Segment = To - From;
    const float LenSq = Segment.SizeSquared();
    if (LenSq <= KINDA_SMALL_NUMBER)
    {
        return FVector2D::Distance(From, Center) <= Radius;
    }

    const float T = FMath::Clamp(FVector2D::DotProduct(Center - From, Segment) / LenSq, 0.0f, 1.0f);
    const FVector2D Closest = From + Segment * T;
    return FVector2D::Distance(Closest, Center) <= Radius;
}

bool URadarTerrainSubsystem::SegmentIntersectsRect(FVector2D From, FVector2D To, FVector2D Center, FVector2D HalfExtent)
{
    const FVector2D Min = Center - HalfExtent;
    const FVector2D Max = Center + HalfExtent;

    auto PointInside = [&](const FVector2D& P)
    {
        return P.X >= Min.X && P.X <= Max.X && P.Y >= Min.Y && P.Y <= Max.Y;
    };

    if (PointInside(From) || PointInside(To))
    {
        return true;
    }

    const FVector2D Dir = To - From;
    float TEnter = 0.0f;
    float TExit = 1.0f;

    auto ClipAxis = [&](float P, float Q) -> bool
    {
        if (FMath::IsNearlyZero(P))
        {
            return Q >= 0.0f;
        }

        const float R = Q / P;
        if (P < 0.0f)
        {
            if (R > TExit)
            {
                return false;
            }
            if (R > TEnter)
            {
                TEnter = R;
            }
        }
        else
        {
            if (R < TEnter)
            {
                return false;
            }
            if (R < TExit)
            {
                TExit = R;
            }
        }
        return true;
    };

    if (!ClipAxis(-Dir.X, From.X - Min.X))
    {
        return false;
    }
    if (!ClipAxis(Dir.X, Max.X - From.X))
    {
        return false;
    }
    if (!ClipAxis(-Dir.Y, From.Y - Min.Y))
    {
        return false;
    }
    if (!ClipAxis(Dir.Y, Max.Y - From.Y))
    {
        return false;
    }

    return TEnter <= TExit;
}

bool URadarTerrainSubsystem::HasRadarLineOfSight(FVector2D From, FVector2D To) const
{
    if (!IsRadarLOSEnabled() || BlockerZones.Num() == 0)
    {
        return true;
    }

    for (const FRadarBlockerZone& Zone : BlockerZones)
    {
        bool bBlocked = false;
        if (Zone.Shape == ERadarBlockerShape::Circle)
        {
            bBlocked = SegmentIntersectsCircle(From, To, Zone.Center, Zone.Radius);
        }
        else
        {
            bBlocked = SegmentIntersectsRect(From, To, Zone.Center, Zone.HalfExtent);
        }

        if (bBlocked)
        {
            UE_LOG(LogTemp, Verbose, TEXT("[RADAR LOS] Blocked ping from (%.0f, %.0f) to (%.0f, %.0f) — zone '%s'"),
                From.X, From.Y, To.X, To.Y,
                Zone.Label.IsEmpty() ? TEXT("unnamed") : *Zone.Label);
            return false;
        }
    }

    return true;
}