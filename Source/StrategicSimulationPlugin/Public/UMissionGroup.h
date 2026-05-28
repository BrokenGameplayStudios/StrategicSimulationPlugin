#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UMissionGroup.generated.h"

class UStrategyVehicle;
class UStrategyBase;

UENUM(BlueprintType)
enum class EMissionStatus : uint8
{
    InProgress,
    Returning,
    Completed,
    Failed
};

UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UMissionGroup : public UObject
{
    GENERATED_BODY()

public:
    UMissionGroup();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission")
    TArray<UStrategyVehicle*> VehiclesInFleet;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission")
    UStrategyBase* OriginBase;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission")
    int32 StartDay;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission")
    int32 DurationDays;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission")
    EMissionStatus Status;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission")
    FText MissionName;
};