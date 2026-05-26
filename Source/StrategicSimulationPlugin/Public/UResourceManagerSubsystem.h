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

    // Get current resources for a faction
    UFUNCTION(BlueprintCallable, Category = "Resources")
    FResourceStockpile GetResources(EFactionType Faction) const;

    // Add or subtract resources (negative values = spend)
    UFUNCTION(BlueprintCallable, Category = "Resources")
    void AddResources(EFactionType Faction, const FResourceStockpile& Amount);

    // Set resources to exact values (used by save/load)
    UFUNCTION(BlueprintCallable, Category = "Resources")
    void SetResources(EFactionType Faction, const FResourceStockpile& NewStock);

    // Calculate and add income from all operational facilities
    UFUNCTION(BlueprintCallable, Category = "Resources")
    void ApplyFacilityIncome(EFactionType Faction);

    // Debug helper
    UFUNCTION(BlueprintCallable, Category = "Debug")
    void PrintAllResources() const;

private:
    // One stockpile per faction
    UPROPERTY(VisibleAnywhere, Transient, Category = "Resources")
    TMap<EFactionType, FResourceStockpile> FactionResources;
};