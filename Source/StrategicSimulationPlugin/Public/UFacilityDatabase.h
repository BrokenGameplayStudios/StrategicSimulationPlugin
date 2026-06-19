#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UFacilityDefinition.h"
#include "UFacilityDatabase.generated.h"

/** Master catalog of facility definitions for base construction and AI planning. */
UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UFacilityDatabase : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Catalog",
        meta = (ToolTip = "Every facility type players or AI can build. Prerequisites and unlocks are defined on each facility asset."))
    TArray<TSoftObjectPtr<UFacilityDefinition>> AvailableFacilities;
};