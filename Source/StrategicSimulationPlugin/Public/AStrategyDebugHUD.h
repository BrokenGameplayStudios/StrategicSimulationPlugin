#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Math/Vector2D.h" // ensure FVector2D is available
#include "AStrategyDebugHUD.generated.h"

class UStrategyBase;
class UMissionGroup;
class UStrategyVehicle;   // NEW

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

    // Easy tuning for the visual map
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Map")
    float MapScale = 0.85f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Map")
    ::FVector2D MapOffset = ::FVector2D(100.0f, 100.0f);

    // NEW: Vehicle debug visuals
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Map|Vehicles")
    bool bShowVehiclePaths = true;

    UFUNCTION(Exec)
    void ToggleDebugHUD();

    UFUNCTION(Exec)
    void ToggleStrategyMap();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void DrawHUD() override;

private:
    void DrawBase(UStrategyBase* Base, FLinearColor Color);
    void DrawMission(UMissionGroup* Mission);
    void DrawAllPotentialSites();
    void DrawVehicle(UStrategyVehicle* Vehicle);   // NEW    
    void DrawDiscoveredSites();

    ::FVector2D GetScreenPosition(const ::FVector2D& WorldPos) const;
};