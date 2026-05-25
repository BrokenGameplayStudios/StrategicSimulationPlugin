#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "UStrategyActivatableWidget.generated.h"

UCLASS()
class STRATEGICSIMULATIONPLUGIN_API UStrategyActivatableWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    // Override to control input when activated
    virtual void NativeOnActivated() override;
    virtual void NativeOnDeactivated() override;
};