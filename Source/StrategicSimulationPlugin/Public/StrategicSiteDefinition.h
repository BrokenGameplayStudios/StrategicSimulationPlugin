#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "StrategicSimulationTypes.h"
#include "StrategicSiteDefinition.generated.h"

/** Kind of strategic map site (base candidate, resource node, wreck, or POI). */
UENUM(BlueprintType)
enum class EStrategySiteType : uint8
{
    PotentialBase,
    ResourceNode,
    SalvageSite,
    PointOfInterest
};

/** Lifecycle state for a vehicle wreck salvage site on the strategic map. */
UENUM(BlueprintType)
enum class ESalvageSiteState : uint8
{
    Active,
    Depleted,
    Removed
};

/** How a site became known to a faction (radar sweep vs combat engagement). */
UENUM(BlueprintType)
enum class EDiscoveryReason : uint8
{
    Radar   UMETA(DisplayName = "Radar"),
    Combat  UMETA(DisplayName = "Combat")
};

class UVehicleDefinition;
class UStrategySoldier;

/** Data asset describing a strategic map site: location, resources, and salvage wreck metadata. */
UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UStrategySiteDefinition : public UDataAsset
{
    GENERATED_BODY()

public:
    // === Identity (design-time / map authoring) ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    FString SiteName = TEXT("Unnamed Site");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    EStrategySiteType SiteType = EStrategySiteType::PotentialBase;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity",
        meta = (ToolTip = "Logical map position in pixels (matches vehicle waypoints and radar)."))
    FVector2D Location;

    // === Runtime state ===
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Runtime")
    FGuid SiteId;

    /** Faction that owned the vehicle destroyed at this salvage site (Human = blue, Enemy = red on debug map). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Salvage|Runtime")
    EFactionType WreckOwnerFaction = EFactionType::Neutral;

    /** Factions that know this wreck location without radar (combat engagement participants). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Salvage|Runtime")
    TArray<EFactionType> KnownFactions;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Salvage|Runtime")
    ESalvageSiteState SalvageState = ESalvageSiteState::Active;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Salvage|Runtime")
    TSoftObjectPtr<UVehicleDefinition> SourceVehicleDefinition;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Salvage|Runtime")
    int32 CreatedOnSimulationDay = 0;

    /** Simulation day when this wreck is removed if not salvaged (CreatedOnSimulationDay + expiry). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Salvage|Runtime")
    int32 SalvageExpiresOnDay = 0;

    /** Soldiers missing after the crash (rescue/POW eligible while wreck is active). */
    UPROPERTY(VisibleAnywhere, Transient, Category = "Salvage|Runtime")
    TArray<UStrategySoldier*> MIASoldiers;

    /** Soldiers killed in the vehicle destruction (not mission KIA). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Salvage|Runtime")
    int32 KIACrashCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Runtime",
        meta = (ToolTip = "True after a base has been built or a one-time resource node has been fully harvested."))
    bool bHasBeenUsed = false;

    // === Resources (design-time seed + runtime depletion) ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resources|Seed",
        meta = (ToolTip = "Starting extractable stockpile when the site is generated or authored."))
    FResourceStockpile MaxResources;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resources|Runtime")
    FResourceStockpile CurrentResources;
};