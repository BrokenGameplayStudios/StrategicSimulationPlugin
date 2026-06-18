#include "UStrategicSimulationDisplayHelpers.h"

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