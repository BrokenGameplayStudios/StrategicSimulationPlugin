#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "StrategicSimulationTypes.h"
#include "UResearchTechDefinition.h"
#include "UActiveResearchProject.generated.h"

UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UActiveResearchProject : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Research")
    UResearchTechDefinition* ResearchDefinition;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Progress")
    int32 RemainingDays = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Status")
    bool bIsCompleted = false;
};