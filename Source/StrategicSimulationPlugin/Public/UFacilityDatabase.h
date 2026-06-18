#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UFacilityDefinition.h"
#include "UFacilityDatabase.generated.h"

/** Registry of facility definitions available for base construction and AI planning. */
UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UFacilityDatabase : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    /** Soft references to all facility types that can be built at bases. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facilities")
    TArray<TSoftObjectPtr<UFacilityDefinition>> AvailableFacilities;
};