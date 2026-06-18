#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "StrategicSimulationTypes.h"
#include "UMissionGroup.generated.h"

class UStrategyVehicle;
class UStrategyBase;

/** Qualitative result of a completed strategic mission simulation. */
UENUM(BlueprintType)
enum class EMissionOutcome : uint8
{
    Success             UMETA(DisplayName = "Success"),
    PartialSuccess      UMETA(DisplayName = "Partial Success"),
    Failure             UMETA(DisplayName = "Failure"),
    CatastrophicFailure UMETA(DisplayName = "Catastrophic Failure")
};

/** Runtime lifecycle state of an active or finished mission group. */
UENUM(BlueprintType)
enum class EMissionStatus : uint8
{
    InProgress,
    Returning,
    Completed,
    Failed
};

/** Runtime mission instance tracking a fleet, schedule, targets, and post-mission results. */
UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UMissionGroup : public UObject
{
    GENERATED_BODY()

public:
    /** Initializes default status and display name for a newly created mission. */
    UMissionGroup();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission")
    TArray<UStrategyVehicle*> VehiclesInFleet;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission")
    UStrategyBase* OriginBase;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission")
    int32 StartDay;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission")
    int32 DurationDays;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission")
    EMissionStatus Status;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission")
    FText MissionName;

    /** NEW for Phase 2: Mission Type — controls simulation flow */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission")
    EMissionType MissionType;

    /** NEW: Attacking faction (required for symmetric POW + resource handling) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission")
    EFactionType AttackingFaction = EFactionType::Enemy;

    // === NEW: Mission Results (Phase 3) ===
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission|Result")
    EMissionOutcome Outcome;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission|Result")
    FResourceStockpile ResourcesGained;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission|Result")
    int32 SoldiersKilled = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission|Result")
    int32 VehiclesLost = 0;

    UPROPERTY()
    bool bIsLiveMovement = false;

    /** Absolute game-hour timestamp when fleet departs (used for staggered daily launches) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission|Schedule")
    float ScheduledLaunchGameHours = 0.f;

    /** True once vehicles have left the hangar and live movement has started */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission|Schedule")
    bool bMovementActivated = false;

    /** Enemy base targeted by an Offensive mission (AI vs AI simulated assault) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission|Target")
    UStrategyBase* TargetEnemyBase = nullptr;

    /** Salvage wreck targeted by a Salvage mission */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission|Target")
    class UStrategySiteDefinition* TargetSalvageSite = nullptr;

    /** True once the base-attack arrival placeholder has been logged for this mission */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission|Target")
    bool bBaseAttackArrivalLogged = false;

    /** Passive-radar contact that spawned an Interception mission (PR-11). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission|Target")
    FGuid TargetContactId;

    /** Live enemy vehicle resolved from a radar contact. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission|Target")
    TWeakObjectPtr<UStrategyVehicle> TargetInterceptVehicle;

    /** Potential base site targeted by a BaseExpansion mission. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission|Target")
    class UStrategySiteDefinition* TargetExpansionSite = nullptr;

    /** Base name to use when the expansion vehicle claims the site. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission|Target")
    FText PendingExpansionBaseName;

    /** Base shell created after a successful site claim (guard until Command Center completes). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission|Target")
    TWeakObjectPtr<UStrategyBase> ExpansionBaseUnderConstruction;
};