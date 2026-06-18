#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Math/Vector2D.h" // ensure FVector2D is available
#include "AStrategyDebugHUD.generated.h"

class UStrategyBase;
class UMissionGroup;
class UStrategyVehicle;
class UStrategySiteDefinition;

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

    /** Returns the current uniform scale factor so icons/lines also scale with the map */
    UFUNCTION(BlueprintCallable, Category = "Debug Map")
    float GetCurrentMapScale() const;

    /** Logical 1920x1080 pixel map → fits any Canvas size, centered, aspect preserved */
    FVector2D GetScreenPosition(const FVector2D& LogicalPos) const;

    // NEW: Vehicle debug visuals
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Map|Vehicles")
    bool bShowVehiclePaths = true;

    UFUNCTION(Exec)
    void ToggleDebugHUD();

    UFUNCTION(Exec)
    void ToggleStrategyMap();

    // === Site Inspector ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Map|Site Info")
    int32 SelectedSiteIndex = -1;

    UFUNCTION(Exec)
    void ShowSiteInfo(int32 SiteIndex);

    UFUNCTION(Exec)
    void ClearSiteInfo();

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
    void DrawInspectedSiteHighlight();
    void AppendCommandCenterStats(UStrategyBase* Base, FString& DebugText);
    void DrawSalvageSite(UStrategySiteDefinition* Site, int32 SiteIndex, float Scale);
    void DrawSiteTriangle(const FVector2D& ScreenPos, float Size, float LineThickness, const FLinearColor& Color);
    static FString BuildFacilityListText(UStrategyBase* Base);
};