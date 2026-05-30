#include "UStrategyEventDispatcher.h"
#include "Engine/Engine.h"

void UStrategyEventDispatcher::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogTemp, Display, TEXT("UStrategyEventDispatcher initialized — ALL UI events ready (full list + loadout)"));
}