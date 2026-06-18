#include "UStrategicSimulationDisplayHelpers.h"
#include "StrategicSiteDefinition.h"
#include "UBaseManagerSubsystem.h"
#include "UStrategyCampaignSubsystem.h"
#include "UVehicleDefinition.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

FString UStrategicSimulationDisplayHelpers::GetFacilityTypeShortName(EFacilityType FacilityType)
{
    const UEnum* Enum = StaticEnum<EFacilityType>();
    if (!Enum)
    {
        return TEXT("Unknown");
    }

    return Enum->GetNameStringByValue(static_cast<int64>(FacilityType));
}

FText UStrategicSimulationDisplayHelpers::GetFacilityTypeDisplayName(EFacilityType FacilityType)
{
    const UEnum* Enum = StaticEnum<EFacilityType>();
    if (!Enum)
    {
        return FText::FromString(TEXT("Unknown"));
    }

    return Enum->GetDisplayNameTextByValue(static_cast<int64>(FacilityType));
}

FString UStrategicSimulationDisplayHelpers::FormatFacilityCount(EFacilityType FacilityType, int32 Count)
{
    return FString::Printf(TEXT("%s, %d"), *GetFacilityTypeShortName(FacilityType), Count);
}

FText UStrategicSimulationDisplayHelpers::GetSiteTypeDisplayName(EStrategySiteType SiteType)
{
    const UEnum* Enum = StaticEnum<EStrategySiteType>();
    if (!Enum)
    {
        return FText::FromString(TEXT("Unknown"));
    }
    return Enum->GetDisplayNameTextByValue(static_cast<int64>(SiteType));
}

FString UStrategicSimulationDisplayHelpers::GetSiteStatusDisplayText(const UStrategySiteDefinition* Site)
{
    if (!Site)
    {
        return TEXT("Unknown");
    }

    switch (Site->SiteType)
    {
    case EStrategySiteType::SalvageSite:
        if (Site->SalvageState == ESalvageSiteState::Removed)
        {
            return TEXT("Removed");
        }
        if (Site->SalvageState == ESalvageSiteState::Depleted)
        {
            return TEXT("Depleted (Salvage)");
        }
        return TEXT("Active Wreck");
    case EStrategySiteType::PotentialBase:
        return Site->bHasBeenUsed ? TEXT("Base Built") : TEXT("Available");
    default:
        return Site->bHasBeenUsed ? TEXT("Used") : TEXT("Available");
    }
}

bool UStrategicSimulationDisplayHelpers::IsSalvageCapableVehicleType(EVehicleType VehicleType)
{
    return VehicleType == EVehicleType::Transport
        || VehicleType == EVehicleType::Support
        || VehicleType == EVehicleType::Scout;
}

FLinearColor UStrategicSimulationDisplayHelpers::GetSalvageWreckColor(EFactionType WreckOwnerFaction)
{
    if (WreckOwnerFaction == EFactionType::Human)
    {
        return FLinearColor::Blue;
    }
    if (WreckOwnerFaction == EFactionType::Enemy)
    {
        return FLinearColor::Red;
    }
    return FLinearColor(0.7f, 0.7f, 0.7f, 0.9f);
}

bool UStrategicSimulationDisplayHelpers::ShouldShowSalvageToFaction(const UStrategySiteDefinition* Site,
    EFactionType ViewerFaction, const UBaseManagerSubsystem* BaseManager)
{
    if (!Site || !BaseManager || Site->SiteType != EStrategySiteType::SalvageSite)
    {
        return false;
    }

    if (Site->SalvageState != ESalvageSiteState::Active)
    {
        return false;
    }

    return BaseManager->IsSiteKnownToFaction(ViewerFaction, Site);
}

bool UStrategicSimulationDisplayHelpers::ShouldShowSiteToFaction(const UStrategySiteDefinition* Site,
    EFactionType ViewerFaction, const UBaseManagerSubsystem* BaseManager)
{
    if (!Site || !BaseManager)
    {
        return false;
    }

    if (Site->SiteType == EStrategySiteType::SalvageSite)
    {
        return ShouldShowSalvageToFaction(Site, ViewerFaction, BaseManager);
    }

    const TArray<UStrategySiteDefinition*>& Discovered =
        (ViewerFaction == EFactionType::Human) ? BaseManager->DiscoveredSitesHuman : BaseManager->DiscoveredSitesEnemy;

    return Discovered.Contains(Site);
}

bool UStrategicSimulationDisplayHelpers::IsPlayerSalvageMapLayerEnabled(const UStrategyCampaignSubsystem* Campaign)
{
    return Campaign && Campaign->bSalvageSitesEnabled && Campaign->bSitesPersistenceEnabled;
}

float UStrategicSimulationDisplayHelpers::GetMapUniformScale(FVector2D WidgetSize, const UStrategyCampaignSubsystem* Campaign)
{
    float LogicalWidth = 1920.0f;
    float LogicalHeight = 1080.0f;

    if (Campaign)
    {
        LogicalWidth = Campaign->LogicalMapWidth;
        LogicalHeight = Campaign->LogicalMapHeight;
    }

    if (WidgetSize.X <= KINDA_SMALL_NUMBER || WidgetSize.Y <= KINDA_SMALL_NUMBER)
    {
        return 1.0f;
    }

    const float ScaleX = WidgetSize.X / LogicalWidth;
    const float ScaleY = WidgetSize.Y / LogicalHeight;
    return FMath::Min(ScaleX, ScaleY);
}

