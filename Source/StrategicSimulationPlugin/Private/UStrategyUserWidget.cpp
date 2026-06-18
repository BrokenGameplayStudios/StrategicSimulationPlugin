#include "UStrategyUserWidget.h"
#include "Engine/Engine.h"

/** Calls Super::NativeConstruct; override in subclasses for data binding and event hooks. */
void UStrategyUserWidget::NativeConstruct()
{
    Super::NativeConstruct();
    // UE_LOG(LogTemp, Display, TEXT("UStrategyUserWidget constructed: %s"), *GetName());
}