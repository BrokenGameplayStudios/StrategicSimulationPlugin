#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "StrategicSimulationTypes.h"
#include "UActiveResearchProject.h"
#include "UResearchTechDefinition.h"
#include "UTimeManagerSubsystem.h"
#include "UBaseManagerSubsystem.h"
#include "UResearchManagerSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnResearchListChanged, EFactionType, Faction);

UCLASS()
class STRATEGICSIMULATIONPLUGIN_API UResearchManagerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    // Start a new research project
    UFUNCTION(BlueprintCallable, Category = "Research")
    UActiveResearchProject* StartResearch(EFactionType Faction, UResearchTechDefinition* ProjectDef);

    // Get all active research for a faction
    UFUNCTION(BlueprintCallable, Category = "Research")
    TArray<UActiveResearchProject*> GetActiveResearch(EFactionType Faction) const;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnResearchListChanged OnResearchListChanged;

    // Returns true if this research is currently active for the faction
    UFUNCTION(BlueprintCallable, Category = "Research")
    bool IsResearchInProgress(EFactionType Faction, UResearchTechDefinition* Tech) const;

    // Returns true if this research has been completed
    UFUNCTION(BlueprintCallable, Category = "Research")
    bool HasCompletedResearch(EFactionType Faction, UResearchTechDefinition* Tech) const;

    // Advance all active research for a faction (called every day by AI)
    UFUNCTION(BlueprintCallable, Category = "Research")
    void AdvanceDay(EFactionType Faction);

    // NEW: Full reset (used by Campaign ResetSimulation)
    UFUNCTION(BlueprintCallable, Category = "Research")
    void ResetResearch();

private:
    // Two separate arrays (UHT-friendly)
    UPROPERTY(VisibleAnywhere, Transient, Category = "Research")
    TArray<UActiveResearchProject*> HumanResearchQueue;

    UPROPERTY(VisibleAnywhere, Transient, Category = "Research")
    TArray<UActiveResearchProject*> EnemyResearchQueue;

    // Called by Time Manager every day
    UFUNCTION()
    void OnDayPassed(int32 NewDay);
};