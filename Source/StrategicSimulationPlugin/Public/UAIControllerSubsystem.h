#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "StrategicSimulationTypes.h"
#include "UItemDefinition.h"
#include "UTimeManagerSubsystem.h"
#include "UBaseManagerSubsystem.h"
#include "UAIControllerSubsystem.generated.h"

UCLASS()
class STRATEGICSIMULATIONPLUGIN_API UAIControllerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    UFUNCTION(BlueprintCallable, Category = "AI Control")
    void SetAIEnabled(bool bEnable);

    UFUNCTION(BlueprintCallable, Category = "AI Control")
    bool IsAIEnabled() const;

    // === NEW: Per-faction flags (default ON) ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Control")
    bool bSimulateHumanAI = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Control")
    bool bSimulateEnemyAI = true;

    // === NEW: Per-faction simulation toggles ===
    UFUNCTION(BlueprintCallable, Category = "AI Control")
    void SetSimulateHumanAI(bool bEnable);

    UFUNCTION(BlueprintCallable, Category = "AI Control")
    void SetSimulateEnemyAI(bool bEnable);

    UFUNCTION(BlueprintCallable, Category = "AI Control")
    bool IsSimulatingHumanAI() const;

    UFUNCTION(BlueprintCallable, Category = "AI Control")
    bool IsSimulatingEnemyAI() const;

    UFUNCTION()
    void OnDayPassed(int32 NewDay);

    UFUNCTION(BlueprintCallable, Category = "Debug")
    void Debug_RunAI();

    // Public so Campaign can call it if needed
    void RunAIForFaction(EFactionType Faction, int32 CurrentDay);

    // === NEW: Player-callable version (so UI/PlayerController can trigger the same logic later) ===
    UFUNCTION(BlueprintCallable, Category = "AI Control|Player Ready")
    void PerformDailyBuildOrder(EFactionType Faction);

private:

    int32 LastProcessedAIDay = -1;  // Prevents double AI execution on the same day

    UPROPERTY(VisibleAnywhere, Category = "AI Control")
    bool bAIEnabled = true;

    bool TryRecruit(EFactionType Faction);
    bool TryBuyAndEquip(EFactionType Faction);
    bool TryBuildFacility(EFactionType Faction, EFacilityType FacilityTypeToBuild, UStrategyBase* TargetBase = nullptr);
    bool TryResearch(EFactionType Faction);
    bool TryBuildVehicle(EFactionType Faction, UStrategyBase* TargetBase);

    /** Returns the base with the fewest parked vehicles (for staggered distribution) */
    UStrategyBase* GetBaseWithFewestVehicles(EFactionType Faction) const;
};