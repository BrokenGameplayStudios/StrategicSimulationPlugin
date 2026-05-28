#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "StrategicSimulationTypes.h"
#include "UFacilityDefinition.h"
#include "UStrategyFacility.generated.h"

UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UStrategyFacility : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Facility")
    UFacilityDefinition* FacilityDefinition;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Build")
    int32 BuildProgressDays = 0;     // 0 = completed

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Status")
    bool bIsOperational = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Status")
    int32 CurrentPowerDraw = 0;

    /** Vehicles currently parked in this hanger (only used if FacilityType == Hanger) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hanger")
    TArray<class UStrategyVehicle*> ParkedVehicles;
};