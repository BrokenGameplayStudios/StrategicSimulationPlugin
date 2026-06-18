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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSoldierRecruited, EFactionType, Faction, UStrategySoldier*, Soldier);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSoldierListChanged, EFactionType, Faction, const TArray<UStrategySoldier*>&, Soldiers);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSoldierLoadoutChanged, EFactionType, Faction, UStrategySoldier*, Soldier);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSoldierDismissed, EFactionType, Faction, UStrategySoldier*, Soldier);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnResearchCompleted, EFactionType, Faction, UResearchTechDefinition*, Tech);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnVehicleCompleted, EFactionType, Faction, UStrategyVehicle*, Vehicle);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemProduced, EFactionType, Faction, UItemDefinition*, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFacilityCompleted, EFactionType, Faction, UStrategyFacility*, Facility);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnProductionCompleted, EFactionType, Faction, UItemDefinition*, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMonthlyEvent, int32, Month);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSalvageSiteCreated, EFactionType, WreckOwnerFaction, const TArray<EFactionType>&, KnownFactions, UStrategySiteDefinition*, Site);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSiteDiscovered, EFactionType, Faction, UStrategySiteDefinition*, Site, EDiscoveryReason, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRadarContactUpdated, EFactionType, Faction, FRadarContact, Contact);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRadarContactExpired, EFactionType, Faction, FRadarContact, Contact);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnOpposingFactionRadarAlert, FRadarContact, Contact, FText, AlertMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSalvageSiteRemoved, FGuid, SiteId, EFactionType, LastSalvagingFaction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSalvageContestStarted, UStrategySiteDefinition*, Site,
    FSalvageContestForceSnapshot, HumanForceSnapshot, FSalvageContestForceSnapshot, EnemyForceSnapshot);

UCLASS()
class STRATEGICSIMULATIONPLUGIN_API UStrategyEventDispatcher : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;   // ← declaration only (body in .cpp)

    UPROPERTY(BlueprintAssignable, Category = "Events") FOnSoldierRecruited OnSoldierRecruited;
    UPROPERTY(BlueprintAssignable, Category = "Events") FOnSoldierListChanged OnSoldierListChanged;
    UPROPERTY(BlueprintAssignable, Category = "Events") FOnSoldierLoadoutChanged OnSoldierLoadoutChanged;
    UPROPERTY(BlueprintAssignable, Category = "Events") FOnSoldierDismissed OnSoldierDismissed;
    UPROPERTY(BlueprintAssignable, Category = "Events") FOnResearchCompleted OnResearchCompleted;
    UPROPERTY(BlueprintAssignable, Category = "Events") FOnVehicleCompleted OnVehicleCompleted;
    UPROPERTY(BlueprintAssignable, Category = "Events") FOnItemProduced OnItemProduced;
    UPROPERTY(BlueprintAssignable, Category = "Events") FOnFacilityCompleted OnFacilityCompleted;
    UPROPERTY(BlueprintAssignable, Category = "Events") FOnProductionCompleted OnProductionCompleted;
    UPROPERTY(BlueprintAssignable, Category = "Events") FOnMonthlyEvent OnMonthlyEvent;
    UPROPERTY(BlueprintAssignable, Category = "Events|Sites") FOnSalvageSiteCreated OnSalvageSiteCreated;
    UPROPERTY(BlueprintAssignable, Category = "Events|Sites") FOnSiteDiscovered OnSiteDiscovered;
    UPROPERTY(BlueprintAssignable, Category = "Events|Radar") FOnRadarContactUpdated OnRadarContactUpdated;
    UPROPERTY(BlueprintAssignable, Category = "Events|Radar") FOnRadarContactExpired OnRadarContactExpired;
    UPROPERTY(BlueprintAssignable, Category = "Events|Radar") FOnOpposingFactionRadarAlert OnOpposingFactionRadarAlert;
    UPROPERTY(BlueprintAssignable, Category = "Events|Sites") FOnSalvageSiteRemoved OnSalvageSiteRemoved;
    UPROPERTY(BlueprintAssignable, Category = "Events|Sites") FOnSalvageContestStarted OnSalvageContestStarted;
};