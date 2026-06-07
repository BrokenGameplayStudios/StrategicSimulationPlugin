#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "AStrategyDebugHUD.generated.h"

class UStrategyBase;
class UMissionGroup;

UCLASS()
class STRATEGICSIMULATIONPLUGIN_API AStrategyDebugHUD : public AHUD
{
    GENERATED_BODY()

public:
    AStrategyDebugHUD();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bDebugVisible = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Map")
    bool bShowStrategyMap = false;

    /** Toggle the text debug panel */
    UFUNCTION(Exec)
    void ToggleDebugHUD();

    /** Toggle the graphical strategy map */
    UFUNCTION(Exec)
    void ToggleStrategyMap();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void DrawHUD() override;

private:
    void DrawBase(UStrategyBase* Base, FLinearColor Color);
    void DrawMission(UMissionGroup* Mission);
    FVector2D GetScreenPosition(const FVector2D& WorldPos) const;
};