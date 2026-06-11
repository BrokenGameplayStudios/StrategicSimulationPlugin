#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "StrategicSimulationTypes.h"
#include "UStrategyFacility.h"
#include "UFacilityDefinition.h"
#include "StrategicSiteDefinition.h"
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
    UStrategyBase* BuildNewBase(EFactionType Faction, FText BaseName, FVector2D MapLocation);

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

    /** Called by vehicles during recon when they find something good */
    UFUNCTION(BlueprintCallable, Category = "Expansion")
    UStrategySiteDefinition* AddDiscoveredSite(EFactionType Faction, FVector2D Location, EStrategySiteType Type = EStrategySiteType::PotentialBase, float OptionalScore = 0.0f);

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