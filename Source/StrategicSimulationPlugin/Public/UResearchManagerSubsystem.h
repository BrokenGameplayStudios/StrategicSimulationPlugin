#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "StrategicSimulationTypes.h"
#include "UActiveResearchProject.h"
#include "UResearchTechDefinition.h"
#include "UTimeManagerSubsystem.h"
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