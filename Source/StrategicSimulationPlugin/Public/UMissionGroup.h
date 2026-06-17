#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "StrategicSimulationTypes.h"
#include "UMissionGroup.generated.h"

class UStrategyVehicle;
class UStrategyBase;

UENUM(BlueprintType)
enum class EMissionOutcome : uint8
{
    Success             UMETA(DisplayName = "Success"),
    PartialSuccess      UMETA(DisplayName = "Partial Success"),
    Failure             UMETA(DisplayName = "Failure"),
    CatastrophicFailure UMETA(DisplayName = "Catastrophic Failure")
};

UENUM(BlueprintType)
enum class EMissionStatus : uint8
{
    InProgress,
    Returning,
    Completed,
    Failed
};

UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UMissionGroup : public UObject
{
    GENERATED_BODY()

public:
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
};