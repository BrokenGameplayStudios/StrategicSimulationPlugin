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
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    UFUNCTION(BlueprintCallable, Category = "Radar LOS")
    void SetBlockerZones(const TArray<FRadarBlockerZone>& InZones);

    UFUNCTION(BlueprintPure, Category = "Radar LOS")
    const TArray<FRadarBlockerZone>& GetBlockerZones() const { return BlockerZones; }

    UFUNCTION(BlueprintPure, Category = "Radar LOS")
    bool IsRadarLOSEnabled() const;

    /** Returns false when a blocker zone lies between From and To. */
    UFUNCTION(BlueprintPure, Category = "Radar LOS")
    bool HasRadarLineOfSight(FVector2D From, FVector2D To) const;

private:
    UPROPERTY()
    TArray<FRadarBlockerZone> BlockerZones;

    static bool SegmentIntersectsCircle(FVector2D From, FVector2D To, FVector2D Center, float Radius);
    static bool SegmentIntersectsRect(FVector2D From, FVector2D To, FVector2D Center, FVector2D HalfExtent);
};