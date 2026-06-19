#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "StrategicSimulationTypes.h"
#include "UItemDefinition.h"
#include "UTimeManagerSubsystem.h"
#include "UBaseManagerSubsystem.h"
#include "UAIControllerSubsystem.generated.h"

class UVehicleDefinition;
class UStrategyVehicle;

/**
 * Game-instance subsystem that runs daily AI build, recruit, research,
 * vehicle production, mission scheduling, and combat engagement decisions.
 */
UCLASS()
class STRATEGICSIMULATIONPLUGIN_API UAIControllerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /** Binds to the time manager and logs AI simulation flags. */
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /** Maximum number of bases the AI is allowed to build (you said 10) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI | Expansion")
    int32 MaxBases = 10;

    /** Enables or disables AI processing for both factions. */
    UFUNCTION(BlueprintCallable, Category = "AI Control")
    void SetAIEnabled(bool bEnable);

    /** Returns whether the global AI controller is enabled. */
    UFUNCTION(BlueprintCallable, Category = "AI Control")
    bool IsAIEnabled() const;

    // === NEW: Per-faction flags (default ON) ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Control")
    bool bSimulateHumanAI = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Control")
    bool bSimulateEnemyAI = true;

    // === NEW: Per-faction simulation toggles ===
    /** Enables or disables daily AI simulation for the Human faction. */
    UFUNCTION(BlueprintCallable, Category = "AI Control")
    void SetSimulateHumanAI(bool bEnable);

    /** Enables or disables daily AI simulation for the Enemy faction. */
    UFUNCTION(BlueprintCallable, Category = "AI Control")
    void SetSimulateEnemyAI(bool bEnable);

    /** Returns whether Human faction AI is being simulated. */
    UFUNCTION(BlueprintCallable, Category = "AI Control")
    bool IsSimulatingHumanAI() const;

    /** Returns whether Enemy faction AI is being simulated. */
    UFUNCTION(BlueprintCallable, Category = "AI Control")
    bool IsSimulatingEnemyAI() const;

    /** Daily tick: advances construction and runs AI for enabled factions. */
    UFUNCTION()
    void OnDayPassed(int32 NewDay);

    /** Manually triggers one AI pass for both enabled factions (debug). */
    UFUNCTION(BlueprintCallable, Category = "Debug")
    void Debug_RunAI();

    /** Executes the full daily AI routine for one faction (build, recruit, missions, expand). */
    void RunAIForFaction(EFactionType Faction, int32 CurrentDay);

    /** AI decision logic when one of its vehicles detects an enemy vehicle */
    void HandleVehicleDetection(UStrategyVehicle* DetectingVehicle, UStrategyVehicle* DetectedVehicle);

    /** Clears per-faction day guards so a restarted campaign can run Day 1 AI again. */
    void ResetDailyProcessingState();

    /** True for Gunship and Heavy vehicle types used in combat decisions. */
    static bool IsCombatVehicleType(EVehicleType Type);

private:

    /** Per-faction last processed day guard (prevents double-tick on same day). */
    UPROPERTY(VisibleAnywhere, Category = "AI Control")
    TMap<EFactionType, int32> LastProcessedDayPerFaction;

    UPROPERTY(VisibleAnywhere, Category = "AI Control")
    bool bAIEnabled = true;

    /** Queues soldier training up to barracks capacity and budget. */
    bool TryRecruit(EFactionType Faction);
    /** Purchases and equips priority items on under-geared soldiers. */
    bool TryBuyAndEquip(EFactionType Faction);
    /** Starts facility construction at a base after resource and MaxBuilt checks. */
    bool TryBuildFacility(EFactionType Faction, EFacilityType FacilityTypeToBuild, UStrategyBase* TargetBase = nullptr);
    /** Starts the next priority research project if a lab is available. */
    bool TryResearch(EFactionType Faction);
    /** Queues vehicle production in hangar slots at the target base. */
    bool TryBuildVehicle(EFactionType Faction, UStrategyBase* TargetBase);

    /** Chooses scout vs combat vehicle definition based on fleet composition. */
    UVehicleDefinition* SelectVehicleDefinitionToBuild(EFactionType Faction) const;
    /** Picks Recon, Salvage, Interception, Defensive, or Offensive for an idle vehicle. */
    EMissionType PickAIMissionTypeForVehicle(UStrategyVehicle* Vehicle, int32 CurrentDay) const;
    /** True when the detecting vehicle should attack the detected enemy. */
    bool ShouldEngageVehicle(UStrategyVehicle* DetectingVehicle, UStrategyVehicle* DetectedVehicle) const;
    /** True when en-route intercept should take priority over normal mission pathing. */
    bool ShouldPrioritizeEnRouteIntercept(UStrategyVehicle* DetectingVehicle, UStrategyVehicle* DetectedVehicle) const;
    /** True when an enemy vehicle is inbound toward a friendly faction base. */
    bool IsEnemyInboundToFaction(const UStrategyVehicle* EnemyVehicle, EFactionType FriendlyFaction) const;

    /** True for Scout, Transport, and Support vehicle types. */
    static bool IsReconVehicleType(EVehicleType Type);
    /** Counts parked and on-mission vehicles matching the given types. */
    int32 CountFactionVehiclesOfTypes(EFactionType Faction, const TArray<EVehicleType>& Types) const;

    /** Finds a suitable discovered but unused site for AI expansion */
    UStrategySiteDefinition* FindExpansionSiteForAI(EFactionType Faction) const;

    /** Picks nearest idle vehicle (prefer combat) and orders a BaseExpansion mission. */
    bool TryStartAIExpansion(EFactionType Faction) const;

    /** True when the faction has inbound-threat radar contacts (blocks preempting defensive missions). */
    bool FactionHasInboundThreatContacts(EFactionType Faction) const;

    /** Returns the base with the fewest parked vehicles (for staggered distribution) */
    UStrategyBase* GetBaseWithFewestVehicles(EFactionType Faction) const;
};