#include "UStrategicSimulationDisplayHelpers.h"
#include "StrategicSiteDefinition.h"
#include "UBaseManagerSubsystem.h"

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