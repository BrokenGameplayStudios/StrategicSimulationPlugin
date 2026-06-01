#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AStrategyDebugHUD.generated.h"

UCLASS()
class STRATEGICSIMULATIONPLUGIN_API AStrategyDebugHUD : public AActor
{
    GENERATED_BODY()

public:
    AStrategyDebugHUD();

    UFUNCTION(BlueprintCallable, CallInEditor)
    void ToggleDebugHUD();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

private:
    bool bDebugVisible = true;
};