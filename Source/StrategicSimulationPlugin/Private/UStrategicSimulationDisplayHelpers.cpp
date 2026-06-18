#include "UStrategicSimulationDisplayHelpers.h"
#include "StrategicSiteDefinition.h"
#include "UBaseManagerSubsystem.h"
#include "UFactionIntelSubsystem.h"
#include "URadarContactSubsystem.h"
#include "UMissionManagerSubsystem.h"
#include "UStrategyCampaignSubsystem.h"
#include "UVehicleDefinition.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

/** Returns the short enum name (e.g. "Command", "LivingQuarters") without the EFacilityType:: prefix. */
FString UStrategicSimulationDisplayHelpers::GetFacilityTypeShortName(EFacilityType FacilityType)
{
    const UEnum* Enum = StaticEnum<EFacilityType>();
    if (!Enum)
    {
        return TEXT("Unknown");
    }

    return Enum->GetNameStringByValue(static_cast<int64>(FacilityType));
}

/** Returns a human-readable display name, using UMETA(DisplayName) when available. */
FText UStrategicSimulationDisplayHelpers::GetFacilityTypeDisplayName(EFacilityType FacilityType)
{
    const UEnum* Enum = StaticEnum<EFacilityType>();
    if (!Enum)
    {
        return FText::FromString(TEXT("Unknown"));
    }

    return Enum->GetDisplayNameTextByValue(static_cast<int64>(FacilityType));
}

/** Formats a facility count for HUD text (e.g. "Command, 1"). */
FString UStrategicSimulationDisplayHelpers::FormatFacilityCount(EFacilityType FacilityType, int32 Count)
{
    return FString::Printf(TEXT("%s, %d"), *GetFacilityTypeShortName(FacilityType), Count);
}

/** Returns a human-readable site type name from EStrategySiteType UMETA(DisplayName). */
FText UStrategicSimulationDisplayHelpers::GetSiteTypeDisplayName(EStrategySiteType SiteType)
{
    const UEnum* Enum = StaticEnum<EStrategySiteType>();
    if (!Enum)
    {
        return FText::FromString(TEXT("Unknown"));
    }
    return Enum->GetDisplayNameTextByValue(static_cast<int64>(SiteType));
}

/** Returns a short status string for a site (wreck state, base built, available, etc.). */
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

/** Faction-aware status using last-known base-built state when stale intel is enabled. */
FString UStrategicSimulationDisplayHelpers::GetSiteStatusDisplayTextForFaction(const UStrategySiteDefinition* Site,
    EFactionType ViewerFaction, const UObject* WorldContextObject)
{
    if (!Site)
    {
        return TEXT("Unknown");
    }

    if (Site->SiteType == EStrategySiteType::SalvageSite)
    {
        return GetSiteStatusDisplayText(Site);
    }

    UFactionIntelSubsystem* IntelMgr = nullptr;
    if (WorldContextObject)
    {
        if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
        {
            if (World->GetGameInstance())
            {
                IntelMgr = World->GetGameInstance()->GetSubsystem<UFactionIntelSubsystem>();
            }
        }
    }

    if (!IntelMgr || !IntelMgr->IsStaleIntelEnabled())
    {
        return GetSiteStatusDisplayText(Site);
    }

    FString Status;
    if (Site->SiteType == EStrategySiteType::PotentialBase)
    {
        Status = IntelMgr->GetDisplayHasBase(ViewerFaction, Site) ? TEXT("Base Built") : TEXT("Available");
    }
    else
    {
        Status = IntelMgr->GetDisplayHasBase(ViewerFaction, Site) ? TEXT("Used") : TEXT("Available");
    }

    if (!IntelMgr->IsIntelFresh(ViewerFaction, Site))
    {
        Status += TEXT(" (intel stale)");
    }

    return Status;
}

/** Returns true for Transport, Support, and Scout vehicles that can run salvage missions. */
bool UStrategicSimulationDisplayHelpers::IsSalvageCapableVehicleType(EVehicleType VehicleType)
{
    return VehicleType == EVehicleType::Transport
        || VehicleType == EVehicleType::Support
        || VehicleType == EVehicleType::Scout;
}

/** Returns the map marker color for a wreck based on WreckOwnerFaction (blue/red/gray). */
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

/** Returns true when Site is an active salvage wreck known to ViewerFaction via fog-of-war rules. */
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

/** Returns true when Site is visible to ViewerFaction (salvage rules or discovered-site list). */
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

/** Returns true when salvage sites and site persistence are both enabled on the campaign. */
bool UStrategicSimulationDisplayHelpers::IsPlayerSalvageMapLayerEnabled(const UStrategyCampaignSubsystem* Campaign)
{
    return Campaign && Campaign->bSalvageSitesEnabled && Campaign->bSitesPersistenceEnabled;
}

/** Computes aspect-fit uniform scale from widget size to logical map dimensions. */
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

/** Converts a logical map position to centered widget coordinates with optional scale multiplier. */
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

