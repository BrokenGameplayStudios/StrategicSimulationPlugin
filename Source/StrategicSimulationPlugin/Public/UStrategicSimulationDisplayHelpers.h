#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "StrategicSimulationTypes.h"
#include "StrategicSiteDefinition.h"
#include "UStrategicSimulationDisplayHelpers.generated.h"

class UBaseManagerSubsystem;
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

    UFUNCTION(BlueprintPure, Category = "Strategic Simulation|Display")
    static FText GetSiteTypeDisplayName(EStrategySiteType SiteType);

    UFUNCTION(BlueprintPure, Category = "Strategic Simulation|Display")
    static FString GetSiteStatusDisplayText(const class UStrategySiteDefinition* Site);

    UFUNCTION(BlueprintPure, Category = "Strategic Simulation|Display")
    static FLinearColor GetSalvageWreckColor(EFactionType WreckOwnerFaction);

    UFUNCTION(BlueprintPure, Category = "Strategic Simulation|Display")
    static bool ShouldShowSalvageToFaction(const class UStrategySiteDefinition* Site, EFactionType ViewerFaction,
        const class UBaseManagerSubsystem* BaseManager);

    UFUNCTION(BlueprintPure, Category = "Strategic Simulation|Display")
    static bool ShouldShowSiteToFaction(const class UStrategySiteDefinition* Site, EFactionType ViewerFaction,
        const class UBaseManagerSubsystem* BaseManager);

    UFUNCTION(BlueprintPure, Category = "Strategic Simulation|Display|Salvage Map")
    static bool IsPlayerSalvageMapLayerEnabled(const UStrategyCampaignSubsystem* Campaign);

    UFUNCTION(BlueprintPure, Category = "Strategic Simulation|Display|Salvage Map")
    static float GetMapUniformScale(FVector2D WidgetSize, const UStrategyCampaignSubsystem* Campaign);

    UFUNCTION(BlueprintPure, Category = "Strategic Simulation|Display|Salvage Map")
    static FVector2D MapLogicalToWidgetPosition(FVector2D LogicalPosition, FVector2D WidgetSize,
        const UStrategyCampaignSubsystem* Campaign, float MapScaleMultiplier = 0.85f);

    UFUNCTION(BlueprintPure, Category = "Strategic Simulation|Display|Salvage Map")
    static FText FormatSalvageTooltipText(const UStrategySiteDefinition* Site, const UBaseManagerSubsystem* BaseManager);

    UFUNCTION(BlueprintPure, Category = "Strategic Simulation|Display|Salvage Map")
    static FText FormatSalvageDiscoveryToast(EFactionType Faction, const UStrategySiteDefinition* Site,
        EDiscoveryReason Reason);

    UFUNCTION(BlueprintCallable, Category = "Strategic Simulation|Display|Salvage Map", meta = (WorldContext = "WorldContextObject"))
    static TArray<FSalvageMapMarker> BuildSalvageMapMarkers(const UObject* WorldContextObject, EFactionType ViewerFaction,
        FVector2D WidgetSize, float MapScaleMultiplier = 0.85f);

    UFUNCTION(BlueprintCallable, Category = "Strategic Simulation|Display|Salvage Map", meta = (WorldContext = "WorldContextObject"))
    static TArray<UStrategySiteDefinition*> GetVisibleSalvageSitesForFaction(const UObject* WorldContextObject,
        EFactionType ViewerFaction);
};