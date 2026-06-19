#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "StrategicSimulationTypes.h"
#include "UActiveResearchProject.h"
#include "UResearchTechDefinition.h"
#include "UTimeManagerSubsystem.h"
#include "UBaseManagerSubsystem.h"
#include "UResearchManagerSubsystem.generated.h"

/** Broadcast when a faction's active research list changes (start, complete, or reset). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnResearchListChanged, EFactionType, Faction);

/** Game-instance subsystem that assigns research jobs to laboratory facilities. */
UCLASS()
class STRATEGICSIMULATIONPLUGIN_API UResearchManagerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /** Logs subsystem initialization; research state lives on facility production queues. */
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /** Finds a free lab slot for Faction and starts ProjectDef; broadcasts OnResearchListChanged on success. */
    UFUNCTION(BlueprintCallable, Category = "Research")
    UActiveResearchProject* StartResearch(EFactionType Faction, UResearchTechDefinition* ProjectDef);

    /** Builds transient UActiveResearchProject snapshots from all laboratory jobs for Faction. */
    UFUNCTION(BlueprintCallable, Category = "Research")
    TArray<UActiveResearchProject*> GetActiveResearch(EFactionType Faction) const;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnResearchListChanged OnResearchListChanged;

    /** Returns true when Tech is currently queued in any laboratory for Faction. */
    UFUNCTION(BlueprintCallable, Category = "Research")
    bool IsResearchInProgress(EFactionType Faction, UResearchTechDefinition* Tech) const;

    /** Returns whether Tech has been completed by Faction (stub: always true until per-faction tracking exists). */
    UFUNCTION(BlueprintCallable, Category = "Research")
    bool HasCompletedResearch(EFactionType Faction, UResearchTechDefinition* Tech) const;

    /** Clears all research jobs from Human laboratories and notifies both factions. */
    UFUNCTION(BlueprintCallable, Category = "Research")
    void ResetResearch();
};