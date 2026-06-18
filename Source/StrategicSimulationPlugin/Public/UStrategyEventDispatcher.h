#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "StrategicSimulationTypes.h"
#include "UStrategySoldier.h"
#include "UResearchTechDefinition.h"
#include "UItemDefinition.h"
#include "UStrategyFacility.h"
#include "UStrategyVehicle.h"
#include "StrategicSiteDefinition.h"
#include "UStrategyEventDispatcher.generated.h"

// === ALL EVENTS — SINGLE SOURCE OF TRUTH ===

/** Fires when a soldier is added to a faction roster via recruitment or training completion. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSoldierRecruited, EFactionType, Faction, UStrategySoldier*, Soldier);

/** Fires when a faction's active roster array changes (recruit, dismiss, POW/KIA/MIA, rescue). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSoldierListChanged, EFactionType, Faction, const TArray<UStrategySoldier*>&, Soldiers);

/** Fires when a soldier's equipment changes (purchase, training loadout, or AI gear assignment). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSoldierLoadoutChanged, EFactionType, Faction, UStrategySoldier*, Soldier);

/** Reserved for soldier dismissal; not yet broadcast anywhere in the plugin. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSoldierDismissed, EFactionType, Faction, UStrategySoldier*, Soldier);

/** Fires when a laboratory research job completes (UStrategyFacility or UProductionManagerSubsystem). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnResearchCompleted, EFactionType, Faction, UResearchTechDefinition*, Tech);

/** Fires when a hanger vehicle construction job completes and the vehicle is parked. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnVehicleCompleted, EFactionType, Faction, UStrategyVehicle*, Vehicle);

/** Reserved for workshop item output; use OnProductionCompleted instead (not broadcast separately). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemProduced, EFactionType, Faction, UItemDefinition*, Item);

/** Fires when a facility build completes or the initial Command Center is placed at game start. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFacilityCompleted, EFactionType, Faction, UStrategyFacility*, Facility);

/** Fires when a workshop item production job completes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnProductionCompleted, EFactionType, Faction, UItemDefinition*, Item);

/** Reserved for calendar month rollover; not yet broadcast anywhere in the plugin. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMonthlyEvent, int32, Month);

/** Fires when UBaseManagerSubsystem::CreateSalvageSite registers a new vehicle wreck on the map. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSalvageSiteCreated, EFactionType, WreckOwnerFaction, const TArray<EFactionType>&, KnownFactions, UStrategySiteDefinition*, Site);

/** Fires when a faction discovers a site for the first time via AddDiscoveredSite (radar, combat, etc.). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSiteDiscovered, EFactionType, Faction, UStrategySiteDefinition*, Site, EDiscoveryReason, Reason);

/** Fires when URadarContactSubsystem creates or updates a passive radar track for Faction. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRadarContactUpdated, EFactionType, Faction, FRadarContact, Contact);

/** Fires when a radar contact exceeds RadarContactExpiryHours and is pruned from the contact map. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRadarContactExpired, EFactionType, Faction, FRadarContact, Contact);

/** Fires when Enemy passive radar first detects an inbound threat and player notification is enabled. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnOpposingFactionRadarAlert, FRadarContact, Contact, FText, AlertMessage);

/** Fires when a salvage wreck is removed from the map (depleted or expired). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSalvageSiteRemoved, FGuid, SiteId, EFactionType, LastSalvagingFaction);

/** Fires when UMissionManagerSubsystem::BeginSalvageContest pauses the clock for opposing salvage fleets. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSalvageContestStarted, UStrategySiteDefinition*, Site,
    FSalvageContestForceSnapshot, HumanForceSnapshot, FSalvageContestForceSnapshot, EnemyForceSnapshot);

/** Central Blueprint-assignable event bus for strategic simulation UI and test hooks. */
UCLASS()
class STRATEGICSIMULATIONPLUGIN_API UStrategyEventDispatcher : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /** Logs that the event dispatcher is ready for UI subscriptions. */
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /** @see FOnSoldierRecruited — USoldierManagerSubsystem::RecruitSoldier / FinishSoldierTraining. */
    UPROPERTY(BlueprintAssignable, Category = "Events") FOnSoldierRecruited OnSoldierRecruited;

    /** @see FOnSoldierListChanged — USoldierManagerSubsystem::BroadcastSoldierListChanged / FinishSoldierTraining. */
    UPROPERTY(BlueprintAssignable, Category = "Events") FOnSoldierListChanged OnSoldierListChanged;

    /** @see FOnSoldierLoadoutChanged — UEngineeringManagerSubsystem::PurchaseItem, FinishSoldierTraining, AI gear. */
    UPROPERTY(BlueprintAssignable, Category = "Events") FOnSoldierLoadoutChanged OnSoldierLoadoutChanged;

    /** @see FOnSoldierDismissed — reserved, not yet fired. */
    UPROPERTY(BlueprintAssignable, Category = "Events") FOnSoldierDismissed OnSoldierDismissed;

    /** @see FOnResearchCompleted — UStrategyFacility job completion and UProductionManagerSubsystem. */
    UPROPERTY(BlueprintAssignable, Category = "Events") FOnResearchCompleted OnResearchCompleted;

    /** @see FOnVehicleCompleted — UStrategyFacility hanger job completion. */
    UPROPERTY(BlueprintAssignable, Category = "Events") FOnVehicleCompleted OnVehicleCompleted;

    /** @see FOnItemProduced — reserved alias; OnProductionCompleted is used instead. */
    UPROPERTY(BlueprintAssignable, Category = "Events") FOnItemProduced OnItemProduced;

    /** @see FOnFacilityCompleted — UStrategyFacility self-build and UBaseManagerSubsystem initial Command Center. */
    UPROPERTY(BlueprintAssignable, Category = "Events") FOnFacilityCompleted OnFacilityCompleted;

    /** @see FOnProductionCompleted — UStrategyFacility workshop item job completion. */
    UPROPERTY(BlueprintAssignable, Category = "Events") FOnProductionCompleted OnProductionCompleted;

    /** @see FOnMonthlyEvent — reserved, not yet fired. */
    UPROPERTY(BlueprintAssignable, Category = "Events") FOnMonthlyEvent OnMonthlyEvent;

    /** @see FOnSalvageSiteCreated — UBaseManagerSubsystem::CreateSalvageSite. */
    UPROPERTY(BlueprintAssignable, Category = "Events|Sites") FOnSalvageSiteCreated OnSalvageSiteCreated;

    /** @see FOnSiteDiscovered — UBaseManagerSubsystem::AddDiscoveredSite (first-time discovery only). */
    UPROPERTY(BlueprintAssignable, Category = "Events|Sites") FOnSiteDiscovered OnSiteDiscovered;

    /** @see FOnRadarContactUpdated — URadarContactSubsystem::UpsertVehicleContact (new and refresh pings). */
    UPROPERTY(BlueprintAssignable, Category = "Events|Radar") FOnRadarContactUpdated OnRadarContactUpdated;

    /** @see FOnRadarContactExpired — URadarContactSubsystem::ExpireStaleContacts. */
    UPROPERTY(BlueprintAssignable, Category = "Events|Radar") FOnRadarContactExpired OnRadarContactExpired;

    /** @see FOnOpposingFactionRadarAlert — first inbound Enemy contact when bNotifyPlayerOfEnemyRadarContacts is true. */
    UPROPERTY(BlueprintAssignable, Category = "Events|Radar") FOnOpposingFactionRadarAlert OnOpposingFactionRadarAlert;

    /** @see FOnSalvageSiteRemoved — UBaseManagerSubsystem wreck removal (expiry or depletion). */
    UPROPERTY(BlueprintAssignable, Category = "Events|Sites") FOnSalvageSiteRemoved OnSalvageSiteRemoved;

    /** @see FOnSalvageContestStarted — UMissionManagerSubsystem::BeginSalvageContest. */
    UPROPERTY(BlueprintAssignable, Category = "Events|Sites") FOnSalvageContestStarted OnSalvageContestStarted;
};