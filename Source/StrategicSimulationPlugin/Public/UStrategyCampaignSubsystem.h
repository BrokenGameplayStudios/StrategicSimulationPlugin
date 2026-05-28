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

    /** Full reset of the simulation (call from UI for New Game) */
    UFUNCTION(BlueprintCallable, Category = "Simulation")
    void ResetSimulation();

    // Save / Load
    UFUNCTION(BlueprintCallable, Category = "Campaign")
    void SaveCampaign(int32 SlotIndex = 1);

    UFUNCTION(BlueprintCallable, Category = "Campaign")
    void LoadCampaign(int32 SlotIndex = 1);

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

    // Configurable Item Database
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    TSoftObjectPtr<class UItemDatabase> ItemDatabaseAsset;

    // === DATABASES ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Databases")
    TSoftObjectPtr<class UFacilityDatabase> FacilityDatabaseAsset;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Databases")
    TSoftObjectPtr<class USoldierClassDatabase> SoldierClassDatabaseAsset;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Databases")
    TSoftObjectPtr<class UResearchDatabase> ResearchDatabaseAsset;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Databases")
    TSoftObjectPtr<class UVehicleDatabase> VehicleDatabaseAsset;

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

private:
    UFUNCTION()
    void OnDayPassed(int32 NewDay);
};