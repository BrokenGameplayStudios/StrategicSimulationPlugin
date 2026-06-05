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

    // === AI / Simulation Settings ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign | AI")
    int32 MaxAIBases = 4;   // ← change this in the editor / BP to control AI expansion globally

    /** Full reset of the simulation (call from UI for New Game) */
    UFUNCTION(BlueprintCallable, Category = "Simulation")
    void ResetSimulation();

    // Save / Load
    UFUNCTION(BlueprintCallable, Category = "Campaign")
    void SaveCampaign(int32 SlotIndex = 1);

    UFUNCTION(BlueprintCallable, Category = "Campaign")
    void LoadCampaign(int32 SlotIndex = 1);
        
    // === POW/KIA SETTINGS (Phase 1) ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POW/KIA|Settings")
    float POWCaptureChanceOnVictory = 0.35f;     // 35% chance to capture enemy soldiers as POWs when you win

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POW/KIA|Settings")
    float KIAChanceOnVictory = 0.25f;            // 25% chance enemy soldiers become KIA (recoverable later)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POW/KIA|Settings")
    float EnemyPOWCaptureChanceOnDefeat = 0.20f; // when you lose, chance enemy captures YOUR troops as POWs

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POW/KIA|Settings")
    float EnemyKIAChanceOnDefeat = 0.40f;        // when you lose, chance your troops become KIA

    // Debug / testing
    UFUNCTION(BlueprintCallable, Category = "POW/KIA|Debug")
    void SetPOWChance(float NewCaptureChance, float NewKIAChance);

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
    bool bVerboseLogging = true;

    /** Debug toggles (controlled from AStrategyGameInitializer in the level) */    
    bool bShowUnlockMessages = true;
    bool bShowFacilityTicks = false;

    /** Tracks already announced unlocks to prevent daily spam */
    TSet<FString> AnnouncedUnlocks;

private:
    UFUNCTION()
    void OnDayPassed(int32 NewDay);
};