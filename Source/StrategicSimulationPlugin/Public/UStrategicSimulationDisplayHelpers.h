#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "StrategicSimulationTypes.h"
#include "StrategicSiteDefinition.h"
#include "UStrategicSimulationDisplayHelpers.generated.h"

class UBaseManagerSubsystem;
class UFactionIntelSubsystem;
class UStrategyCampaignSubsystem;

USTRUCT(BlueprintType)
struct FSalvageMapMarker
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Salvage Map")
    TObjectPtr<UStrategySiteDefinition> Site = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "Salvage Map")
    FVector2D WidgetPosition = FVector2D::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Salvage Map")
    FLinearColor Color = FLinearColor::White;

    UPROPERTY(BlueprintReadOnly, Category = "Salvage Map")
    FText Tooltip;
};

USTRUCT(BlueprintType)
struct FRadarContactMapMarker
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Radar Contact Map")
    FGuid ContactId;

    UPROPERTY(BlueprintReadOnly, Category = "Radar Contact Map")
    FVector2D WidgetPosition = FVector2D::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Radar Contact Map")
    FLinearColor Color = FLinearColor::White;

    UPROPERTY(BlueprintReadOnly, Category = "Radar Contact Map")
    FText Tooltip;

    UPROPERTY(BlueprintReadOnly, Category = "Radar Contact Map")
    bool bIsInboundThreat = false;

    UPROPERTY(BlueprintReadOnly, Category = "Radar Contact Map")
    bool bCanIntercept = false;

    UPROPERTY(BlueprintReadOnly, Category = "Radar Contact Map")
    bool bAlreadyTargeted = false;

    /** 1.0 = fresh, approaches 0 as contact nears expiry. */
    UPROPERTY(BlueprintReadOnly, Category = "Radar Contact Map")
    float StalenessAlpha = 1.0f;
};

