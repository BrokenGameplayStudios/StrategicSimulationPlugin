#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "StrategicSimulationTypes.h"
#include "UStrategyFacility.h"
#include "UFacilityDefinition.h"
#include "UStrategyBase.h"
#include "UTimeManagerSubsystem.h"
#include "UBaseManagerSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFacilityListChanged, EFactionType, Faction);  // kept for backward compat
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBaseListChanged, EFactionType, Faction);

UCLASS()
class STRATEGICSIMULATIONPLUGIN_API UBaseManagerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    // === NEW: Multiple Bases ===
    UFUNCTION(BlueprintCallable, Category = "Base")
    UStrategyBase* BuildNewBase(EFactionType Faction, FText BaseName, FVector2D MapLocation);

    UFUNCTION(BlueprintCallable, Category = "Base")
    const TArray<UStrategyBase*>& GetBases(EFactionType Faction) const;

    UFUNCTION(BlueprintCallable, Category = "Base")
    bool CanBuildNewBase(EFactionType Faction) const;

    UFUNCTION(BlueprintCallable, Category = "Base")
    int32 GetNumberOfOperationalHangers(EFactionType Faction) const;

    // === Legacy functions (now aggregate across all bases) ===
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
    const TArray<UStrategyFacility*>& GetFacilities(EFactionType Faction) const;  // returns all facilities across bases (for compatibility)

    // Get available vehicle parking slots in all hangers
    UFUNCTION(BlueprintCallable, Category = "Base")
    int32 GetTotalAvailableHangerSlots(EFactionType Faction) const;

    // NEW: Full reset (used by Campaign ResetSimulation)
    UFUNCTION(BlueprintCallable, Category = "Base")
    void ResetAllBases();

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnFacilityListChanged OnFacilityListChanged;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnBaseListChanged OnBaseListChanged;

private:
    UPROPERTY(VisibleAnywhere, Transient, Category = "Bases")
    TArray<UStrategyBase*> HumanBases;

    UPROPERTY(VisibleAnywhere, Transient, Category = "Bases")
    TArray<UStrategyBase*> EnemyBases;

    UFUNCTION()
    void OnDayPassed(int32 NewDay);

    // Internal helper
    TArray<UStrategyBase*>& GetMutableBases(EFactionType Faction);
    const TArray<UStrategyBase*>& GetBasesInternal(EFactionType Faction) const;
};