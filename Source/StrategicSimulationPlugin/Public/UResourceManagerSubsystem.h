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

    // Simple tick — will be called by Time Manager later (income over time)
    UFUNCTION(BlueprintCallable, Category = "Resources")
    void TickResources(float DeltaTime);   // for now we ignore DeltaTime and just give flat income

    // Set resources to exact values (used by save/load)
    UFUNCTION(BlueprintCallable, Category = "Resources")
    void SetResources(EFactionType Faction, const FResourceStockpile& NewStock);

    // Debug helper
    UFUNCTION(BlueprintCallable, Category = "Debug")
    void PrintAllResources() const;

private:
    // One stockpile per faction
    UPROPERTY(VisibleAnywhere, Transient, Category = "Resources")
    TMap<EFactionType, FResourceStockpile> FactionResources;
};