/** Blueprint function library for HUD/map display strings, colors, and marker layout. */
UCLASS()
class STRATEGICSIMULATIONPLUGIN_API UStrategicSimulationDisplayHelpers : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Returns the short enum name (e.g. "Command", "LivingQuarters") without the EFacilityType:: prefix. */
    UFUNCTION(BlueprintPure, Category = "Strategic Simulation|Display")
    static FString GetFacilityTypeShortName(EFacilityType FacilityType);

    /** Returns a human-readable display name, using UMETA(DisplayName) when available. */
    UFUNCTION(BlueprintPure, Category = "Strategic Simulation|Display")
    static FText GetFacilityTypeDisplayName(EFacilityType FacilityType);

    /** Formats a facility count for HUD text (e.g. "Command, 1"). */
    UFUNCTION(BlueprintPure, Category = "Strategic Simulation|Display")
    static FString FormatFacilityCount(EFacilityType FacilityType, int32 Count);

    /** Returns a human-readable site type name from EStrategySiteType UMETA(DisplayName). */
    UFUNCTION(BlueprintPure, Category = "Strategic Simulation|Display")
    static FText GetSiteTypeDisplayName(EStrategySiteType SiteType);

    /** Returns a short status string for a site (wreck state, base built, available, etc.). */
    UFUNCTION(BlueprintPure, Category = "Strategic Simulation|Display")
    static FString GetSiteStatusDisplayText(const class UStrategySiteDefinition* Site);

    /** Faction-aware status using last-known base-built state when stale intel is enabled. */
    UFUNCTION(BlueprintPure, Category = "Strategic Simulation|Display", meta = (WorldContext = "WorldContextObject"))
    static FString GetSiteStatusDisplayTextForFaction(const class UStrategySiteDefinition* Site, EFactionType ViewerFaction,
        const UObject* WorldContextObject);

    /** Returns the map marker color for a wreck based on WreckOwnerFaction (blue/red/gray). */
    UFUNCTION(BlueprintPure, Category = "Strategic Simulation|Display")
    static FLinearColor GetSalvageWreckColor(EFactionType WreckOwnerFaction);

    /** Returns true for Transport, Support, and Scout vehicles that can run salvage missions. */
    UFUNCTION(BlueprintPure, Category = "Strategic Simulation|Display|Salvage")
    static bool IsSalvageCapableVehicleType(EVehicleType VehicleType);

    /** Returns true when Site is an active salvage wreck known to ViewerFaction via fog-of-war rules. */
    UFUNCTION(BlueprintPure, Category = "Strategic Simulation|Display")
    static bool ShouldShowSalvageToFaction(const class UStrategySiteDefinition* Site, EFactionType ViewerFaction,
        const class UBaseManagerSubsystem* BaseManager);

    /** Returns true when Site is visible to ViewerFaction (salvage rules or discovered-site list). */
    UFUNCTION(BlueprintPure, Category = "Strategic Simulation|Display")
    static bool ShouldShowSiteToFaction(const class UStrategySiteDefinition* Site, EFactionType ViewerFaction,
        const class UBaseManagerSubsystem* BaseManager);

    /** Returns true when salvage sites and site persistence are both enabled on the campaign. */
    UFUNCTION(BlueprintPure, Category = "Strategic Simulation|Display|Salvage Map")
    static bool IsPlayerSalvageMapLayerEnabled(const UStrategyCampaignSubsystem* Campaign);

    /** Computes aspect-fit uniform scale from widget size to logical map dimensions. */
    UFUNCTION(BlueprintPure, Category = "Strategic Simulation|Display|Salvage Map")
    static float GetMapUniformScale(FVector2D WidgetSize, const UStrategyCampaignSubsystem* Campaign);

    /** Converts a logical map position to centered widget coordinates with optional scale multiplier. */
    UFUNCTION(BlueprintPure, Category = "Strategic Simulation|Display|Salvage Map")
    static FVector2D MapLogicalToWidgetPosition(FVector2D LogicalPosition, FVector2D WidgetSize,
        const UStrategyCampaignSubsystem* Campaign, float MapScaleMultiplier = 0.85f);

    /** Builds multi-line tooltip text for a salvage wreck (resources, days left, stale intel). */
    UFUNCTION(BlueprintPure, Category = "Strategic Simulation|Display|Salvage Map")
    static FText FormatSalvageTooltipText(const UStrategySiteDefinition* Site, const UBaseManagerSubsystem* BaseManager,
        EFactionType ViewerFaction);

    /** Formats a short discovery toast when a wreck is first revealed to Faction. */
    UFUNCTION(BlueprintPure, Category = "Strategic Simulation|Display|Salvage Map")
    static FText FormatSalvageDiscoveryToast(EFactionType Faction, const UStrategySiteDefinition* Site,
        EDiscoveryReason Reason);

    /** Builds positioned salvage map markers for all wrecks visible to ViewerFaction. */
    UFUNCTION(BlueprintCallable, Category = "Strategic Simulation|Display|Salvage Map", meta = (WorldContext = "WorldContextObject"))
    static TArray<FSalvageMapMarker> BuildSalvageMapMarkers(const UObject* WorldContextObject, EFactionType ViewerFaction,
        FVector2D WidgetSize, float MapScaleMultiplier = 0.85f);

    /** Returns active salvage sites that pass ShouldShowSalvageToFaction for ViewerFaction. */
    UFUNCTION(BlueprintCallable, Category = "Strategic Simulation|Display|Salvage Map", meta = (WorldContext = "WorldContextObject"))
    static TArray<UStrategySiteDefinition*> GetVisibleSalvageSitesForFaction(const UObject* WorldContextObject,
        EFactionType ViewerFaction);

    /** Returns true for Gunship and Heavy vehicles used in interception combat. */
    UFUNCTION(BlueprintPure, Category = "Strategic Simulation|Display|Radar Contact")
    static bool IsCombatVehicleType(EVehicleType VehicleType);

    /** Returns true when passive base radar contacts are enabled on the campaign. */
    UFUNCTION(BlueprintPure, Category = "Strategic Simulation|Display|Radar Contact Map")
    static bool IsPlayerRadarContactLayerEnabled(const UStrategyCampaignSubsystem* Campaign);

    /** Picks marker color from threat state, intercept availability, and targeting status. */
    UFUNCTION(BlueprintPure, Category = "Strategic Simulation|Display|Radar Contact Map")
    static FLinearColor GetRadarContactMarkerColor(const struct FRadarContact& Contact, bool bCanIntercept, bool bAlreadyTargeted);

    /** Returns alpha 1.0 (fresh) down to 0.15 as the contact approaches expiry age. */
    UFUNCTION(BlueprintPure, Category = "Strategic Simulation|Display|Radar Contact Map")
    static float GetRadarContactStalenessAlpha(const struct FRadarContact& Contact, float CurrentGameHours, float ExpiryHours);

    /** Builds multi-line tooltip text for a radar contact (position, speed, intercept action). */
    UFUNCTION(BlueprintPure, Category = "Strategic Simulation|Display|Radar Contact Map")
    static FText FormatRadarContactTooltipText(const struct FRadarContact& Contact, bool bCanIntercept, bool bAlreadyTargeted,
        float CurrentGameHours = 0.0f, float ExpiryHours = 6.0f);

    /** Formats a short toast when a new radar contact is first detected. */
    UFUNCTION(BlueprintPure, Category = "Strategic Simulation|Display|Radar Contact Map")
    static FText FormatRadarContactDiscoveryToast(const struct FRadarContact& Contact);

    /** Builds positioned radar contact markers for all contacts visible to ViewerFaction. */
    UFUNCTION(BlueprintCallable, Category = "Strategic Simulation|Display|Radar Contact Map", meta = (WorldContext = "WorldContextObject"))
    static TArray<FRadarContactMapMarker> BuildRadarContactMapMarkers(const UObject* WorldContextObject, EFactionType ViewerFaction,
        FVector2D WidgetSize, float MapScaleMultiplier = 0.85f);
};