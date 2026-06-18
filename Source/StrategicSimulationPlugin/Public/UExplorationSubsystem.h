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

    /** Next spoke index for round-robin spoke-and-wheel patrol. */
    int32 NextSpokeIndex = 0;

    UPROPERTY()
    TArray<int32> SpokeRingDepths;

    /** Spokes to re-patrol sooner after inbound radar contacts (PR-15). */
    UPROPERTY()
    TArray<int32> HotSpokeIndices;

    /** Game hours until hot-spoke bias expires and normal spoke rotation resumes. */
    UPROPERTY()
    float HotSpokeUntilGameHours = 0.0f;
};

UCLASS()
class STRATEGICSIMULATIONPLUGIN_API UExplorationSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /** Initializes spoke-patrol and survey tracking state. */
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /** Clears per-base exploration state and surveyed-site ID sets for both factions. */
    void ClearAllExplorationState();

    /**
     * Fog-fair recon target: nearest discovered PotentialBase/ResourceNode not yet surveyed on-station.
     * Requires round-trip range from the vehicle's home base.
     */
    bool FindSurveyTarget(UStrategyVehicle* Vehicle, UStrategySiteDefinition*& OutSite) const;

    /**
     * Spoke-and-wheel patrol: advances one of NumSpokes bearings outward from OriginBase.
     * Honors hot-spoke bias after inbound contacts; clamps to map bounds and vehicle range.
     */
    bool PickSpokePatrolTarget(UStrategyBase* OriginBase, UStrategyVehicle* Vehicle, FVector2D& OutTarget);

    /**
     * Threat-weighted guard patrol: tries PickInboundEntryPatrolTarget first, then falls back
     * to a bearing toward the centroid of all inbound contact entry points.
     */
    bool PickThreatBearingPatrolTarget(UStrategyBase* OriginBase, UStrategyVehicle* Vehicle, FVector2D& OutTarget) const;

    /**
     * Border-guard patrol toward an inbound threat's radar entry lane (PR-15).
     * Selects the freshest relevant inbound contact for this base, uses GetContactInterceptPosition
     * as the entry point, and offsets along estimated threat velocity by AmbushLaneOffsetPixels.
     * Falls back to the raw entry point if the ambush offset exceeds vehicle range.
     */
    bool PickInboundEntryPatrolTarget(UStrategyBase* OriginBase, UStrategyVehicle* Vehicle, FVector2D& OutTarget) const;

    /** True when the base's faction has active inbound radar tracks relevant to this base. */
    bool HasInboundThreatsNearBase(const UStrategyBase* OriginBase) const;

    /**
     * Called when radar first detects an inbound threat at a base.
     * Marks the spoke toward the contact entry point as "hot" for accelerated re-patrol.
     */
    void NotifyInboundThreatContact(UStrategyBase* DetectingBase, const FRadarContact& Contact, float CurrentGameHours);

    /** Advances spoke ring depth and NextSpokeIndex after a recon patrol is scheduled. */
    void NotifyReconPatrolScheduled(UStrategyBase* OriginBase, const FVector2D& PatrolTarget);

    /** Records that a faction has completed on-station survey of a site (fog-of-war fairness). */
    void MarkSiteSurveyed(EFactionType Faction, const UStrategySiteDefinition* Site);

    static constexpr int32 NumSpokes = 8;
    static constexpr float SpokeStepPixels = 140.0f;
    static constexpr float HotSpokeDurationHours = 12.0f;
    static constexpr float AmbushLaneOffsetPixels = 48.0f;

private:
    TMap<TWeakObjectPtr<UStrategyBase>, FBaseExplorationState> ExplorationByBase;

    TSet<FGuid> SurveyedSiteIdsHuman;
    TSet<FGuid> SurveyedSiteIdsEnemy;

    /** Returns or creates exploration state for a base, initializing spoke ring depths. */
    FBaseExplorationState& GetOrCreateState(UStrategyBase* Base);

    /** Const access to the surveyed-site ID set for Human or Enemy. */
    const TSet<FGuid>& GetSurveyedSet(EFactionType Faction) const;

    /** Mutable access to the surveyed-site ID set for Human or Enemy. */
    TSet<FGuid>& GetSurveyedSetMutable(EFactionType Faction);

    /** Reads logical map bounds from campaign settings (with safe defaults). */
    static bool ComputeMapBounds(const UObject* WorldContext, float& MinX, float& MinY, float& MaxX, float& MaxY);

    /** Clamps a patrol target inside map padding; returns true if the point was adjusted. */
    static bool ClampToMap(FVector2D& InOutPoint, float MinX, float MinY, float MaxX, float MaxY);
};