#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Math/Vector2D.h" // ensure FVector2D is available
#include "AStrategyDebugHUD.generated.h"

class UStrategyBase;
class UMissionGroup;
class UStrategyVehicle;
class UStrategySiteDefinition;
class UBaseManagerSubsystem;

/** Canvas-based debug HUD for faction stats, strategy map overlays, and site inspection. */
UCLASS()
class STRATEGICSIMULATIONPLUGIN_API AStrategyDebugHUD : public AHUD
{
    GENERATED_BODY()

public:
    /** Enables tick for on-screen debug text updates. */
    AStrategyDebugHUD();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bDebugVisible = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Map")
    bool bShowStrategyMap = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Map")
    float MapScale = 0.85f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Map")
    ::FVector2D MapOffset = ::FVector2D(100.0f, 100.0f);

    /** Returns the current uniform scale factor so icons/lines also scale with the map */
    UFUNCTION(BlueprintCallable, Category = "Debug Map")
    float GetCurrentMapScale() const;

    /** Logical map pixel position → screen position, centered and aspect preserved. */
    FVector2D GetScreenPosition(const FVector2D& LogicalPos) const;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Map|Vehicles")
    bool bShowVehiclePaths = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Map|Radar")
    bool bShowFriendlyRadarContacts = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Map|Radar")
    bool bShowEnemyRadarContacts = true;

    /** Console exec: toggles on-screen faction/resource debug text (gated by campaign debug flag). */
    UFUNCTION(Exec)
    void ToggleDebugHUD();

    /** Console exec: toggles the canvas strategy map overlay (gated by campaign debug flag). */
    UFUNCTION(Exec)
    void ToggleStrategyMap();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Map|Site Info")
    int32 SelectedSiteIndex = -1;

    /** Console exec: selects a site index for the bottom-of-screen site inspector panel. */
    UFUNCTION(Exec)
    void ShowSiteInfo(int32 SiteIndex);

    /** Console exec: clears the selected site inspector index. */
    UFUNCTION(Exec)
    void ClearSiteInfo();

protected:
    /** Toggles debug HUD visibility on level start. */
    virtual void BeginPlay() override;

    /** Builds and displays the on-screen faction/resource summary each frame. */
    virtual void Tick(float DeltaTime) override;

    /** Draws bases, missions, vehicles, sites, radar overlays, legend, and site inspector. */
    virtual void DrawHUD() override;

private:
    /** Draws a scaled base box, name label, and passive radar ring when applicable. */
    void DrawBase(UStrategyBase* Base, FLinearColor Color);

    /** Draws mission type label and faint line from origin base to lead vehicle. */
    void DrawMission(UMissionGroup* Mission);

    /** Iterates AllPotentialSites and draws neutral nodes or salvage wrecks. */
    void DrawAllPotentialSites();

    /** Draws vehicle position, state label, waypoint paths, and onboard radar circle. */
    void DrawVehicle(UStrategyVehicle* Vehicle);

    /** Draws a yellow highlight box around SelectedSiteIndex on the map. */
    void DrawInspectedSiteHighlight();

    /** Appends Command Center facility and force counts to DebugText. */
    void AppendCommandCenterStats(UStrategyBase* Base, FString& DebugText);

    /** Draws a faction-colored triangle and discovery markers for an active salvage wreck. */
    void DrawSalvageSite(UStrategySiteDefinition* Site, int32 SiteIndex, float Scale, const UBaseManagerSubsystem* BaseManager);

    /** Draws gray radar LOS blocker zones from URadarTerrainSubsystem. */
    void DrawRadarBlockerZones();

    /** Draws cyan/magenta diamonds at friendly and enemy radar contact entry points. */
    void DrawRadarContactEntryPoints();

    /** Draws a three-segment outlined triangle at ScreenPos. */
    void DrawSiteTriangle(const FVector2D& ScreenPos, float Size, float LineThickness, const FLinearColor& Color);

    /** Builds a comma-separated facility count string for a base (non-zero types only). */
    static FString BuildFacilityListText(UStrategyBase* Base);

    /** Appends forward bases with CC construction in progress and days remaining. */
    void AppendExpansionConstructionStatus(const UBaseManagerSubsystem* BaseMgr, FString& DebugText);
};