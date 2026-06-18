#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StrategyTestActor.generated.h"

UCLASS()
class STRATEGICSIMULATIONPLUGIN_API AStrategyTestActor : public AActor
{
    GENERATED_BODY()

public:
    AStrategyTestActor();

protected:
    virtual void BeginPlay() override;

    UFUNCTION(BlueprintCallable, CallInEditor)
    void RunPhase13Test();

    // Assign your HUD widget here in the Details panel
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    TSubclassOf<UUserWidget> StrategicHUDClass;

    /** Fog-aware salvage wreck overlay for WBP_StrategicHUD (PR-5). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    TSubclassOf<class UStrategySalvageMapWidget> SalvageMapWidgetClass;

private:
    UFUNCTION()
    void OnSoldierRecruited_Test(EFactionType Faction, UStrategySoldier* Soldier);

    UFUNCTION()
    void OnResearchCompleted_Test(EFactionType Faction, UResearchTechDefinition* Tech);

    UFUNCTION()
    void OnSiteDiscovered_Test(EFactionType Faction, class UStrategySiteDefinition* Site, EDiscoveryReason Reason);

    UFUNCTION()
    void OnSalvageContestStarted_Test(class UStrategySiteDefinition* Site,
        struct FSalvageContestForceSnapshot HumanForceSnapshot,
        struct FSalvageContestForceSnapshot EnemyForceSnapshot);
};