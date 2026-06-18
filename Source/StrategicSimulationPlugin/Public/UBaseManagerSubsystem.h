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

UCLASS()
class STRATEGICSIMULATIONPLUGIN_API UBaseManagerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    UFUNCTION(BlueprintCallable, Category = "Base")
    UStrategyBase* BuildNewBase(EFactionType Faction, FText BaseName, FVector2D MapLocation, UStrategySiteDefinition* Site = nullptr);

    UFUNCTION(BlueprintCallable, Category = "Base")
    const TArray<UStrategyBase*>& GetBases(EFactionType Faction) const;

    UFUNCTION(BlueprintCallable, Category = "Base")
    bool CanBuildNewBase(EFactionType Faction) const;

    UFUNCTION(BlueprintCallable, Category = "Base")
    int32 GetNumberOfOperationalHangers(EFactionType Faction) const;

    UFUNCTION(BlueprintCallable, Category = "Base")
    UStrategyFacility* BuildFacility(EFactionType Faction, UFacilityDefinition* FacilityDef, UStrategyBase* TargetBase = nullptr);

    UFUNCTION(BlueprintCallable, Category = "Base")
    int32 GetTotalPowerProvided(EFactionType Faction) const;

    UFUNCTION(BlueprintCallable, Category = "Base")
    int32 GetTotalPowerDrawn(EFactionType Faction) const;

    UFUNCTION(BlueprintCallable, Category = "Base")
    int32 GetNetPower(EFactionType Faction) const;

    UFUNCTION(BlueprintCallable, Category = "Base")
    int32 GetTotalBarracksCapacity(EFactionType Faction) const;

    UFUNCTION(BlueprintCallable, Category = "Base")
    bool HasFacilityOfType(EFactionType Faction, EFacilityType FacilityType) const;

    UFUNCTION(BlueprintCallable, Category = "Base")
    int32 GetCurrentCountOfType(EFactionType Faction, EFacilityType FacilityType) const;

    UFUNCTION(BlueprintCallable, Category = "Base")
    void AdvanceFacilityConstruction(EFactionType Faction);

    UFUNCTION(BlueprintCallable, Category = "Base")
    const TArray<UStrategyFacility*>& GetFacilities(EFactionType Faction) const;

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

    /** Spawns a salvage site at a destroyed vehicle wreck. */
    UFUNCTION(BlueprintCallable, Category = "Expansion|Salvage")
    UStrategySiteDefinition* CreateSalvageSite(FVector2D Location, class UStrategyVehicle* DestroyedVehicle);

    /** Adds combat-known factions to discovery lists (no radar required). */
    UFUNCTION(BlueprintCallable, Category = "Expansion|Salvage")
    void RegisterCombatKnownSalvage(UStrategySiteDefinition* Site);

    UFUNCTION(BlueprintPure, Category = "Expansion|Salvage")
    bool IsSalvageSite(const UStrategySiteDefinition* Site) const;

    UFUNCTION(BlueprintPure, Category = "Expansion|Salvage")
    bool CanSalvageSite(EFactionType Faction, const UStrategySiteDefinition* Site,
        const class UStrategyVehicle* SalvageVehicle = nullptr) const;

    UFUNCTION(BlueprintPure, Category = "Expansion|Salvage")
    bool IsSiteKnownToFaction(EFactionType Faction, const UStrategySiteDefinition* Site) const;

    UFUNCTION(BlueprintPure, Category = "Expansion|Salvage")
    int32 GetSalvageDaysRemaining(const UStrategySiteDefinition* Site) const;

    UFUNCTION(BlueprintCallable, Category = "Expansion|Salvage")
    void RemoveSalvageSite(UStrategySiteDefinition* Site, bool bExpired = false);

    void ProcessSalvageSiteExpiry(int32 CurrentSimulationDay);

    UFUNCTION(BlueprintCallable, Category = "Expansion|Save")
    TArray<FStrategySiteSaveData> SerializeAllSites() const;

    UFUNCTION(BlueprintCallable, Category = "Expansion|Save")
    void DeserializeAllSites(const TArray<FStrategySiteSaveData>& SavedSites);

    /** Attempts to build a new base on a discovered site. Returns true if successful. */
    UFUNCTION(BlueprintCallable, Category = "Base Expansion")
    bool TryBuildBaseOnSite(EFactionType Faction, UStrategySiteDefinition* TargetSite, FText BaseName);

    /** Checks if a faction can build a base on this specific site. */
    UFUNCTION(BlueprintCallable, Category = "Base Expansion")
    bool CanBuildBaseOnSite(EFactionType Faction, UStrategySiteDefinition* Site) const;

    /** Processes daily resource extraction from sites for all bases of a faction */
    void ProcessDailyResourceExtraction(EFactionType Faction);

    UFUNCTION(BlueprintCallable, Category = "Base")
    void ResetAllBases();

    UFUNCTION(BlueprintCallable, Category = "Repair")
    void SimulateDailyRepairs(EFactionType Faction);

    // === NEW: Queue integration ===
    UFUNCTION(BlueprintCallable, Category = "Construction")
    void AdvanceAllConstruction();

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnFacilityListChanged OnFacilityListChanged;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnBaseListChanged OnBaseListChanged;

    UFUNCTION(BlueprintCallable, Category = "Debug|UI")
    void DebugPrintFullBaseState(EFactionType Faction) const;
        
    FString GetBaseStateDebugString(EFactionType Faction) const;

    /** Places the initial Command Centers for both factions on random sites with distance separation */
    UFUNCTION(BlueprintCallable, Category = "Base|Initialization")
    void InitializeStartingBases(int32 MinDistanceBetweenFactions = 700);

private:
    UPROPERTY(VisibleAnywhere, Transient, Category = "Bases")
    TArray<UStrategyBase*> HumanBases;

    UPROPERTY(VisibleAnywhere, Transient, Category = "Bases")
    TArray<UStrategyBase*> EnemyBases;

    UFUNCTION()
    void OnDayPassed(int32 NewDay);

    TArray<UStrategyBase*>& GetMutableBases(EFactionType Faction);
    const TArray<UStrategyBase*>& GetBasesInternal(EFactionType Faction) const;
};