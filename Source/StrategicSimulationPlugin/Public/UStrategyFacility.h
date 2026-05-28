#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "StrategicSimulationTypes.h"
#include "UFacilityDefinition.h"
#include "UStrategyFacility.generated.h"

class UStrategyVehicle;
class UStrategyBase;

UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UStrategyFacility : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Facility")
    UFacilityDefinition* FacilityDefinition;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Build")
    int32 BuildProgressDays = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Status")
    bool bIsOperational = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Status")
    int32 CurrentPowerDraw = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hanger")
    TArray<class UStrategyVehicle*> ParkedVehicles;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Repair")
    TArray<class UStrategyVehicle*> VehiclesInRepair;

    /** Simulate one day of repairs for all vehicles in this bay */
    UFUNCTION(BlueprintCallable, Category = "Repair")
    void SimulateDailyRepair(UStrategyBase* OwningBase);
};