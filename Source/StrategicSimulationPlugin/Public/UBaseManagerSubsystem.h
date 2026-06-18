#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "StrategicSimulationTypes.h"
#include "UStrategyFacility.h"
#include "UFacilityDefinition.h"
#include "StrategicSiteDefinition.h"
#include "UStrategySaveGame.h"
#include "UStrategyBase.h"
#include "UTimeManagerSubsystem.h"
#include "UBaseManagerSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFacilityListChanged, EFactionType, Faction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBaseListChanged, EFactionType, Faction);

/**
 * Game-instance subsystem that owns faction bases, facilities, strategic sites,
 * salvage wrecks, and daily construction/repair/extraction simulation.
 */
UCLASS()
class STRATEGICSIMULATIONPLUGIN_API UBaseManagerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /** Binds to the time manager and initializes base/site systems. */
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /** Creates a new base at the given map location, optionally linked to a site. */
    UFUNCTION(BlueprintCallable, Category = "Base")
    UStrategyBase* BuildNewBase(EFactionType Faction, FText BaseName, FVector2D MapLocation, UStrategySiteDefinition* Site = nullptr);

    /** Returns all bases owned by the given faction. */
    UFUNCTION(BlueprintCallable, Category = "Base")
    const TArray<UStrategyBase*>& GetBases(EFactionType Faction) const;

    /** True when the faction may found another base (hangar and cap limits). */
    UFUNCTION(BlueprintCallable, Category = "Base")
    bool CanBuildNewBase(EFactionType Faction) const;

    /** Counts bases that have an operational hangar facility. */
    UFUNCTION(BlueprintCallable, Category = "Base")
    int32 GetNumberOfOperationalHangers(EFactionType Faction) const;

    /** Starts construction of a facility at the target base (or auto-selected base). */
    UFUNCTION(BlueprintCallable, Category = "Base")
    UStrategyFacility* BuildFacility(EFactionType Faction, UFacilityDefinition* FacilityDef, UStrategyBase* TargetBase = nullptr);

    /** Sum of power provided by all operational facilities across faction bases. */
    UFUNCTION(BlueprintCallable, Category = "Base")
    int32 GetTotalPowerProvided(EFactionType Faction) const;

    /** Sum of power drawn by all operational facilities across faction bases. */
    UFUNCTION(BlueprintCallable, Category = "Base")
    int32 GetTotalPowerDrawn(EFactionType Faction) const;

    /** Net power (provided minus drawn) for the faction. */
    UFUNCTION(BlueprintCallable, Category = "Base")
    int32 GetNetPower(EFactionType Faction) const;

    /** Total living-quarters capacity (soldier berths) across all faction bases. */
    UFUNCTION(BlueprintCallable, Category = "Base")
    int32 GetTotalBarracksCapacity(EFactionType Faction) const;

    /** True if any faction base has a facility of the given type. */
    UFUNCTION(BlueprintCallable, Category = "Base")
    bool HasFacilityOfType(EFactionType Faction, EFacilityType FacilityType) const;

    /** Total count of facilities of the given type across faction bases. */
    UFUNCTION(BlueprintCallable, Category = "Base")
    int32 GetCurrentCountOfType(EFactionType Faction, EFacilityType FacilityType) const;

    /** Advances build progress for all in-progress facilities of a faction. */
    UFUNCTION(BlueprintCallable, Category = "Base")
    void AdvanceFacilityConstruction(EFactionType Faction);

    /** Flat list of all facilities across all bases for a faction. */
    UFUNCTION(BlueprintCallable, Category = "Base")
    const TArray<UStrategyFacility*>& GetFacilities(EFactionType Faction) const;

    /** Total hangar vehicle capacity across all operational hangars. */
    UFUNCTION(BlueprintCallable, Category = "Base")
    int32 GetTotalAvailableHangerSlots(EFactionType Faction) const;

    /** All discovered strategic sites for each faction */
    UPROPERTY(BlueprintReadOnly, Category = "Expansion")
    TArray<UStrategySiteDefinition*> DiscoveredSitesHuman;

    UPROPERTY(BlueprintReadOnly, Category = "Expansion")
    TArray<UStrategySiteDefinition*> DiscoveredSitesEnemy;

    /** All potential base sites on the map (generated at game start) - neutral until discovered */
    UPROPERTY(BlueprintReadOnly, Category = "Expansion")
    TArray<UStrategySiteDefinition*> AllPotentialSites;

    /** Generate initial potential base sites at game start (callable with different parameters later) */
    UFUNCTION(BlueprintCallable, Category = "Expansion")
    void GenerateInitialSites(int32 NumSites = 25, float MinDistanceBetweenSites = 180.0f,
        float LogicalMapWidth = 1920.0f, float LogicalMapHeight = 1080.0f, float BorderPadding = 100.0f);

    /** Registers the exact site pointer for a faction (canonical discovery path). */
    UFUNCTION(BlueprintCallable, Category = "Expansion")
    UStrategySiteDefinition* AddDiscoveredSite(EFactionType Faction, UStrategySiteDefinition* Site,
        EDiscoveryReason Reason = EDiscoveryReason::Radar);

    /** Legacy location-based discovery. Prefer AddDiscoveredSite(Faction, Site) — nearest-match can register the wrong site. */
    UFUNCTION(BlueprintCallable, Category = "Expansion", meta = (DeprecatedFunction, DeprecationMessage = "Use AddDiscoveredSite(Faction, Site)."))
    UStrategySiteDefinition* AddDiscoveredSiteAtLocation(EFactionType Faction, FVector2D Location, EStrategySiteType Type = EStrategySiteType::PotentialBase, float OptionalScore = 0.0f);

    /**
     * Creates an active SalvageSite wreck at Location from a destroyed vehicle.
     * Seeds resources from build cost, sets expiry, registers combat-known factions, and broadcasts creation.
     */
    UFUNCTION(BlueprintCallable, Category = "Expansion|Salvage")
    UStrategySiteDefinition* CreateSalvageSite(FVector2D Location, class UStrategyVehicle* DestroyedVehicle);

    /** Adds combat-known factions to discovery lists (no radar required). */
    UFUNCTION(BlueprintCallable, Category = "Expansion|Salvage")
    void RegisterCombatKnownSalvage(UStrategySiteDefinition* Site);

    /** True when the site is an active salvage wreck site. */
    UFUNCTION(BlueprintPure, Category = "Expansion|Salvage")
    bool IsSalvageSite(const UStrategySiteDefinition* Site) const;

    /** True when the faction may dispatch salvage to this wreck (range, intel, caps). */
    UFUNCTION(BlueprintPure, Category = "Expansion|Salvage")
    bool CanSalvageSite(EFactionType Faction, const UStrategySiteDefinition* Site,
        const class UStrategyVehicle* SalvageVehicle = nullptr) const;

    /** True when the site appears in the faction's discovery or combat-known lists. */
    UFUNCTION(BlueprintPure, Category = "Expansion|Salvage")
    bool IsSiteKnownToFaction(EFactionType Faction, const UStrategySiteDefinition* Site) const;

    /** Days until an active salvage wreck expires (0 if not applicable). */
    UFUNCTION(BlueprintPure, Category = "Expansion|Salvage")
    int32 GetSalvageDaysRemaining(const UStrategySiteDefinition* Site) const;

    /** Removes a salvage site from the map and resolves any MIA soldiers at the wreck. */
    UFUNCTION(BlueprintCallable, Category = "Expansion|Salvage")
    void RemoveSalvageSite(UStrategySiteDefinition* Site, bool bExpired = false,
        EFactionType LastSalvagingFaction = EFactionType::Neutral);

    /** Finds the nearest site within tolerance of a map location. */
    UFUNCTION(BlueprintPure, Category = "Expansion")
    UStrategySiteDefinition* FindSiteAtLocation(FVector2D Location, float Tolerance = 128.f) const;

    /** Expires salvage wrecks whose expiry day has been reached. */
    void ProcessSalvageSiteExpiry(int32 CurrentSimulationDay);

    /** Serializes all potential and salvage sites for save/load. */
    UFUNCTION(BlueprintCallable, Category = "Expansion|Save")
    TArray<FStrategySiteSaveData> SerializeAllSites() const;

    /** Restores site lists and discovery state from saved data. */
    UFUNCTION(BlueprintCallable, Category = "Expansion|Save")
    void DeserializeAllSites(const TArray<FStrategySiteSaveData>& SavedSites);

    /** Debug/instant path: builds a base immediately without a guard vehicle. Prefer StartBaseExpansion for gameplay. */
    UFUNCTION(BlueprintCallable, Category = "Base Expansion", meta = (DeprecatedFunction, DeprecationMessage = "Use StartBaseExpansion for gameplay expansion."))
    bool TryBuildBaseOnSite(EFactionType Faction, UStrategySiteDefinition* TargetSite, FText BaseName);

    /** Checks if a faction can build a base on this specific site. */
    UFUNCTION(BlueprintCallable, Category = "Base Expansion")
    bool CanBuildBaseOnSite(EFactionType Faction, UStrategySiteDefinition* Site) const;

    /** Orders a vehicle mission to race to a site, claim it, guard CC construction, then return home. */
    UFUNCTION(BlueprintCallable, Category = "Base Expansion")
    bool StartBaseExpansion(EFactionType Faction, UStrategySiteDefinition* TargetSite, UStrategyBase* OriginBase,
        class UStrategyVehicle* Vehicle, FText BaseName);

    /** Atomic site claim: starts base shell and Command Center construction if gates still pass. */
    UFUNCTION(BlueprintCallable, Category = "Base Expansion")
    UStrategyBase* TryClaimExpansionSite(EFactionType Faction, UStrategySiteDefinition* TargetSite,
        class UStrategyVehicle* GuardVehicle, FText BaseName);

    /** Removes an in-progress expansion base and reopens the site for a new race. */
    UFUNCTION(BlueprintCallable, Category = "Base Expansion")
    void CancelExpansionConstruction(UStrategyBase* ExpansionBase, UStrategySiteDefinition* Site);

    /** True when the base has an operational Command Center facility. */
    UFUNCTION(BlueprintPure, Category = "Base Expansion")
    bool IsCommandCenterOperational(const UStrategyBase* Base) const;

    /** Days remaining on an in-progress Command Center (0 if none or already operational). */
    UFUNCTION(BlueprintPure, Category = "Base Expansion")
    int32 GetCommandCenterBuildDaysRemaining(const UStrategyBase* Base) const;

    /** Finds a base under construction at a site (CC not yet operational). */
    UFUNCTION(BlueprintPure, Category = "Base Expansion")
    UStrategyBase* FindExpansionBaseAtSite(const UStrategySiteDefinition* Site) const;

    /** Processes daily resource extraction from sites for all bases of a faction */
    void ProcessDailyResourceExtraction(EFactionType Faction);

    /** Destroys and clears all bases for both factions. */
    UFUNCTION(BlueprintCallable, Category = "Base")
    void ResetAllBases();

    /** Runs daily facility simulation (production, repair) for all bases of a faction. */
    UFUNCTION(BlueprintCallable, Category = "Repair")
    void SimulateDailyRepairs(EFactionType Faction);

    /** Advances production/construction queues for all facilities in both factions. */
    UFUNCTION(BlueprintCallable, Category = "Construction")
    void AdvanceAllConstruction();

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnFacilityListChanged OnFacilityListChanged;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnBaseListChanged OnBaseListChanged;

    /** Logs a detailed snapshot of base, facility, and personnel state to the output log. */
    UFUNCTION(BlueprintCallable, Category = "Debug|UI")
    void DebugPrintFullBaseState(EFactionType Faction) const;

    /** Builds the same base-state report as a FString for UI or tooling. */
    FString GetBaseStateDebugString(EFactionType Faction) const;

    /** Places the initial Command Centers for both factions on random sites with distance separation */
    UFUNCTION(BlueprintCallable, Category = "Base|Initialization")
    void InitializeStartingBases(int32 MinDistanceBetweenFactions = 700);

private:
    UPROPERTY(VisibleAnywhere, Transient, Category = "Bases")
    TArray<UStrategyBase*> HumanBases;

    UPROPERTY(VisibleAnywhere, Transient, Category = "Bases")
    TArray<UStrategyBase*> EnemyBases;

    /** Daily tick: construction, repairs, extraction, and salvage expiry. */
    UFUNCTION()
    void OnDayPassed(int32 NewDay);

    /** Returns the mutable base array for a faction. */
    TArray<UStrategyBase*>& GetMutableBases(EFactionType Faction);
    /** Returns the const base array for a faction. */
    const TArray<UStrategyBase*>& GetBasesInternal(EFactionType Faction) const;
};