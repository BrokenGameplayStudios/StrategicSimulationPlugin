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

UCLASS()
class STRATEGICSIMULATIONPLUGIN_API UStrategyCampaignSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    // === Map generation (overridden by AStrategyGameInitializer in the level) ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | Map Generation")
    int32 NumberOfStrategicSites = 25;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | Map Generation")
    float MinimumDistanceBetweenSites = 350.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | Map Generation")
    float LogicalMapWidth = 1920.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | Map Generation")
    float LogicalMapHeight = 1080.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | Map Generation")
    float MapBorderPadding = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | Map Generation")
    float MinDistanceBetweenFactions = 700.0f;

    // === AI / Simulation Settings ===
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

    /** Days a wreck remains on the map before auto-removal if not salvaged. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | Salvage", meta = (ClampMin = "1", ClampMax = "90"))
    int32 SalvageWreckExpiryDays = 7;

    /** Per-soldier chance to die in the vehicle crash/destruction (remainder become MIA). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | Salvage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float VehicleCrashDeathChance = 0.25f;

    /** AI: chance each enemy MIA becomes POW when the opposing faction salvages the wreck. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | Salvage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float OpposingSalvageMIAPOWChance = 0.40f;

    /** Full reset of the simulation (call from UI for New Game) */
    UFUNCTION(BlueprintCallable, Category = "Simulation")
    void ResetSimulation();

    // Save / Load
    UFUNCTION(BlueprintCallable, Category = "Campaign")
    void SaveCampaign(int32 SlotIndex = 1);

    UFUNCTION(BlueprintCallable, Category = "Campaign")
    void LoadCampaign(int32 SlotIndex = 1);
        
    // === POW/KIA SETTINGS (Phase 1 + 3) ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POW/KIA|Settings")
    float POWCaptureChanceOnVictory = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POW/KIA|Settings")
    float KIAChanceOnVictory = 0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POW/KIA|Settings")
    float EnemyPOWCaptureChanceOnDefeat = 0.20f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POW/KIA|Settings")
    float EnemyKIAChanceOnDefeat = 0.40f;

    // === DEBUG COMMANDS (clean & clear) ===
    UFUNCTION(BlueprintCallable, Category = "POW/KIA|Debug")
    void SetVictoryChances(float NewPOWCaptureChance, float NewKIAChanceOnVictory);

    UFUNCTION(BlueprintCallable, Category = "POW/KIA|Debug")
    void SetDefeatKIAChance(float NewEnemyKIAChanceOnDefeat);

    // Manager getters
    UFUNCTION(BlueprintCallable, Category = "Managers")
    class UResourceManagerSubsystem* GetResourceManager() const;

    UFUNCTION(BlueprintCallable, Category = "Managers")
    class USoldierManagerSubsystem* GetSoldierManager() const;

    UFUNCTION(BlueprintCallable, Category = "Managers")
    class UResearchManagerSubsystem* GetResearchManager() const;

    UFUNCTION(BlueprintCallable, Category = "Managers")
    class UEngineeringManagerSubsystem* GetEngineeringManager() const;

    UFUNCTION(BlueprintCallable, Category = "Managers")
    class UBaseManagerSubsystem* GetBaseManager() const;

    UFUNCTION(BlueprintCallable, Category = "Managers")
    class UTimeManagerSubsystem* GetTimeManager() const;

    UFUNCTION(BlueprintCallable, Category = "Managers")
    class UAIControllerSubsystem* GetAIController() const;

    UFUNCTION(BlueprintCallable, Category = "Managers")
    class UMissionManagerSubsystem* GetMissionManager() const;

    UFUNCTION(BlueprintCallable, Category = "Managers")
    class UFactionIntelSubsystem* GetFactionIntelManager() const;

    UFUNCTION(BlueprintCallable, Category = "Managers")
    class URadarTerrainSubsystem* GetRadarTerrainManager() const;

    UFUNCTION(BlueprintCallable, Category = "Managers")
    class URadarContactSubsystem* GetRadarContactManager() const;

    UFUNCTION(BlueprintCallable, Category = "Databases")
    class UItemDatabase* GetItemDatabase() const { return ItemDatabaseAsset.Get(); }

    UFUNCTION(BlueprintCallable, Category = "Databases")
    class UFacilityDatabase* GetFacilityDatabase() const { return FacilityDatabaseAsset.Get(); }

    UFUNCTION(BlueprintCallable, Category = "Databases")
    class USoldierClassDatabase* GetSoldierClassDatabase() const { return SoldierClassDatabaseAsset.Get(); }

    UFUNCTION(BlueprintCallable, Category = "Databases")
    class UResearchDatabase* GetResearchDatabase() const { return ResearchDatabaseAsset.Get(); }

    UFUNCTION(BlueprintCallable, Category = "Databases")
    class UVehicleDatabase* GetVehicleDatabase() const { return VehicleDatabaseAsset.Get(); }

    UFUNCTION(BlueprintCallable, Category = "Databases")
    class UItemDatabase* GetVehicleItemDatabase() const { return VehicleItemDatabaseAsset.Get(); }

    // === DATABASES ===
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    TSoftObjectPtr<class UItemDatabase> ItemDatabaseAsset;
        
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Databases")
    TSoftObjectPtr<class UFacilityDatabase> FacilityDatabaseAsset;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Databases")
    TSoftObjectPtr<class USoldierClassDatabase> SoldierClassDatabaseAsset;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Databases")
    TSoftObjectPtr<class UResearchDatabase> ResearchDatabaseAsset;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Databases")
    TSoftObjectPtr<class UVehicleDatabase> VehicleDatabaseAsset;

    /** NEW: Dedicated database for all vehicle weapons, defense systems, and ammo items */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Databases")
    TSoftObjectPtr<class UItemDatabase> VehicleItemDatabaseAsset;

    // Simulation control
    UFUNCTION(BlueprintCallable, Category = "Campaign")
    void StartSimulation();

    UFUNCTION(BlueprintCallable, Category = "Campaign")
    void StopSimulation();

    UFUNCTION(BlueprintCallable, Category = "Campaign|Salvage Contest")
    void PauseStrategicClock();

    UFUNCTION(BlueprintCallable, Category = "Campaign|Salvage Contest")
    void ResumeStrategicClock();

    UFUNCTION(BlueprintPure, Category = "Campaign|Salvage Contest")
    bool IsSalvageContestActive() const { return bSalvageContestActive; }

    UFUNCTION(BlueprintCallable, Category = "Campaign|Salvage Contest")
    void ResolveSalvageContest(ESalvageContestOutcome Outcome);

    UFUNCTION(BlueprintCallable, Category = "Campaign")
    FString GetFormattedDate() const;

    UFUNCTION(BlueprintCallable, Category = "Campaign")
    TArray<class UStrategySaveGame*> GetAllSaveMetadata() const;

    // Research / Unlock helpers
    UFUNCTION(BlueprintCallable, Category = "Unlocks")
    bool HasCompletedResearch(EFactionType Faction, class UResearchTechDefinition* Tech) const;

    UFUNCTION(BlueprintCallable, Category = "Unlocks")
    bool IsItemUnlocked(EFactionType Faction, class UItemDefinition* ItemDef) const;

    UFUNCTION(BlueprintCallable, Category = "Debug")
    void Debug_RunAI();

    /** Toggle for extra verbose facility tick logging (helps reduce console spam) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bVerboseFacilityLogging = false;

    /** Master toggle for extra debug logging (Facility Ticks, etc.). Set false in production. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bVerboseLogging = false;

    /** Debug toggles (controlled from AStrategyGameInitializer in the level) */    
    bool bShowUnlockMessages = true;
    bool bShowFacilityTicks = false;

    /** Tracks already announced unlocks to prevent daily spam */
    TSet<FString> AnnouncedUnlocks;

    UPROPERTY(Transient)
    bool bSalvageContestActive = false;

    UPROPERTY(Transient)
    TObjectPtr<class UStrategySiteDefinition> ContestedSalvageSite = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<class UMissionGroup> ContestedHumanSalvageMission = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<class UMissionGroup> ContestedEnemySalvageMission = nullptr;

    UPROPERTY(Transient)
    FSalvageContestForceSnapshot ContestedHumanSnapshot;

    UPROPERTY(Transient)
    FSalvageContestForceSnapshot ContestedEnemySnapshot;

    void ClearSalvageContestState();

    void ActivateSalvageContest(class UStrategySiteDefinition* Site, class UMissionGroup* HumanMission,
        class UMissionGroup* EnemyMission, const FSalvageContestForceSnapshot& HumanSnapshot,
        const FSalvageContestForceSnapshot& EnemySnapshot);

private:
    UFUNCTION()
    void OnDayPassed(int32 NewDay);
};