#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "StrategicSimulationTypes.h"
#include "UVehicleDefinition.generated.h"

UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UVehicleDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
    FText VehicleName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
    EVehicleType VehicleType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
    int32 SoldierCapacity = 8;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
    int32 MaxMissionDurationDays = 30;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
    int32 AttackPower = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
    FResourceStockpile BuildCost;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
    int32 ProductionDays = 8;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
    FText Description;
};