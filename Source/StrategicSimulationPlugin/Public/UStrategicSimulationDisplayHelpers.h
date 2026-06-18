#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "StrategicSimulationTypes.h"
#include "UStrategicSimulationDisplayHelpers.generated.h"

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
};