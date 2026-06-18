#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "StrategicSimulationTypes.h"
#include "StrategicSiteDefinition.h"
#include "UStrategicSimulationDisplayHelpers.generated.h"

class UBaseManagerSubsystem;

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
};