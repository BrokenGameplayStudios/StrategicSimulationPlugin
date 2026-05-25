#include "UStrategyUserWidget.h"
#include "Engine/Engine.h"

void UStrategyUserWidget::NativeConstruct()
{
    Super::NativeConstruct();
    UE_LOG(LogTemp, Display, TEXT("UStrategyUserWidget constructed: %s"), *GetName());
}