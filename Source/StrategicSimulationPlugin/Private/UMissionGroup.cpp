#include "UMissionGroup.h"
#include "UStrategyVehicle.h"
#include "UStrategyBase.h"

UMissionGroup::UMissionGroup()
{
    Status = EMissionStatus::InProgress;
    MissionName = FText::FromString("New Mission");
}