/** Builds multi-line tooltip text for a salvage wreck (resources, days left, stale intel). */
FText UStrategicSimulationDisplayHelpers::FormatSalvageTooltipText(const UStrategySiteDefinition* Site,
    const UBaseManagerSubsystem* BaseManager, EFactionType ViewerFaction)
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

    UFactionIntelSubsystem* IntelMgr = nullptr;
    if (BaseManager && BaseManager->GetGameInstance())
    {
        IntelMgr = BaseManager->GetGameInstance()->GetSubsystem<UFactionIntelSubsystem>();
    }

    FResourceStockpile Res = Site->CurrentResources;
    bool bIntelStale = false;
    if (IntelMgr && IntelMgr->IsStaleIntelEnabled())
    {
        Res = IntelMgr->GetDisplayResources(ViewerFaction, Site);
        bIntelStale = !IntelMgr->IsIntelFresh(ViewerFaction, Site);
    }

    const FString StaleSuffix = bIntelStale ? TEXT("\nIntel stale") : TEXT("");

    if (DaysRemaining >= 0)
    {
        return FText::FromString(FString::Printf(
            TEXT("%s\nRemaining — M: %d  Mt: %d  Chem: %d  Exo: %d\nDays left: %d%s"),
            *Site->SiteName,
            Res.Money, Res.Metals, Res.Chemicals, Res.ExoticMaterial,
            DaysRemaining,
            *StaleSuffix));
    }

    return FText::FromString(FString::Printf(
        TEXT("%s\nRemaining — M: %d  Mt: %d  Chem: %d  Exo: %d%s"),
        *Site->SiteName,
        Res.Money, Res.Metals, Res.Chemicals, Res.ExoticMaterial,
        *StaleSuffix));
}

/** Formats a short discovery toast when a wreck is first revealed to Faction. */
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

/** Returns active salvage sites that pass ShouldShowSalvageToFaction for ViewerFaction. */
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

/** Builds positioned salvage map markers for all wrecks visible to ViewerFaction. */
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
        Marker.Tooltip = FormatSalvageTooltipText(Site, BaseManager, ViewerFaction);
        Markers.Add(Marker);
    }

    return Markers;
}

/** Returns true for Gunship and Heavy vehicles used in interception combat. */
bool UStrategicSimulationDisplayHelpers::IsCombatVehicleType(EVehicleType VehicleType)
{
    return VehicleType == EVehicleType::Gunship || VehicleType == EVehicleType::Heavy;
}

/** Returns true when passive base radar contacts are enabled on the campaign. */
bool UStrategicSimulationDisplayHelpers::IsPlayerRadarContactLayerEnabled(const UStrategyCampaignSubsystem* Campaign)
{
    return Campaign && Campaign->bBasePassiveRadarEnabled;
}

/** Picks marker color from threat state, intercept availability, and targeting status. */
FLinearColor UStrategicSimulationDisplayHelpers::GetRadarContactMarkerColor(const FRadarContact& Contact,
    bool bCanIntercept, bool bAlreadyTargeted)
{
    if (bAlreadyTargeted)
    {
        return FLinearColor(0.45f, 0.45f, 0.45f, 0.75f);
    }

    if (Contact.bIsInboundThreat)
    {
        return bCanIntercept
            ? FLinearColor(1.0f, 0.35f, 0.1f, 1.0f)
            : FLinearColor(0.9f, 0.2f, 0.2f, 0.85f);
    }

    return bCanIntercept
        ? FLinearColor(0.2f, 0.85f, 1.0f, 0.95f)
        : FLinearColor(0.55f, 0.75f, 0.95f, 0.7f);
}

/** Returns alpha 1.0 (fresh) down to 0.15 as the contact approaches expiry age. */
float UStrategicSimulationDisplayHelpers::GetRadarContactStalenessAlpha(const FRadarContact& Contact,
    float CurrentGameHours, float ExpiryHours)
{
    if (!Contact.ContactId.IsValid() || ExpiryHours <= KINDA_SMALL_NUMBER || CurrentGameHours <= 0.0f)
    {
        return 1.0f;
    }

    const float AgeHours = FMath::Max(0.0f, CurrentGameHours - Contact.LastSeenGameHours);
    return FMath::Clamp(1.0f - (AgeHours / ExpiryHours), 0.15f, 1.0f);
}

