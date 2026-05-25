#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "StrategicSimulationTypes.h"
#include "UFacilityDefinition.generated.h"

// Forward declaration only
class UResearchTechDefinition;

UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UFacilityDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facility")
    FText FacilityName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facility")
    EFacilityType FacilityType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cost")
    FResourceStockpile BuildCost;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build")
    int32 BuildTimeDays = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Power")
    int32 PowerProvided = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Power")
    int32 PowerDraw = 0;

    // Facility ONLY unlocks Research Projects (as you specified)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unlocks")
    TArray<TSoftObjectPtr<UResearchTechDefinition>> UnlocksResearch;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capacity")
    int32 Capacity = 0;
};