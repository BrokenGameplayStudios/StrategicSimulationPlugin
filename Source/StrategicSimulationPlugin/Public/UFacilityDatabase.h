#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UFacilityDefinition.h"
#include "UFacilityDatabase.generated.h"

UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UFacilityDatabase : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facilities")
    TArray<TSoftObjectPtr<UFacilityDefinition>> AvailableFacilities;
};