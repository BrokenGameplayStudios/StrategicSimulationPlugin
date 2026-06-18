#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "StrategicSimulationTypes.h"
#include "UResourceManagerSubsystem.generated.h"

/** Game-instance subsystem that tracks per-faction resource stockpiles and daily facility income. */
UCLASS()
class STRATEGICSIMULATIONPLUGIN_API UResourceManagerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /** Seeds Human/Enemy starting stockpiles and registers them in FactionResources. */
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /** Returns the current stockpile for the given faction (zeros if none registered). */
    UFUNCTION(BlueprintCallable, Category = "Resources")
    FResourceStockpile GetResources(EFactionType Faction) const;

    /** Adds Amount to every resource field for Faction (creates the entry if missing). */
    UFUNCTION(BlueprintCallable, Category = "Resources")
    void AddResources(EFactionType Faction, const FResourceStockpile& Amount);

    /** Replaces the entire stockpile for Faction with NewStock. */
    UFUNCTION(BlueprintCallable, Category = "Resources")
    void SetResources(EFactionType Faction, const FResourceStockpile& NewStock);

    /** Sums operational facility ProductionPerDay across all bases and credits Faction. */
    UFUNCTION(BlueprintCallable, Category = "Resources")
    void ApplyFacilityIncome(EFactionType Faction);

    /** Logs every faction stockpile to the output log for debugging. */
    UFUNCTION(BlueprintCallable, Category = "Debug")
    void PrintAllResources() const;

    /** Returns true when Faction's stockpile meets or exceeds Cost on all fields. */
    UFUNCTION(BlueprintCallable, Category = "Resources")
    bool CanAfford(EFactionType Faction, const FResourceStockpile& Cost) const;

    /** Deducts Cost when affordable; logs a warning and returns false otherwise. */
    UFUNCTION(BlueprintCallable, Category = "Resources")
    bool SubtractResources(EFactionType Faction, const FResourceStockpile& Cost);

    /** Clears Faction stockpile; Enemy receives a small default grant for AI testing. */
    UFUNCTION(BlueprintCallable, Category = "Resources")
    void ResetResources(EFactionType Faction);

    /** Updates HumanStartingResources and live Human stockpile when already initialized. */
    UFUNCTION(BlueprintCallable, Category = "Resources|Starting")
    void SetHumanStartingResources(const FResourceStockpile& NewStart);

    /** Updates EnemyStartingResources and live Enemy stockpile when already initialized. */
    UFUNCTION(BlueprintCallable, Category = "Resources|Starting")
    void SetEnemyStartingResources(const FResourceStockpile& NewStart);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resources|Starting")
    FResourceStockpile HumanStartingResources;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resources|Starting")
    FResourceStockpile EnemyStartingResources;

private:
    UPROPERTY(VisibleAnywhere, Transient, Category = "Resources")
    TMap<EFactionType, FResourceStockpile> FactionResources;
};