#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "StrategicSimulationTypes.h"
#include "URadarContactSubsystem.generated.h"

class UBaseManagerSubsystem;
class UMissionManagerSubsystem;
class UStrategyBase;
class UStrategyVehicle;

UCLASS()
class STRATEGICSIMULATIONPLUGIN_API URadarContactSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /** Initializes passive radar contact tracking for both factions. */
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /**
     * Accumulates simulation time and fires base radar pings when the campaign ping interval elapses.
     * Called from live vehicle updates; no-op when base passive radar is disabled.
     */
    void TickBaseRadar(float CurrentGameHours, float DeltaGameHours);

    /** Returns a copy of all active radar contacts visible to the given faction. */
    UFUNCTION(BlueprintPure, Category = "Radar Contact")
    TArray<FRadarContact> GetContactsForFaction(EFactionType Faction) const;

    /** Looks up a single contact by stable ContactId within a faction's contact map. */
    UFUNCTION(BlueprintPure, Category = "Radar Contact")
    bool GetContactById(EFactionType Faction, FGuid ContactId, FRadarContact& OutContact) const;

    /**
     * Picks the highest-scoring untargeted contact a vehicle can intercept from OriginBase.
     * Prefers inbound threats and uses GetContactInterceptPosition for range checks.
     */
    UFUNCTION(BlueprintPure, Category = "Radar Contact")
    bool FindBestContactForInterception(EFactionType Faction, class UStrategyBase* OriginBase,
        const UStrategyVehicle* Vehicle, FRadarContact& OutContact) const;

    /** Clears all contacts, vehicle-to-contact mappings, interception targets, and ping accumulator. */
    UFUNCTION(BlueprintCallable, Category = "Radar Contact")
    void ClearAllContacts();

    /** True when an interception mission is already assigned to this ContactId. */
    bool IsContactAlreadyTargeted(FGuid ContactId) const;

    /** Marks a contact as claimed so AI/player cannot launch duplicate interceptions. */
    void MarkContactTargeted(FGuid ContactId);

    /** Releases a contact after mission completion, failure, or contact expiry. */
    void UnmarkContactTargeted(FGuid ContactId);

    /**
     * Resolves the live enemy vehicle backing a contact by name and proximity to LastPosition.
     * Used when missions need the actual UObject rather than radar track data.
     */
    UStrategyVehicle* ResolveTrackedVehicle(const FRadarContact& Contact, EFactionType DetectingFaction) const;

    /**
     * Classifies whether an enemy vehicle is heading toward friendly territory
     * (offensive mission, interception mission, or proximity to nearest friendly base).
     */
    static bool IsInboundThreatVehicle(const UStrategyVehicle* EnemyVehicle, EFactionType FriendlyFaction,
        UBaseManagerSubsystem* BaseMgr);

    /**
     * Map/intercept anchor for a radar contact.
     * Returns FirstDetectedPosition when the track recorded an entry point; otherwise LastPosition.
     * Used by interception range checks, patrol targeting, and inbound-threat UI.
     */
    UFUNCTION(BlueprintPure, Category = "Radar Contact")
    static FVector2D GetContactInterceptPosition(const FRadarContact& Contact);

