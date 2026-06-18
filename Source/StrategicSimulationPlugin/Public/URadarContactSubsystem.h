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
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    void TickBaseRadar(float CurrentGameHours, float DeltaGameHours);

    UFUNCTION(BlueprintPure, Category = "Radar Contact")
    TArray<FRadarContact> GetContactsForFaction(EFactionType Faction) const;

    UFUNCTION(BlueprintPure, Category = "Radar Contact")
    bool GetContactById(EFactionType Faction, FGuid ContactId, FRadarContact& OutContact) const;

    UFUNCTION(BlueprintPure, Category = "Radar Contact")
    bool FindBestContactForInterception(EFactionType Faction, class UStrategyBase* OriginBase,
        const UStrategyVehicle* Vehicle, FRadarContact& OutContact) const;

    UFUNCTION(BlueprintCallable, Category = "Radar Contact")
    void ClearAllContacts();

    bool IsContactAlreadyTargeted(FGuid ContactId) const;
    void MarkContactTargeted(FGuid ContactId);
    void UnmarkContactTargeted(FGuid ContactId);

    UStrategyVehicle* ResolveTrackedVehicle(const FRadarContact& Contact, EFactionType DetectingFaction) const;

    static bool IsInboundThreatVehicle(const UStrategyVehicle* EnemyVehicle, EFactionType FriendlyFaction,
        UBaseManagerSubsystem* BaseMgr);

    /** Intercept/map marker position — first detection point when available. */
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
    TSet<FGuid> ContactsWithActiveInterception;

    struct FDeferredReactiveIntercept
    {
        EFactionType Faction = EFactionType::Neutral;
        TWeakObjectPtr<UStrategyBase> Base;
        FGuid ContactId;
    };

    TArray<FDeferredReactiveIntercept> DeferredReactiveIntercepts;

    void QueueReactiveInterception(EFactionType Faction, UStrategyBase* Base, FGuid ContactId);
    void FlushDeferredReactiveInterceptions(UMissionManagerSubsystem* MissionMgr);

    TMap<FGuid, FRadarContact>& GetContactMap(EFactionType Faction);
    const TMap<FGuid, FRadarContact>& GetContactMap(EFactionType Faction) const;
    TMap<TWeakObjectPtr<UStrategyVehicle>, FGuid>& GetVehicleIdMap(EFactionType Faction);

    void ProcessBaseRadarPings(float CurrentGameHours);
    void ProcessBaseSites(class UStrategyBase* Base, EFactionType Faction, float Range, float CurrentGameHours,
        UBaseManagerSubsystem* BaseMgr, class UFactionIntelSubsystem* IntelMgr, class URadarTerrainSubsystem* TerrainMgr);
    void ProcessBaseVehicles(class UStrategyBase* Base, EFactionType Faction, float Range, float CurrentGameHours,
        UMissionManagerSubsystem* MissionMgr);
    FRadarContact UpsertVehicleContact(EFactionType DetectingFaction, UStrategyBase* DetectingBase,
        UStrategyVehicle* EnemyVehicle, float CurrentGameHours, bool bIsInboundThreat);
    void ExpireStaleContacts(float CurrentGameHours);
    void TryReactiveInterception(EFactionType Faction, UStrategyBase* Base, const FRadarContact& Contact,
        UMissionManagerSubsystem* MissionMgr);

    static FString InferThreatenedBaseName(const UStrategyVehicle* EnemyVehicle, EFactionType FriendlyFaction,
        UBaseManagerSubsystem* BaseMgr);
    static float GetBaseRadarRange();
};