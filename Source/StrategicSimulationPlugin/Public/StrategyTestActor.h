#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StrategyTestActor.generated.h"

/** Development actor that spawns the strategic HUD and subscribes to key events for Phase 13 testing. */
UCLASS()
class STRATEGICSIMULATIONPLUGIN_API AStrategyTestActor : public AActor
{
    GENERATED_BODY()

public:
    /** Disables tick; sets default salvage and radar overlay widget classes. */
    AStrategyTestActor();

protected:
    /** Automatically runs RunPhase13Test on level start. */
    virtual void BeginPlay() override;

    /** Spawns WBP_StrategicHUD plus salvage/radar overlays and binds test event handlers. */
    UFUNCTION(BlueprintCallable, CallInEditor)
    void RunPhase13Test();

    /** Optional override for the main strategic HUD widget class. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    TSubclassOf<UUserWidget> StrategicHUDClass;

    /** Fog-aware salvage wreck overlay for WBP_StrategicHUD (PR-5). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    TSubclassOf<class UStrategySalvageMapWidget> SalvageMapWidgetClass;

    /** Passive radar contact overlay — click to launch interception (PR-11). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    TSubclassOf<class UStrategyRadarContactMapWidget> RadarContactMapWidgetClass;

private:
    /** Test handler: logs OnSoldierRecruited events. */
    UFUNCTION()
    void OnSoldierRecruited_Test(EFactionType Faction, UStrategySoldier* Soldier);

    /** Test handler: logs OnResearchCompleted events. */
    UFUNCTION()
    void OnResearchCompleted_Test(EFactionType Faction, UResearchTechDefinition* Tech);

    /** Test handler: logs salvage-site OnSiteDiscovered events. */
    UFUNCTION()
    void OnSiteDiscovered_Test(EFactionType Faction, class UStrategySiteDefinition* Site, EDiscoveryReason Reason);

    /** Test handler: logs OnSalvageContestStarted with fleet counts. */
    UFUNCTION()
    void OnSalvageContestStarted_Test(class UStrategySiteDefinition* Site,
        struct FSalvageContestForceSnapshot HumanForceSnapshot,
        struct FSalvageContestForceSnapshot EnemyForceSnapshot);
};