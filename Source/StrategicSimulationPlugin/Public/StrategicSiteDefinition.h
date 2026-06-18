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
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Site")
    FGuid SiteId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Site")
    FVector2D Location;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Site")
    EStrategySiteType SiteType = EStrategySiteType::PotentialBase;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Site")
    EFactionType DiscoveringFaction = EFactionType::Human;

    /** Faction that owned the vehicle destroyed at this salvage site (Human = blue, Enemy = red on debug map). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Site|Salvage")
    EFactionType WreckOwnerFaction = EFactionType::Neutral;

    /** Factions that know this wreck location without radar (combat engagement participants). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Site|Salvage")
    TArray<EFactionType> KnownFactions;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Site|Salvage")
    ESalvageSiteState SalvageState = ESalvageSiteState::Active;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Site|Salvage")
    TSoftObjectPtr<UVehicleDefinition> SourceVehicleDefinition;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Site|Salvage")
    int32 CreatedOnSimulationDay = 0;

    /** Simulation day when this wreck is removed if not salvaged (CreatedOnSimulationDay + expiry). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Site|Salvage")
    int32 SalvageExpiresOnDay = 0;

    /** Soldiers missing after the crash (rescue/POW eligible while wreck is active). */
    UPROPERTY(VisibleAnywhere, Transient, Category = "Site|Salvage")
    TArray<UStrategySoldier*> MIASoldiers;

    /** Soldiers killed in the vehicle destruction (not mission KIA). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Site|Salvage")
    int32 KIACrashCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Site")
    bool bHasBeenUsed = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Site")
    FString SiteName = TEXT("Unnamed Site");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Site|Resources")
    FResourceStockpile MaxResources;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Site|Resources")
    FResourceStockpile CurrentResources;
};