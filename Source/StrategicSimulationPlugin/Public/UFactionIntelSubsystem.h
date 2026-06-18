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
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /** Capture ground-truth site state for a faction (radar ping, combat-known, on-station). */
    UFUNCTION(BlueprintCallable, Category = "Intel")
    void ObserveSite(EFactionType Faction, UStrategySiteDefinition* Site, EDiscoveryReason Reason, float ObservedGameHours);

    UFUNCTION(BlueprintPure, Category = "Intel")
    bool IsStaleIntelEnabled() const;

    UFUNCTION(BlueprintPure, Category = "Intel")
    bool HasKnownSiteLocation(EFactionType Faction, const UStrategySiteDefinition* Site) const;

    UFUNCTION(BlueprintPure, Category = "Intel")
    bool GetSiteIntelSnapshot(EFactionType Faction, const UStrategySiteDefinition* Site, FSiteIntelSnapshot& OutSnapshot) const;

    UFUNCTION(BlueprintPure, Category = "Intel")
    FResourceStockpile GetDisplayResources(EFactionType ViewerFaction, const UStrategySiteDefinition* Site) const;

    UFUNCTION(BlueprintPure, Category = "Intel")
    bool GetDisplayHasBase(EFactionType ViewerFaction, const UStrategySiteDefinition* Site) const;

    UFUNCTION(BlueprintPure, Category = "Intel")
    bool IsIntelFresh(EFactionType ViewerFaction, const UStrategySiteDefinition* Site) const;

    /** Clears fresh flags at end of each simulation step (intel becomes stale until next observation). */
    void ClearFreshIntelFlags();

    void ClearAllIntel();

    /** After site-map load without intel save data — seed snapshots from discovery lists. */
    void SeedIntelFromDiscoveredSites(class UBaseManagerSubsystem* BaseManager);

    TArray<FSiteIntelSnapshot> SerializeIntel(EFactionType Faction) const;
    void DeserializeIntel(EFactionType Faction, const TArray<FSiteIntelSnapshot>& SavedIntel,
        class UBaseManagerSubsystem* BaseManager);

private:
    UPROPERTY()
    TMap<FGuid, FSiteIntelSnapshot> HumanIntelBySiteId;

    UPROPERTY()
    TMap<FGuid, FSiteIntelSnapshot> EnemyIntelBySiteId;

    TMap<FGuid, FSiteIntelSnapshot>& GetIntelMap(EFactionType Faction);
    const TMap<FGuid, FSiteIntelSnapshot>& GetIntelMap(EFactionType Faction) const;

    static bool CaptureGroundTruth(UStrategySiteDefinition* Site, FSiteIntelSnapshot& OutSnapshot);
};