FVector2D UStrategicSimulationDisplayHelpers::MapLogicalToWidgetPosition(FVector2D LogicalPosition, FVector2D WidgetSize,
    const UStrategyCampaignSubsystem* Campaign, float MapScaleMultiplier)
{
    float LogicalWidth = 1920.0f;
    float LogicalHeight = 1080.0f;

    if (Campaign)
    {
        LogicalWidth = Campaign->LogicalMapWidth;
        LogicalHeight = Campaign->LogicalMapHeight;
    }

    const float UniformScale = GetMapUniformScale(WidgetSize, Campaign) * MapScaleMultiplier;
    const FVector2D ScaledPos = LogicalPosition * UniformScale;
    const FVector2D WidgetCenter(WidgetSize.X * 0.5f, WidgetSize.Y * 0.5f);
    const FVector2D LogicalCenter(LogicalWidth * 0.5f, LogicalHeight * 0.5f);

    return WidgetCenter + (ScaledPos - LogicalCenter * UniformScale);
}

FText UStrategicSimulationDisplayHelpers::FormatSalvageTooltipText(const UStrategySiteDefinition* Site,
    const UBaseManagerSubsystem* BaseManager)
{
    if (!Site)
    {
        return FText::GetEmpty();
    }

    int32 DaysRemaining = -1;
    if (BaseManager)
    {
        DaysRemaining = BaseManager->GetSalvageDaysRemaining(Site);
    }

    const FResourceStockpile& Res = Site->CurrentResources;
    if (DaysRemaining >= 0)
    {
        return FText::FromString(FString::Printf(
            TEXT("%s\nRemaining — M: %d  Mt: %d  Chem: %d  Exo: %d\nDays left: %d"),
            *Site->SiteName,
            Res.Money, Res.Metals, Res.Chemicals, Res.ExoticMaterial,
            DaysRemaining));
    }

    return FText::FromString(FString::Printf(
        TEXT("%s\nRemaining — M: %d  Mt: %d  Chem: %d  Exo: %d"),
        *Site->SiteName,
        Res.Money, Res.Metals, Res.Chemicals, Res.ExoticMaterial));
}

FText UStrategicSimulationDisplayHelpers::FormatSalvageDiscoveryToast(EFactionType Faction,
    const UStrategySiteDefinition* Site, EDiscoveryReason Reason)
{
    if (!Site)
    {
        return FText::GetEmpty();
    }

    const FString ReasonText = (Reason == EDiscoveryReason::Combat) ? TEXT("combat") : TEXT("radar");
    return FText::FromString(FString::Printf(TEXT("Wreck discovered (%s): %s"),
        *ReasonText, *Site->SiteName));
}

TArray<UStrategySiteDefinition*> UStrategicSimulationDisplayHelpers::GetVisibleSalvageSitesForFaction(
    const UObject* WorldContextObject, EFactionType ViewerFaction)
{
    TArray<UStrategySiteDefinition*> VisibleSites;

    if (!WorldContextObject)
    {
        return VisibleSites;
    }

    UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
    if (!World || !World->GetGameInstance())
    {
        return VisibleSites;
    }

    UStrategyCampaignSubsystem* Campaign = World->GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();
    UBaseManagerSubsystem* BaseManager = World->GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
    if (!IsPlayerSalvageMapLayerEnabled(Campaign) || !BaseManager)
    {
        return VisibleSites;
    }

    for (UStrategySiteDefinition* Site : BaseManager->AllPotentialSites)
    {
        if (ShouldShowSalvageToFaction(Site, ViewerFaction, BaseManager))
        {
            VisibleSites.Add(Site);
        }
    }

    return VisibleSites;
}

TArray<FSalvageMapMarker> UStrategicSimulationDisplayHelpers::BuildSalvageMapMarkers(const UObject* WorldContextObject,
    EFactionType ViewerFaction, FVector2D WidgetSize, float MapScaleMultiplier)
{
    TArray<FSalvageMapMarker> Markers;

    if (!WorldContextObject)
    {
        return Markers;
    }

    UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
    if (!World || !World->GetGameInstance())
    {
        return Markers;
    }

    UStrategyCampaignSubsystem* Campaign = World->GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();
    UBaseManagerSubsystem* BaseManager = World->GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
    if (!IsPlayerSalvageMapLayerEnabled(Campaign) || !BaseManager)
    {
        return Markers;
    }

    const TArray<UStrategySiteDefinition*> VisibleSites = GetVisibleSalvageSitesForFaction(WorldContextObject, ViewerFaction);
    Markers.Reserve(VisibleSites.Num());

    for (UStrategySiteDefinition* Site : VisibleSites)
    {
        if (!Site)
        {
            continue;
        }

        FSalvageMapMarker Marker;
        Marker.Site = Site;
        Marker.WidgetPosition = MapLogicalToWidgetPosition(Site->Location, WidgetSize, Campaign, MapScaleMultiplier);
        Marker.Color = GetSalvageWreckColor(Site->WreckOwnerFaction);
        Marker.Tooltip = FormatSalvageTooltipText(Site, BaseManager);
        Markers.Add(Marker);
    }

    return Markers;
}