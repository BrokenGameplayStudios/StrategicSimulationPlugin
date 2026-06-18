#include "UMissionGroup.h"
#include "UStrategyVehicle.h"
#include "UStrategyBase.h"

/** Sets InProgress status and a default mission display name. */
UMissionGroup::UMissionGroup()
{
    Status = EMissionStatus::InProgress;
    MissionName = FText::FromString("New Mission");
}