private:
    UPROPERTY()
    TMap<FGuid, FRadarContact> HumanContactsById;

    UPROPERTY()
    TMap<FGuid, FRadarContact> EnemyContactsById;

    TMap<TWeakObjectPtr<UStrategyVehicle>, FGuid> HumanVehicleContactIds;
    TMap<TWeakObjectPtr<UStrategyVehicle>, FGuid> EnemyVehicleContactIds;

    float AccumulatedPingHours = 0.0f;

    /** ContactIds already assigned to an active interception mission. */
    TSet<FGuid> ContactsWithActiveInterception;

    /** Pending reactive-intercept request queued during a radar ping (processed after all bases ping). */
    struct FDeferredReactiveIntercept
    {
        EFactionType Faction = EFactionType::Neutral;
        TWeakObjectPtr<UStrategyBase> Base;
        FGuid ContactId;
    };

    /**
     * Deferred reactive intercept queue.
     * Inbound threats discovered during ProcessBaseVehicles are queued here instead of launching
     * immediately, so all bases finish pinging before TryReactiveInterception runs.
     * FlushDeferredReactiveInterceptions drains the queue once per ping cycle.
     */
    TArray<FDeferredReactiveIntercept> DeferredReactiveIntercepts;

    /** Enqueues a reactive intercept if the contact is not already targeted or duplicate-queued. */
    void QueueReactiveInterception(EFactionType Faction, UStrategyBase* Base, FGuid ContactId);

    /**
     * Drains DeferredReactiveIntercepts after a full ping pass.
     * Skips stale bases, expired contacts, and contacts already targeted; otherwise calls TryReactiveInterception.
     */
    void FlushDeferredReactiveInterceptions(UMissionManagerSubsystem* MissionMgr);

    /** Returns the mutable contact map for Human or Enemy. */
    TMap<FGuid, FRadarContact>& GetContactMap(EFactionType Faction);

    /** Returns the read-only contact map for Human or Enemy. */
    const TMap<FGuid, FRadarContact>& GetContactMap(EFactionType Faction) const;

    /** Returns the vehicle-to-ContactId map for stable upsert across ping cycles. */
    TMap<TWeakObjectPtr<UStrategyVehicle>, FGuid>& GetVehicleIdMap(EFactionType Faction);

    /**
     * One full radar cycle: every operational command center pings sites and enemy vehicles,
     * then flushes deferred intercepts and expires stale contacts.
     */
    void ProcessBaseRadarPings(float CurrentGameHours);

    /**
     * Radar-pings undiscovered or known sites within range; records intel for already-known sites.
     * Respects terrain line-of-sight when enabled.
     */
    void ProcessBaseSites(class UStrategyBase* Base, EFactionType Faction, float Range, float CurrentGameHours,
        UBaseManagerSubsystem* BaseMgr, class UFactionIntelSubsystem* IntelMgr, class URadarTerrainSubsystem* TerrainMgr);

    /**
     * Tracks enemy fleet vehicles within radar range; queues deferred reactive intercepts for inbound threats.
     */
    void ProcessBaseVehicles(class UStrategyBase* Base, EFactionType Faction, float Range, float CurrentGameHours,
        UMissionManagerSubsystem* MissionMgr);

    /**
     * Creates or updates a vehicle contact, preserving FirstDetectedPosition and velocity estimates.
     * Notifies exploration and broadcasts OnRadarContactUpdated (and player alert for new enemy inbound tracks).
     */
    FRadarContact UpsertVehicleContact(EFactionType DetectingFaction, UStrategyBase* DetectingBase,
        UStrategyVehicle* EnemyVehicle, float CurrentGameHours, bool bIsInboundThreat, float DetectingRadarRangePixels);

    /** Removes contacts older than RadarContactExpiryHours and cleans vehicle/interception bookkeeping. */
    void ExpireStaleContacts(float CurrentGameHours);

    /**
     * Attempts to launch a reactive interception from idle combat vehicles at Base.
     * Requires campaign AI reactive interception, faction AI simulation, and an inbound threat contact.
     */
    void TryReactiveInterception(EFactionType Faction, UStrategyBase* Base, const FRadarContact& Contact,
        UMissionManagerSubsystem* MissionMgr);

    /** Best-effort name of the friendly base threatened by an inbound enemy vehicle. */
    static FString InferThreatenedBaseName(const UStrategyVehicle* EnemyVehicle, EFactionType FriendlyFaction,
        UBaseManagerSubsystem* BaseMgr);

    /** Default base radar range in pixels when campaign settings are unavailable. */
    static float GetBaseRadarRange();
};