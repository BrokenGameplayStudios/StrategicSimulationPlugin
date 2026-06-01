#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "StrategicSimulationTypes.h"
#include "UResourceManagerSubsystem.generated.h"

UCLASS()
class STRATEGICSIMULATIONPLUGIN_API UResourceManagerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    UFUNCTION(BlueprintCallable, Category = "Resources")
    FResourceStockpile GetResources(EFactionType Faction) const;

    UFUNCTION(BlueprintCallable, Category = "Resources")
    void AddResources(EFactionType Faction, const FResourceStockpile& Amount);

    UFUNCTION(BlueprintCallable, Category = "Resources")
    void SetResources(EFactionType Faction, const FResourceStockpile& NewStock);

    UFUNCTION(BlueprintCallable, Category = "Resources")
    void ApplyFacilityIncome(EFactionType Faction);

    UFUNCTION(BlueprintCallable, Category = "Debug")
    void PrintAllResources() const;

    UFUNCTION(BlueprintCallable, Category = "Resources")
    bool CanAfford(EFactionType Faction, const FResourceStockpile& Cost) const;

    UFUNCTION(BlueprintCallable, Category = "Resources")
    bool SubtractResources(EFactionType Faction, const FResourceStockpile& Cost);

    UFUNCTION(BlueprintCallable, Category = "Resources")
    void ResetResources(EFactionType Faction);

    // === NEW: Configurable starting stockpiles ===
    UFUNCTION(BlueprintCallable, Category = "Resources|Starting")
    void SetHumanStartingResources(const FResourceStockpile& NewStart);

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