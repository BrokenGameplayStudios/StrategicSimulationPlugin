#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "StrategicSimulationTypes.h"
#include "UExplorationSubsystem.generated.h"

class UStrategyBase;
class UStrategySiteDefinition;
class UStrategyVehicle;

USTRUCT()
struct FBaseExplorationState
{
    GENERATED_BODY()

    int32 NextSpokeIndex = 0;

    UPROPERTY()
    TArray<int32> SpokeRingDepths;

    /** Spokes to re-patrol sooner after inbound radar contacts (PR-15). */
    UPROPERTY()
    TArray<int32> HotSpokeIndices;

    UPROPERTY()
    float HotSpokeUntilGameHours = 0.0f;
};

UCLASS()
class STRATEGICSIMULATIONPLUGIN_API UExplorationSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    void ClearAllExplorationState();

    /** Fog-fair recon: visit a discovered site that has not been surveyed on-station yet. */
    bool FindSurveyTarget(UStrategyVehicle* Vehicle, UStrategySiteDefinition*& OutSite) const;

    /** Spoke-and-wheel patrol expanding outward from a base (no omniscient site coords). */
    bool PickSpokePatrolTarget(UStrategyBase* OriginBase, UStrategyVehicle* Vehicle, FVector2D& OutTarget);

    /** Threat-weighted guard patrol bearing from inbound radar contacts near a base. */
    bool PickThreatBearingPatrolTarget(UStrategyBase* OriginBase, UStrategyVehicle* Vehicle, FVector2D& OutTarget) const;

    /** Patrol to first-detection entry point + ambush offset along threat heading (PR-15). */
    bool PickInboundEntryPatrolTarget(UStrategyBase* OriginBase, UStrategyVehicle* Vehicle, FVector2D& OutTarget) const;

    /** True when this base's faction has active inbound radar tracks relevant to the base. */
    bool HasInboundThreatsNearBase(const UStrategyBase* OriginBase) const;

    void NotifyInboundThreatContact(UStrategyBase* DetectingBase, const FRadarContact& Contact, float CurrentGameHours);
    void NotifyReconPatrolScheduled(UStrategyBase* OriginBase, const FVector2D& PatrolTarget);
    void MarkSiteSurveyed(EFactionType Faction, const UStrategySiteDefinition* Site);

    static constexpr int32 NumSpokes = 8;
    static constexpr float SpokeStepPixels = 140.0f;
    static constexpr float HotSpokeDurationHours = 12.0f;
    static constexpr float AmbushLaneOffsetPixels = 48.0f;

private:
    TMap<TWeakObjectPtr<UStrategyBase>, FBaseExplorationState> ExplorationByBase;

    TSet<FGuid> SurveyedSiteIdsHuman;
    TSet<FGuid> SurveyedSiteIdsEnemy;

    FBaseExplorationState& GetOrCreateState(UStrategyBase* Base);
    const TSet<FGuid>& GetSurveyedSet(EFactionType Faction) const;
    TSet<FGuid>& GetSurveyedSetMutable(EFactionType Faction);

    static bool ComputeMapBounds(const UObject* WorldContext, float& MinX, float& MinY, float& MaxX, float& MaxY);
    static bool ClampToMap(FVector2D& InOutPoint, float MinX, float MinY, float MaxX, float MaxY);
};