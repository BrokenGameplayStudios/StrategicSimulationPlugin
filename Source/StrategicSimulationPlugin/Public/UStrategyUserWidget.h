#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "UStrategyUserWidget.generated.h"

UCLASS()
class STRATEGICSIMULATIONPLUGIN_API UStrategyUserWidget : public UCommonUserWidget
{
    GENERATED_BODY()

public:
    // You can add common C++ helper functions here later (e.g. data binding, styling)
    virtual void NativeConstruct() override;
};