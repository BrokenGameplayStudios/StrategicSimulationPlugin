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

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnResearchListChanged OnResearchListChanged;

    UFUNCTION(BlueprintCallable, Category = "Research")
    bool IsResearchInProgress(EFactionType Faction, UResearchTechDefinition* Tech) const;

    UFUNCTION(BlueprintCallable, Category = "Research")
    bool HasCompletedResearch(EFactionType Faction, UResearchTechDefinition* Tech) const;

    UFUNCTION(BlueprintCallable, Category = "Research")
    void ResetResearch();
};