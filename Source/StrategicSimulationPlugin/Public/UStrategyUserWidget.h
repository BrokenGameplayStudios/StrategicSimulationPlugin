#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "UStrategyUserWidget.generated.h"

/** Base Common UI widget for all strategic simulation HUD screens. */
UCLASS()
class STRATEGICSIMULATIONPLUGIN_API UStrategyUserWidget : public UCommonUserWidget
{
    GENERATED_BODY()

public:
    /** Calls Super::NativeConstruct; override in subclasses for data binding and event hooks. */
    virtual void NativeConstruct() override;
};