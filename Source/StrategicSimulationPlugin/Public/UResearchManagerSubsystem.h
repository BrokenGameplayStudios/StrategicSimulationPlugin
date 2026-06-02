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

    UFUNCTION(BlueprintCallable, Category = "Research")
    UActiveResearchProject* StartResearch(EFactionType Faction, UResearchTechDefinition* ProjectDef);

    UFUNCTION(BlueprintCallable, Category = "Research")
    TArray<UActiveResearchProject*> GetActiveResearch(EFactionType Faction) const;

    /** Starts the next available research for this faction if a lab slot is free.
           Uses the ResearchDatabase below to iterate the tech tree in order. */
    UFUNCTION(BlueprintCallable, Category = "Research")
    bool TryResearch(EFactionType Faction);

    /** Master list of ALL research tech definitions (loaded once at game start).
        This is the same data you see in the "[RESEARCH DATABASE] Loaded 6 research techs" log.
        It lets TryResearch know what the next project should be. */
    UPROPERTY(BlueprintReadOnly, Category = "Research")
    TArray<UResearchTechDefinition*> ResearchDatabase;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnResearchListChanged OnResearchListChanged;

    UFUNCTION(BlueprintCallable, Category = "Research")
    bool IsResearchInProgress(EFactionType Faction, UResearchTechDefinition* Tech) const;

    UFUNCTION(BlueprintCallable, Category = "Research")
    bool HasCompletedResearch(EFactionType Faction, UResearchTechDefinition* Tech) const;

    UFUNCTION(BlueprintCallable, Category = "Research")
    void ResetResearch();
};