/** Builds multi-line tooltip text for a radar contact (position, speed, intercept action). */
FText UStrategicSimulationDisplayHelpers::FormatRadarContactTooltipText(const FRadarContact& Contact,
    bool bCanIntercept, bool bAlreadyTargeted, float CurrentGameHours, float ExpiryHours)
{
    if (!Contact.ContactId.IsValid())
    {
        return FText::GetEmpty();
    }

    const float Speed = Contact.EstimatedVelocity.Size();
    const float HeadingDeg = Contact.EstimatedHeadingDegrees;
    const float AgeHours = CurrentGameHours > 0.0f
        ? FMath::Max(0.0f, CurrentGameHours - Contact.LastSeenGameHours)
        : 0.0f;
    const float RemainingHours = FMath::Max(0.0f, ExpiryHours - AgeHours);

    FString ActionLine;
    if (bAlreadyTargeted)
    {
        ActionLine = TEXT("Interception in progress");
    }
    else if (bCanIntercept)
    {
        ActionLine = TEXT("Click to launch interception");
    }
    else
    {
        ActionLine = TEXT("No idle gunship in range");
    }

    const FString FactionLine = FString::Printf(TEXT("\nDetected by: %s"),
        *UEnum::GetValueAsString(Contact.DetectingFaction));

    const FString BaseLine = Contact.DetectingBaseName.IsEmpty()
        ? FString()
        : FString::Printf(TEXT(" (%s)"), *Contact.DetectingBaseName);

    const FString ThreatLine = Contact.ThreatenedBaseName.IsEmpty()
        ? FString()
        : FString::Printf(TEXT("\nThreatens: %s"), *Contact.ThreatenedBaseName);

    const FString StalenessLine = CurrentGameHours > 0.0f
        ? FString::Printf(TEXT("\nLast seen: %.1fh ago  Expires in: %.1fh"), AgeHours, RemainingHours)
        : FString();

    const FVector2D InterceptPos = URadarContactSubsystem::GetContactInterceptPosition(Contact);

    return FText::FromString(FString::Printf(
        TEXT("%s%s%s%s\nEntry point: (%.0f, %.0f)\nSpeed: %.0f px/h  Heading: %.0f deg%s%s\n%s"),
        *Contact.TrackedVehicleName,
        Contact.bIsInboundThreat ? TEXT(" — INBOUND") : TEXT(""),
        *FactionLine,
        *BaseLine,
        InterceptPos.X, InterceptPos.Y,
        Speed, HeadingDeg,
        *ThreatLine,
        *StalenessLine,
        *ActionLine));
}

/** Formats a short toast when a new radar contact is first detected. */
FText UStrategicSimulationDisplayHelpers::FormatRadarContactDiscoveryToast(const FRadarContact& Contact)
{
    if (!Contact.ContactId.IsValid())
    {
        return FText::GetEmpty();
    }

    const FString DetectorLine = Contact.DetectingBaseName.IsEmpty()
        ? UEnum::GetValueAsString(Contact.DetectingFaction)
        : FString::Printf(TEXT("%s (%s)"),
            *UEnum::GetValueAsString(Contact.DetectingFaction), *Contact.DetectingBaseName);

    return FText::FromString(FString::Printf(
        TEXT("Radar contact%s: %s\nDetected by %s"),
        Contact.bIsInboundThreat ? TEXT(" (INBOUND)") : TEXT(""),
        *Contact.TrackedVehicleName,
        *DetectorLine));
}

/** Builds positioned radar contact markers for all contacts visible to ViewerFaction. */
TArray<FRadarContactMapMarker> UStrategicSimulationDisplayHelpers::BuildRadarContactMapMarkers(
    const UObject* WorldContextObject, EFactionType ViewerFaction, FVector2D WidgetSize, float MapScaleMultiplier)
{
    TArray<FRadarContactMapMarker> Markers;

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
    URadarContactSubsystem* ContactMgr = World->GetGameInstance()->GetSubsystem<URadarContactSubsystem>();
    UMissionManagerSubsystem* MissionMgr = World->GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>();
    if (!IsPlayerRadarContactLayerEnabled(Campaign) || !ContactMgr)
    {
        return Markers;
    }

    const TArray<FRadarContact> Contacts = ContactMgr->GetContactsForFaction(ViewerFaction);
    const float CurrentGameHours = MissionMgr ? MissionMgr->GetCurrentGameHours() : 0.0f;
    const float ExpiryHours = Campaign ? FMath::Max(1.0f, Campaign->RadarContactExpiryHours) : 6.0f;
    Markers.Reserve(Contacts.Num());

    for (const FRadarContact& Contact : Contacts)
    {
        if (!Contact.ContactId.IsValid())
        {
            continue;
        }

        const bool bAlreadyTargeted = ContactMgr->IsContactAlreadyTargeted(Contact.ContactId);
        const bool bCanIntercept = MissionMgr
            ? MissionMgr->CanFactionInterceptContact(ViewerFaction, Contact.ContactId)
            : false;
        const float StalenessAlpha = GetRadarContactStalenessAlpha(Contact, CurrentGameHours, ExpiryHours);

        FRadarContactMapMarker Marker;
        Marker.ContactId = Contact.ContactId;
        Marker.WidgetPosition = MapLogicalToWidgetPosition(
            URadarContactSubsystem::GetContactInterceptPosition(Contact), WidgetSize, Campaign, MapScaleMultiplier);
        Marker.bIsInboundThreat = Contact.bIsInboundThreat;
        Marker.bCanIntercept = bCanIntercept;
        Marker.bAlreadyTargeted = bAlreadyTargeted;
        Marker.StalenessAlpha = StalenessAlpha;
        Marker.Color = GetRadarContactMarkerColor(Contact, bCanIntercept, bAlreadyTargeted);
        Marker.Color.A *= StalenessAlpha;
        Marker.Tooltip = FormatRadarContactTooltipText(Contact, bCanIntercept, bAlreadyTargeted, CurrentGameHours, ExpiryHours);
        Markers.Add(Marker);
    }

    return Markers;
}