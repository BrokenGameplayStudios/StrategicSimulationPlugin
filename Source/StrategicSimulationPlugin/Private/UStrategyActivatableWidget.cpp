#include "UStrategyActivatableWidget.h"
#include "Engine/Engine.h"

/** Called when this widget becomes the active input consumer in the UI stack. */
void UStrategyActivatableWidget::NativeOnActivated()
{
    Super::NativeOnActivated();
    // UE_LOG(LogTemp, Display, TEXT("Activatable Widget Activated: %s"), *GetName());
}

/** Called when this widget is deactivated and relinquishes input focus. */
void UStrategyActivatableWidget::NativeOnDeactivated()
{
    Super::NativeOnDeactivated();
    // UE_LOG(LogTemp, Display, TEXT("Activatable Widget Deactivated: %s"), *GetName());
}