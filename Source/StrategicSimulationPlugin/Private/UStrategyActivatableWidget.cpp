#include "UStrategyActivatableWidget.h"
#include "Engine/Engine.h"

void UStrategyActivatableWidget::NativeOnActivated()
{
    Super::NativeOnActivated();
    UE_LOG(LogTemp, Display, TEXT("Activatable Widget Activated: %s"), *GetName());
}

void UStrategyActivatableWidget::NativeOnDeactivated()
{
    Super::NativeOnDeactivated();
    UE_LOG(LogTemp, Display, TEXT("Activatable Widget Deactivated: %s"), *GetName());
}