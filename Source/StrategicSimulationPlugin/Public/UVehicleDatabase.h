#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UVehicleDefinition.h"
#include "UVehicleDatabase.generated.h"

/** Master catalog of vehicle hull definitions for hangar production and AI fleet planning. */
UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UVehicleDatabase : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Catalog",
        meta = (ToolTip = "All vehicle hulls buildable at Hangars. Weapon/defense items live in the item database."))
    TArray<TSoftObjectPtr<UVehicleDefinition>> AvailableVehicles;
};