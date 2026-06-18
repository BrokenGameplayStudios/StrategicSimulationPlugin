#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "StrategicSimulationTypes.h"
#include "USoldierClassDefinition.h"
#include "UItemDefinition.h"
#include "UStrategySoldier.generated.h"

class UStrategyBase;
class UStrategyFacility;
class UMissionGroup;   // ← Forward declaration (prevents compile errors)

/**
 * Runtime soldier with class stats, loadout bonuses, health/status,
 * and station/mission/POW/MIA tracking for strategic simulation.
 */
UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UStrategySoldier : public UObject
{
    GENERATED_BODY()

public:
    /** Default-constructs health, status, and POW/KIA flags. */
    UStrategySoldier();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Soldier")
    FString SoldierName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Soldier")
    USoldierClassDefinition* ClassDefinition;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Stats")
    FSoldierStats CurrentStats;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Progress")
    int32 XP = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Progress")
    int32 Rank = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Status")
    ESoldierStatus Status = ESoldierStatus::Healthy;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Status")
    bool bIsWounded = false;

    // === NEW: POW / KIA flags (Phase 1) ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "POW/KIA")
    bool bIsPOW = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "POW/KIA")
    bool bIsKIA = false;

    /** Missing in action at a vehicle wreck — rescueable by owning faction or POW if enemy salvages. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "POW/KIA")
    bool bIsMIA = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "POW/KIA")
    FGuid WreckSiteId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Status")
    int32 DaysUntilRecovered = 0;

    /** Permanent reserved barracks slot — belongs to this soldier until death */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Location")
    UStrategyFacility* HomeBarracks = nullptr;

    /** Which base this soldier is currently stationed at */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Location")
    UStrategyBase* StationedBase = nullptr;

    /** Which mission this soldier is currently on (parallel to vehicles) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission")
    UMissionGroup* CurrentMission = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Loadout")
    TArray<TSoftObjectPtr<UItemDefinition>> CurrentLoadout;

    /** Reduces health and updates wounded/dead status. */
    UFUNCTION(BlueprintCallable, Category = "Soldier|Damage")
    void ApplyDamage(int32 DamageAmount);

    /** True when the soldier is wounded but not dead. */
    UFUNCTION(BlueprintCallable, Category = "Soldier|Healing")
    bool NeedsHealing() const;

    /** Restores health up to maximum and refreshes status. */
    UFUNCTION(BlueprintCallable, Category = "Soldier|Healing")
    void Heal(int32 Amount);

    /** Derives ESoldierStatus and recovery days from CurrentStats.Health. */
    UFUNCTION(BlueprintCallable, Category = "Soldier|Healing")
    void UpdateStatusFromHealth();

    /** Logs soldier name, class, health, and status flags to the output log. */
    UFUNCTION(BlueprintCallable, Category = "Debug")
    void PrintInfo() const;

    /** Calculates effective stats incorporating class base + loadout item bonuses (Aim/Armor/etc.). Used by UI + mission sim. */
    UFUNCTION(BlueprintCallable, Category = "Soldier|Stats")
    FSoldierStats GetEffectiveStats() const;

    /** Effective Aim after class base and loadout item bonuses. */
    UFUNCTION(BlueprintCallable, Category = "Soldier|Stats")
    int32 GetEffectiveAim() const;

    /** Effective Defense (primarily armor bonuses from loadout). */
    UFUNCTION(BlueprintCallable, Category = "Soldier|Stats")
    int32 GetEffectiveDefense() const;
};