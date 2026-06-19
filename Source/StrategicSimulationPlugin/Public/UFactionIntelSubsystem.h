#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "StrategicSimulationTypes.h"
#include "StrategicSiteDefinition.h"
#include "UFactionIntelSubsystem.generated.h"

USTRUCT(BlueprintType)
struct FSiteIntelSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Intel")
    FGuid SiteId;

    UPROPERTY(BlueprintReadOnly, Category = "Intel")
    bool bLocationKnown = false;

    UPROPERTY(BlueprintReadOnly, Category = "Intel")
    FVector2D LastKnownLocation = FVector2D::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Intel")
    FResourceStockpile LastKnownResources;

    UPROPERTY(BlueprintReadOnly, Category = "Intel")
    bool bLastKnownHasBase = false;

    UPROPERTY(BlueprintReadOnly, Category = "Intel")
    bool bHasFreshIntel = false;

    UPROPERTY(BlueprintReadOnly, Category = "Intel")
    float LastObservedGameHours = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Intel")
    EDiscoveryReason LastReason = EDiscoveryReason::Radar;
};

UCLASS()
class STRATEGICSIMULATIONPLUGIN_API UFactionIntelSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /** Initializes per-faction site intel maps (Human and Enemy). */
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /**
     * Records a ground-truth observation of a site for one faction.
     * Updates location, resources, and base-built state; marks intel as fresh for the current step.
     * No-op when stale intel is disabled or Faction is not Human/Enemy.
     */
    UFUNCTION(BlueprintCallable, Category = "Intel")
    void ObserveSite(EFactionType Faction, UStrategySiteDefinition* Site, EDiscoveryReason Reason, float ObservedGameHours);

    /** Returns whether the campaign uses stale intel (last-known vs live site state). */
    UFUNCTION(BlueprintPure, Category = "Intel")
    bool IsStaleIntelEnabled() const;

    /** Resources shown to a faction: last-known when stale intel is on, otherwise live site stockpile. */
    UFUNCTION(BlueprintPure, Category = "Intel")
    FResourceStockpile GetDisplayResources(EFactionType ViewerFaction, const UStrategySiteDefinition* Site) const;

    /** Whether the viewer believes a base exists on this site (stale-aware). */
    UFUNCTION(BlueprintPure, Category = "Intel")
    bool GetDisplayHasBase(EFactionType ViewerFaction, const UStrategySiteDefinition* Site) const;

    /** True if this site was observed during the current simulation step (stale intel only). */
    UFUNCTION(BlueprintPure, Category = "Intel")
    bool IsIntelFresh(EFactionType ViewerFaction, const UStrategySiteDefinition* Site) const;

    /** Clears bHasFreshIntel on all snapshots at end of each simulation step. */
    void ClearFreshIntelFlags();

    /** Wipes all Human and Enemy intel snapshots (new campaign / reset). */
    void ClearAllIntel();

    /** Seeds intel from discovery lists when loading a site map without saved intel data. */
    void SeedIntelFromDiscoveredSites(class UBaseManagerSubsystem* BaseManager);

    /** Serializes one faction's intel for save; fresh flags are cleared in the output. */
    TArray<FSiteIntelSnapshot> SerializeIntel(EFactionType Faction) const;

    /** Restores saved intel; falls back to discovered sites if the save array is empty. */
    void DeserializeIntel(EFactionType Faction, const TArray<FSiteIntelSnapshot>& SavedIntel,
        class UBaseManagerSubsystem* BaseManager);

private:
    UPROPERTY()
    TMap<FGuid, FSiteIntelSnapshot> HumanIntelBySiteId;

    UPROPERTY()
    TMap<FGuid, FSiteIntelSnapshot> EnemyIntelBySiteId;

    /** Returns the mutable intel map for Human or Enemy. */
    TMap<FGuid, FSiteIntelSnapshot>& GetIntelMap(EFactionType Faction);

    /** Returns the read-only intel map for Human or Enemy. */
    const TMap<FGuid, FSiteIntelSnapshot>& GetIntelMap(EFactionType Faction) const;

    /** Fills OutSnapshot with live site location, resources, and base-built state. */
    static bool CaptureGroundTruth(UStrategySiteDefinition* Site, FSiteIntelSnapshot& OutSnapshot);
};