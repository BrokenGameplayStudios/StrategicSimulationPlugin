#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UVehicleDefinition.h"
#include "UVehicleDatabase.generated.h"

/** Registry of vehicle definitions available for production and AI fleet planning. */
UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UVehicleDatabase : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    /** Soft references to all vehicle types that can be built at hangars. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicles")
    TArray<TSoftObjectPtr<UVehicleDefinition>> AvailableVehicles;
};