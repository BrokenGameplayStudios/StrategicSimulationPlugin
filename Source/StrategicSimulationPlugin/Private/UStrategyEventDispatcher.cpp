#include "UStrategyEventDispatcher.h"
#include "Engine/Engine.h"

/** Logs that the event dispatcher is ready for UI subscriptions. */
void UStrategyEventDispatcher::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogTemp, Display, TEXT("UStrategyEventDispatcher initialized — UI events ready (soldiers, production, sites/salvage)"));
}