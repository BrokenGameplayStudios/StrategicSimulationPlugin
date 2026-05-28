#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UVehicleDefinition.h"
#include "UVehicleDatabase.generated.h"

UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UVehicleDatabase : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicles")
    TArray<TSoftObjectPtr<UVehicleDefinition>> AvailableVehicles;
};