#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "StrategicSimulationTypes.h"
#include "UResourceManagerSubsystem.h"
#include "UTimeManagerSubsystem.h"
#include "USoldierManagerSubsystem.h"
#include "UEngineeringManagerSubsystem.h"
#include "UBaseManagerSubsystem.h"
#include "UResearchManagerSubsystem.h"
#include "UAIControllerSubsystem.h"
#include "UMissionManagerSubsystem.h"
#include "UItemDatabase.h"
#include "UFacilityDatabase.h"
#include "UVehicleDatabase.h"
#include "UFacilityDefinition.h"
#include "UVehicleDefinition.h"
#include "UResearchDatabase.h"
#include "USoldierClassDatabase.h"
#include "UStrategyCampaignSubsystem.generated.h"

/**
 * Central game-instance facade for the strategic campaign lifecycle.
 * Wires manager subsystems together, owns campaign tuning properties (map, AI, salvage, radar),
 * handles save/load, daily tick orchestration, and contested-salvage clock pausing.
 */
UCLASS()
class STRATEGICSIMULATIONPLUGIN_API UStrategyCampaignSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /** Declares subsystem dependencies and binds OnDayPassed to campaign, mission, and AI handlers. */
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    // === Map generation (overridden by AStrategyGameInitializer in the level) ===
    /** Target count of potential base/resource sites scattered on the logical strategy map. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | Map Generation")
    int32 NumberOfStrategicSites = 25;

    /** Minimum pixel distance between any two generated sites; higher values spread the map out. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | Map Generation")
    float MinimumDistanceBetweenSites = 350.0f;

    /** Width of the logical strategy map in pixels (world coordinates for sites and missions). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | Map Generation")
    float LogicalMapWidth = 1920.0f;

    /** Height of the logical strategy map in pixels. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | Map Generation")
    float LogicalMapHeight = 1080.0f;

    /** Inset from map edges where sites may spawn; defines the playable interior rectangle. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | Map Generation")
    float MapBorderPadding = 100.0f;

    /** Minimum separation between Human and Enemy starting Command Centers at new-game setup. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | Map Generation")
    float MinDistanceBetweenFactions = 700.0f;

    // === AI / Simulation Settings ===
    /** Cap on bases each faction (especially AI) may own during the campaign. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | AI")
    int32 MaxAIBases = 4;

    /** When true, AI schedules each vehicle departure evenly across the 24-hour game day */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | AI")
    bool bStaggerMissionLaunches = true;

    /** In-game day when fighters may begin scheduling Offensive (base attack) missions */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | AI", meta = (ClampMin = "1", ClampMax = "60"))
    int32 OffensiveMissionsStartDay = 5;

    /** Minimum offensive rating (base + weapons) required before AI engages in vehicular combat */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | AI", meta = (ClampMin = "0", ClampMax = "500"))
    int32 MinOffenseToEngage = 10;

    /** When false, destroyed vehicles do not spawn salvage wreck sites. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | Salvage")
    bool bSalvageSitesEnabled = true;

    /** When false, SaveCampaign skips site-map data (dev fast-save). LoadCampaign requires schema >= 2 with sites. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | Salvage")
    bool bSitesPersistenceEnabled = true;

    /** When false, AI and mission scheduling will not assign new Salvage missions. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | Salvage")
    bool bSalvageMissionsEnabled = true;

    /** Hours spent on-station at a wreck during Salvage missions (extraction window). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | Salvage", meta = (ClampMin = "0.5", ClampMax = "24.0"))
    float SalvageOnStationHours = 4.0f;

    /** Multiplier on hourly salvage extraction rate (PR-7: ~4.0 depletes a medium wreck in one on-station window). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | Salvage", meta = (ClampMin = "0.1", ClampMax = "16.0"))
    float SalvageEfficiencyMultiplier = 4.0f;

    /** Max concurrent in-flight Salvage missions per faction (PR-7). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | Salvage", meta = (ClampMin = "1", ClampMax = "8"))
    int32 MaxActiveSalvageMissionsPerFaction = 2;

    /** Max concurrent BaseExpansion guard missions per faction. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | Expansion", meta = (ClampMin = "1", ClampMax = "4"))
    int32 MaxActiveExpansionMissionsPerFaction = 1;

    /** When true, new bases require a vehicle mission to claim and guard the site. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | Expansion")
    bool bBaseExpansionRequiresVehicleGuard = true;

    /** Minimum heuristic score before AI schedules Salvage (PR-7). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | Salvage", meta = (ClampMin = "0.0", ClampMax = "500.0"))
    float MinSalvageScoreThreshold = 15.0f;

    /** AI: chance the combat winner declines salvage and leaves the wreck (retaliation risk). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | Salvage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float SalvageDeclineAfterWinChance = 0.35f;

    /** AI: score multiplier required when recovering own destroyed wreck (loser recovery). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | Salvage", meta = (ClampMin = "1.0", ClampMax = "5.0"))
    float LoserSalvageScoreMultiplier = 1.5f;

    /** AI: own-wreck recovery abandoned when farther than this (map px) and below loser score threshold. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | Salvage", meta = (ClampMin = "100.0", ClampMax = "2000.0"))
    float LoserSalvageMaxDistance = 700.0f;

    /** Days to remember post-combat salvage context for decline/recovery heuristics. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | Salvage", meta = (ClampMin = "1", ClampMax = "14"))
    int32 SalvageCombatMemoryDays = 3;

    /** When false, AStrategyDebugHUD Exec commands are no-ops (shipping safety). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | Radar & Intel")
    bool bAllowDebugExecCommands = false;

    /** Master toggle for radar line-of-sight checks against terrain blockers (PR-10). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | Radar & Intel")
    bool bRadarLOSEnabled = true;

    /** Master toggle for per-faction stale site intel snapshots (PR-9). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | Radar & Intel")
    bool bStaleIntelEnabled = true;

    /** Command Center passive radar without vehicle sortie (PR-11). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | Radar & Intel")
    bool bBasePassiveRadarEnabled = true;

    /** Passive radar range from operational Command Center (logical map pixels). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | Radar & Intel", meta = (ClampMin = "64.0", ClampMax = "2000.0"))
    float BaseRadarRangePixels = 512.0f;

    /** Hours between Command Center passive radar pings. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | Radar & Intel", meta = (ClampMin = "0.25", ClampMax = "24.0"))
    float BaseRadarPingIntervalHours = 1.0f;

    /** Hours before an unseen radar contact expires. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | Radar & Intel", meta = (ClampMin = "1.0", ClampMax = "72.0"))
    float RadarContactExpiryHours = 6.0f;

    /** AI launches Interception when base radar spots inbound threats (PR-11). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | Radar & Intel")
    bool bAIReactiveInterceptionEnabled = true;

    /** Toast when enemy passive radar picks up a friendly inbound vehicle (PR-15). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | Radar & Intel")
    bool bNotifyPlayerOfEnemyRadarContacts = true;

    /** Debug map draws enemy-faction radar entry points (PR-15). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | Radar & Intel")
    bool bShowEnemyRadarContactsOnDebugMap = true;

    /** In-transit combat vehicles engage enemies inbound to friendly bases (PR-14). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | Radar & Intel")
    bool bEngageInboundThreatsWhileInTransit = true;

    /** Days a wreck remains on the map before auto-removal if not salvaged. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | Salvage", meta = (ClampMin = "1", ClampMax = "90"))
    int32 SalvageWreckExpiryDays = 7;

    /** Per-soldier chance to die in the vehicle crash/destruction (remainder become MIA). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | Salvage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float VehicleCrashDeathChance = 0.25f;

    /** AI: chance each enemy MIA becomes POW when the opposing faction salvages the wreck. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | Salvage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float OpposingSalvageMIAPOWChance = 0.40f;

    /**
     * Clears all faction state across manager subsystems for a New Game.
     * Call from UI before StartSimulation when the player chooses restart.
     */
    UFUNCTION(BlueprintCallable, Category = "Simulation")
    void ResetSimulation();

    /**
     * Writes campaign progress to a numbered save slot.
     * Call from save UI; honors bSitesPersistenceEnabled for site/intel payload.
     * @param SlotIndex 1-based slot index (clamped to minimum 1).
     */
    UFUNCTION(BlueprintCallable, Category = "Campaign")
    void SaveCampaign(int32 SlotIndex = 1);

    /**
     * Restores campaign state from a numbered save slot.
     * Call from load UI after validating schema version; may leave 0 bases if save is map-only.
     * @param SlotIndex 1-based slot index (clamped to minimum 1).
     */
    UFUNCTION(BlueprintCallable, Category = "Campaign")
    void LoadCampaign(int32 SlotIndex = 1);
        
    // === POW/KIA SETTINGS (Phase 1 + 3) ===
    /** Chance victorious attackers take enemy soldiers as POW after ground combat. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POW/KIA|Settings")
    float POWCaptureChanceOnVictory = 0.35f;

    /** Chance victorious attackers inflict KIA on enemies instead of capture. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POW/KIA|Settings")
    float KIAChanceOnVictory = 0.25f;

    /** Chance defending losers are captured as POW when the attacker wins. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POW/KIA|Settings")
    float EnemyPOWCaptureChanceOnDefeat = 0.20f;

    /** Chance defending losers are killed in action when the attacker wins. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POW/KIA|Settings")
    float EnemyKIAChanceOnDefeat = 0.40f;

    /**
     * Debug override for victor-side POW and KIA probabilities.
     * Call from test harness or console during balance tuning.
     * @param NewPOWCaptureChance Clamped 0–1 capture chance on victory.
     * @param NewKIAChanceOnVictory Clamped 0–1 KIA chance on victory.
     */
    UFUNCTION(BlueprintCallable, Category = "POW/KIA|Debug")
    void SetVictoryChances(float NewPOWCaptureChance, float NewKIAChanceOnVictory);

    /**
     * Debug override for defender KIA probability on defeat.
     * Call from test harness when tuning casualty rates.
     * @param NewEnemyKIAChanceOnDefeat Clamped 0–1 defender KIA chance.
     */
    UFUNCTION(BlueprintCallable, Category = "POW/KIA|Debug")
    void SetDefeatKIAChance(float NewEnemyKIAChanceOnDefeat);

    /**
     * Returns the resource stockpile manager for both factions.
     * Call from UI or systems that read/write Money, Metals, etc.
     * @return UResourceManagerSubsystem or nullptr if unavailable.
     */
    UFUNCTION(BlueprintCallable, Category = "Managers")
    class UResourceManagerSubsystem* GetResourceManager() const;

    /**
     * Returns the soldier roster manager.
     * Call when querying training, roster, or casualty state.
     * @return USoldierManagerSubsystem or nullptr if unavailable.
     */
    UFUNCTION(BlueprintCallable, Category = "Managers")
    class USoldierManagerSubsystem* GetSoldierManager() const;

    /**
     * Returns the research progress manager.
     * Call when starting or querying tech projects.
     * @return UResearchManagerSubsystem or nullptr if unavailable.
     */
    UFUNCTION(BlueprintCallable, Category = "Managers")
    class UResearchManagerSubsystem* GetResearchManager() const;

    /**
     * Returns the engineering/production queue manager.
     * Call when building items or vehicles.
     * @return UEngineeringManagerSubsystem or nullptr if unavailable.
     */
    UFUNCTION(BlueprintCallable, Category = "Managers")
    class UEngineeringManagerSubsystem* GetEngineeringManager() const;

    /**
     * Returns the base and site map manager.
     * Call for facilities, sites, repairs, and map generation.
     * @return UBaseManagerSubsystem or nullptr if unavailable.
     */
    UFUNCTION(BlueprintCallable, Category = "Managers")
    class UBaseManagerSubsystem* GetBaseManager() const;

    /**
     * Returns the strategic time/clock manager.
     * Call for date, pause, scale, and day-passed events.
     * @return UTimeManagerSubsystem or nullptr if unavailable.
     */
    UFUNCTION(BlueprintCallable, Category = "Managers")
    class UTimeManagerSubsystem* GetTimeManager() const;

    /**
     * Returns the faction AI controller.
     * Call for AI orders, expansion limits, and debug AI runs.
     * @return UAIControllerSubsystem or nullptr if unavailable.
     */
    UFUNCTION(BlueprintCallable, Category = "Managers")
    class UAIControllerSubsystem* GetAIController() const;

    /**
     * Returns the mission and vehicle transit manager.
     * Call when scheduling, updating, or resolving missions.
     * @return UMissionManagerSubsystem or nullptr if unavailable.
     */
    UFUNCTION(BlueprintCallable, Category = "Managers")
    class UMissionManagerSubsystem* GetMissionManager() const;

    /**
     * Returns the per-faction stale intel snapshot manager.
     * Call when reading or updating discovered site knowledge.
     * @return UFactionIntelSubsystem or nullptr if unavailable.
     */
    UFUNCTION(BlueprintCallable, Category = "Managers")
    class UFactionIntelSubsystem* GetFactionIntelManager() const;

    /**
     * Returns the radar terrain/blocker manager.
     * Call when configuring or querying LOS blockers.
     * @return URadarTerrainSubsystem or nullptr if unavailable.
     */
    UFUNCTION(BlueprintCallable, Category = "Managers")
    class URadarTerrainSubsystem* GetRadarTerrainManager() const;

    /**
     * Returns the active radar contact tracker.
     * Call for inbound threat detection and interception hooks.
     * @return URadarContactSubsystem or nullptr if unavailable.
     */
    UFUNCTION(BlueprintCallable, Category = "Managers")
    class URadarContactSubsystem* GetRadarContactManager() const;

    /**
     * Returns the loaded infantry/equipment item database asset.
     * Call from UI or production to resolve buyable item definitions.
     * @return UItemDatabase pointer from ItemDatabaseAsset soft reference.
     */
    UFUNCTION(BlueprintCallable, Category = "Databases")
    class UItemDatabase* GetItemDatabase() const { return ItemDatabaseAsset.Get(); }

    /**
     * Returns the facility definition database asset.
     * Call when building or listing constructible facilities.
     * @return UFacilityDatabase pointer from FacilityDatabaseAsset.
     */
    UFUNCTION(BlueprintCallable, Category = "Databases")
    class UFacilityDatabase* GetFacilityDatabase() const { return FacilityDatabaseAsset.Get(); }

    /**
     * Returns the soldier class definition database asset.
     * Call when training or displaying class stats.
     * @return USoldierClassDatabase pointer from SoldierClassDatabaseAsset.
     */
    UFUNCTION(BlueprintCallable, Category = "Databases")
    class USoldierClassDatabase* GetSoldierClassDatabase() const { return SoldierClassDatabaseAsset.Get(); }

    /**
     * Returns the research tech tree database asset.
     * Call when listing or unlocking research projects.
     * @return UResearchDatabase pointer from ResearchDatabaseAsset.
     */
    UFUNCTION(BlueprintCallable, Category = "Databases")
    class UResearchDatabase* GetResearchDatabase() const { return ResearchDatabaseAsset.Get(); }

    /**
     * Returns the vehicle hull database asset.
     * Call when producing or equipping vehicles.
     * @return UVehicleDatabase pointer from VehicleDatabaseAsset.
     */
    UFUNCTION(BlueprintCallable, Category = "Databases")
    class UVehicleDatabase* GetVehicleDatabase() const { return VehicleDatabaseAsset.Get(); }

    /**
     * Returns the vehicle weapons/defense/ammo item database asset.
     * Call when outfitting vehicles with modular equipment.
     * @return UItemDatabase pointer from VehicleItemDatabaseAsset.
     */
    UFUNCTION(BlueprintCallable, Category = "Databases")
    class UItemDatabase* GetVehicleItemDatabase() const { return VehicleItemDatabaseAsset.Get(); }

    // === DATABASES ===
    
    /** Soft reference to infantry/equipment items loaded at campaign init. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    TSoftObjectPtr<class UItemDatabase> ItemDatabaseAsset;
        
    /** Soft reference to constructible facility definitions. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Databases")
    TSoftObjectPtr<class UFacilityDatabase> FacilityDatabaseAsset;

    /** Soft reference to trainable soldier class definitions. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Databases")
    TSoftObjectPtr<class USoldierClassDatabase> SoldierClassDatabaseAsset;

    /** Soft reference to research project definitions. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Databases")
    TSoftObjectPtr<class UResearchDatabase> ResearchDatabaseAsset;

    /** Soft reference to vehicle hull definitions. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Databases")
    TSoftObjectPtr<class UVehicleDatabase> VehicleDatabaseAsset;

    /** Dedicated database for all vehicle weapons, defense systems, and ammo items */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Databases")
    TSoftObjectPtr<class UItemDatabase> VehicleItemDatabaseAsset;

    /**
     * Begins a new playable campaign: generates map, places bases, runs day-1 tick, logs data assets.
     * Call from UI Start button after initializer has applied settings and databases.
     */
    UFUNCTION(BlueprintCallable, Category = "Campaign")
    void StartSimulation();

    /**
     * Halts time advancement by setting time scale to zero.
     * Call from UI pause/stop; does not clear map or contest state.
     */
    UFUNCTION(BlueprintCallable, Category = "Campaign")
    void StopSimulation();

    /**
     * Freezes the strategic clock while a contested salvage scenario plays out.
     * Call when both factions arrive at the same wreck; delegates to SetStrategicClockPaused(true).
     */
    UFUNCTION(BlueprintCallable, Category = "Campaign|Salvage Contest")
    void PauseStrategicClock();

    /**
     * Resumes the strategic clock after salvage contest resolution or abort.
     * Call from ResolveSalvageContest or UI when contest UI closes.
     */
    UFUNCTION(BlueprintCallable, Category = "Campaign|Salvage Contest")
    void ResumeStrategicClock();

    /**
     * Returns whether a salvage contest is awaiting player/AI resolution.
     * Call to gate input or show contest UI overlays.
     * @return True while contest snapshots and missions are stored on this subsystem.
     */
    UFUNCTION(BlueprintPure, Category = "Campaign|Salvage Contest")
    bool IsSalvageContestActive() const { return bSalvageContestActive; }

    /**
     * Applies the chosen contest outcome, aborts losing/withdrawing missions, and resumes the clock.
     * Call from tactical UI or AI resolver once the player picks an outcome.
     * @param Outcome Who wins, withdraws, or mutually retreats at the contested wreck.
     */
    UFUNCTION(BlueprintCallable, Category = "Campaign|Salvage Contest")
    void ResolveSalvageContest(ESalvageContestOutcome Outcome);

    /**
     * Returns a short HUD-friendly day string from the time manager.
     * Call from lightweight UI that only needs "Day N".
     * @return Formatted day label.
     */
    UFUNCTION(BlueprintCallable, Category = "Campaign")
    FString GetFormattedDate() const;

    /**
     * Scans save slots 1–10 and returns metadata objects for occupied slots.
     * Call from save/load selection screen.
     * @return Array of loaded UStrategySaveGame instances (may be partial reads).
     */
    UFUNCTION(BlueprintCallable, Category = "Campaign")
    TArray<class UStrategySaveGame*> GetAllSaveMetadata() const;

    /**
     * Checks whether a faction has finished a research tech (placeholder implementation).
     * Call from unlock helpers before granting items.
     * @param Faction Human or Enemy faction to query.
     * @param Tech Research definition to test.
     * @return True if completed (currently always true when Tech is valid).
     */
    UFUNCTION(BlueprintCallable, Category = "Unlocks")
    bool HasCompletedResearch(EFactionType Faction, class UResearchTechDefinition* Tech) const;

    /**
     * Returns whether an item is unlocked for a faction via completed facility research chains.
     * Call before allowing purchase or production of an item.
     * @param Faction Faction whose facilities and research are scanned.
     * @param ItemDef Item definition to test.
     * @return True when a completed research path lists the item.
     */
    UFUNCTION(BlueprintCallable, Category = "Unlocks")
    bool IsItemUnlocked(EFactionType Faction, class UItemDefinition* ItemDef) const;

    /**
     * Forces an immediate AI planning pass for debugging.
     * Call from debug menu when verifying AI orders without waiting for OnDayPassed.
     */
    UFUNCTION(BlueprintCallable, Category = "Debug")
    void Debug_RunAI();

    /** Toggle for extra verbose facility tick logging (helps reduce console spam) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bVerboseFacilityLogging = false;

    /** Master toggle for extra debug logging (Facility Ticks, etc.). Set false in production. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bVerboseLogging = false;

    /** When true, logs [UNLOCK] once per newly available item (copied from initializer). */
    bool bShowUnlockMessages = true;

    /** When true, enables per-facility tick spam in logs (copied from initializer). */
    bool bShowFacilityTicks = false;

    /** Tracks already announced unlocks to prevent daily spam */
    TSet<FString> AnnouncedUnlocks;

    /** True while contested salvage UI/flow is active and the strategic clock is paused. */
    UPROPERTY(Transient)
    bool bSalvageContestActive = false;

    /** Wreck site both factions are contesting during an active salvage dispute. */
    UPROPERTY(Transient)
    TObjectPtr<class UStrategySiteDefinition> ContestedSalvageSite = nullptr;

    /** Human salvage mission frozen at the contest site. */
    UPROPERTY(Transient)
    TObjectPtr<class UMissionGroup> ContestedHumanSalvageMission = nullptr;

    /** Enemy salvage mission frozen at the contest site. */
    UPROPERTY(Transient)
    TObjectPtr<class UMissionGroup> ContestedEnemySalvageMission = nullptr;

    /** Force snapshot for human side at contest start (for UI/combat resolution). */
    UPROPERTY(Transient)
    FSalvageContestForceSnapshot ContestedHumanSnapshot;

    /** Force snapshot for enemy side at contest start (for UI/combat resolution). */
    UPROPERTY(Transient)
    FSalvageContestForceSnapshot ContestedEnemySnapshot;

    /** Clears all transient contest fields without resolving missions. */
    void ClearSalvageContestState();

    /**
     * Records an active salvage contest and associated missions/snapshots.
     * Call from mission manager when human and enemy salvage forces collide.
     */
    void ActivateSalvageContest(class UStrategySiteDefinition* Site, class UMissionGroup* HumanMission,
        class UMissionGroup* EnemyMission, const FSalvageContestForceSnapshot& HumanSnapshot,
        const FSalvageContestForceSnapshot& EnemySnapshot);

private:
    /** Campaign daily tick: repairs, logging; AI/mission day handlers run via separate bindings. */
    UFUNCTION()
    void OnDayPassed(int32 NewDay);
};