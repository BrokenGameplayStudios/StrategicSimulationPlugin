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

    UFUNCTION(BlueprintCallable, Category = "Debug")
    void DrawVehicleDebugInfo() const;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    UFUNCTION(BlueprintCallable, CallInEditor)
    void ToggleDebugHUD();

private:
    bool bDebugVisible = true;
};