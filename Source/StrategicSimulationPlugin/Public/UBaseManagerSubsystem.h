#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "StrategicSimulationTypes.h"
#include "UStrategyFacility.h"
#include "UFacilityDefinition.h"
#include "UTimeManagerSubsystem.h"
#include "UBaseManagerSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFacilityListChanged, EFactionType, Faction);

UCLASS()
class STRATEGICSIMULATIONPLUGIN_API UBaseManagerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    // Build a new facility
    UFUNCTION(BlueprintCallable, Category = "Base")
    UStrategyFacility* BuildFacility(EFactionType Faction, UFacilityDefinition* FacilityDef);

    // Power grid stats
    UFUNCTION(BlueprintCallable, Category = "Base")
    int32 GetTotalPowerProvided(EFactionType Faction) const;
    UFUNCTION(BlueprintCallable, Category = "Base")
    int32 GetTotalPowerDrawn(EFactionType Faction) const;
    UFUNCTION(BlueprintCallable, Category = "Base")
    int32 GetNetPower(EFactionType Faction) const;   // positive = surplus 

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnFacilityListChanged OnFacilityListChanged;

    // Returns total barracks capacity across all operational facilities
    UFUNCTION(BlueprintCallable, Category = "Base")
    int32 GetTotalBarracksCapacity(EFactionType Faction) const;

    // Returns true if the faction already has this facility type (building OR completed)
    UFUNCTION(BlueprintCallable, Category = "Base")
    bool HasFacilityOfType(EFactionType Faction, EFacilityType FacilityType) const;

    // Called every day to advance construction of all facilities (reliable fallback)
    UFUNCTION(BlueprintCallable, Category = "Base")
    void AdvanceFacilityConstruction(EFactionType Faction);

    // Public accessor so other subsystems can read built facilities
    UFUNCTION(BlueprintCallable, Category = "Base")
    const TArray<UStrategyFacility*>& GetFacilities(EFactionType Faction) const;

private:
    // Two separate arrays (UHT-friendly)
    UPROPERTY(VisibleAnywhere, Transient, Category = "Base")
    TArray<UStrategyFacility*> HumanFacilities;

    UPROPERTY(VisibleAnywhere, Transient, Category = "Base")
    TArray<UStrategyFacility*> EnemyFacilities;

    UFUNCTION()
    void OnDayPassed(int32 NewDay);
};