#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "UStrategyActivatableWidget.generated.h"

/** Base Common UI activatable widget for modal strategic simulation panels. */
UCLASS()
class STRATEGICSIMULATIONPLUGIN_API UStrategyActivatableWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    /** Called when this widget becomes the active input consumer in the UI stack. */
    virtual void NativeOnActivated() override;

    /** Called when this widget is deactivated and relinquishes input focus. */
    virtual void NativeOnDeactivated() override;
};