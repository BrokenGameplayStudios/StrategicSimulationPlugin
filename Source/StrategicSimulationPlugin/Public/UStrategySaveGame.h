#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "StrategicSimulationTypes.h"
#include "StrategicSiteDefinition.h"
#include "UFactionIntelSubsystem.h"
#include "UStrategySaveGame.generated.h"

/** Minimum schema version that includes site-map round-trip data (PR-4). */
constexpr int32 StrategySiteMapSaveSchemaVersion = 2;

/** Schema version that adds per-faction stale site intel snapshots (PR-9). */
constexpr int32 StrategyIntelSaveSchemaVersion = 3;

/** Serializable snapshot of one strategic site for save/load round-trip (bases, wrecks, resources). */
USTRUCT(BlueprintType)
struct FStrategySiteSaveData
{
    GENERATED_BODY()

    /** Stable identifier used to match sites on deserialize. */
    UPROPERTY()
    FGuid SiteId;

    /** Logical map position in pixels. */
    UPROPERTY()
    FVector2D Location = FVector2D::ZeroVector;

    /** Site role: potential base, wreck, resource node, etc. */
    UPROPERTY()
    EStrategySiteType SiteType = EStrategySiteType::PotentialBase;

    /** Faction that owned the vehicle destroyed at a salvage wreck site. */
    UPROPERTY()
    EFactionType WreckOwnerFaction = EFactionType::Neutral;

    /** Display/debug name persisted for save metadata and logs. */
    UPROPERTY()
    FString SiteName;

    /** Maximum extractable resources when the site was created. */
    UPROPERTY()
    FResourceStockpile MaxResources;

    /** Remaining resources at save time. */
    UPROPERTY()
    FResourceStockpile CurrentResources;

    /** True after a one-time resource site has been fully harvested. */
    UPROPERTY()
    bool bHasBeenUsed = false;

    /** Salvage lifecycle state for wreck sites. */
    UPROPERTY()
    ESalvageSiteState SalvageState = ESalvageSiteState::Active;

    /** Simulation day index when the site (especially wrecks) was created. */
    UPROPERTY()
    int32 CreatedOnSimulationDay = 0;

    /** Simulation day when an unclaimed wreck is auto-removed. */
    UPROPERTY()
    int32 SalvageExpiresOnDay = 0;

    /** Count of KIA from the crash that created this wreck. */
    UPROPERTY()
    int32 KIACrashCount = 0;

    /** Soft path to the destroyed vehicle definition for wreck visuals/rules. */
    UPROPERTY()
    FSoftObjectPath SourceVehicleDefinitionPath;

    /** Factions that have discovered or been told about this site. */
    UPROPERTY()
    TArray<EFactionType> KnownFactions;

    /** Human faction has this site in their discovered set. */
    UPROPERTY()
    bool bDiscoveredByHuman = false;

    /** Enemy faction has this site in their discovered set. */
    UPROPERTY()
    bool bDiscoveredByEnemy = false;
};

/**
 * USaveGame payload for strategic campaign persistence.
 * Stores schema version, site map, stale intel snapshots, calendar day, resources, and slot metadata.
 */
UCLASS()
class STRATEGICSIMULATIONPLUGIN_API UStrategySaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    /** Format version; LoadCampaign rejects saves below StrategySiteMapSaveSchemaVersion. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    int32 SaveSchemaVersion = 0;

    /** True when save includes continued-campaign site/intel data (not a dev fast-save). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    bool bIsContinuedCampaign = false;

    /** Full strategic site map serialized from UBaseManagerSubsystem. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    TArray<FStrategySiteSaveData> SavedSites;

    /** Stale intel snapshots for the human faction at save time. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    TArray<FSiteIntelSnapshot> SavedIntelHuman;

    /** Stale intel snapshots for the enemy faction at save time. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    TArray<FSiteIntelSnapshot> SavedIntelEnemy;

    /** Calendar day-of-month component restored via AdvanceDays on load. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    int32 CurrentDay = 1;

    /** Human faction resource stockpile at save time. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    FResourceStockpile HumanResources;

    /** Enemy faction resource stockpile at save time. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    FResourceStockpile EnemyResources;

    /** Real-world timestamp shown on the save slot selection screen. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    FDateTime LastSavedTime;

    /** Short human-readable summary for save UI (e.g. soldier count). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    FText HumanSummary;   // e.g. "12 Soldiers, 4 Facilities"

    /** Human roster size cached for save slot display. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    int32 HumanSoldierCount = 0;
};