#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "StrategicSimulationTypes.h"
#include "URadarTerrainSubsystem.generated.h"

UCLASS()
class STRATEGICSIMULATIONPLUGIN_API URadarTerrainSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /** Initializes radar line-of-sight blocker zone storage. */
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /** Replaces the blocker zone list (typically from map/campaign setup). */
    UFUNCTION(BlueprintCallable, Category = "Radar LOS")
    void SetBlockerZones(const TArray<FRadarBlockerZone>& InZones);

    /** Returns the current blocker zones used for radar LOS tests. */
    UFUNCTION(BlueprintPure, Category = "Radar LOS")
    const TArray<FRadarBlockerZone>& GetBlockerZones() const { return BlockerZones; }

    /** True when the campaign enables terrain blocking of radar pings. */
    UFUNCTION(BlueprintPure, Category = "Radar LOS")
    bool IsRadarLOSEnabled() const;

    /**
     * Tests whether a straight segment from From to To is unobstructed by blocker zones.
     * Returns true when LOS is disabled, no zones exist, or no zone intersects the segment.
     */
    UFUNCTION(BlueprintPure, Category = "Radar LOS")
    bool HasRadarLineOfSight(FVector2D From, FVector2D To) const;

private:
    UPROPERTY()
    TArray<FRadarBlockerZone> BlockerZones;

    /** True when the segment From–To passes within Radius of Center (circle blocker). */
    static bool SegmentIntersectsCircle(FVector2D From, FVector2D To, FVector2D Center, float Radius);

    /** True when the segment From–To intersects or lies inside the axis-aligned rect blocker. */
    static bool SegmentIntersectsRect(FVector2D From, FVector2D To, FVector2D Center, FVector2D HalfExtent);
};