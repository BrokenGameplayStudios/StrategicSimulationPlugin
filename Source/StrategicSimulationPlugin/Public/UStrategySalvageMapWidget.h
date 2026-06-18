#pragma once

#include "CoreMinimal.h"
#include "UStrategyUserWidget.h"
#include "StrategicSimulationTypes.h"
#include "StrategicSiteDefinition.h"
#include "UStrategicSimulationDisplayHelpers.h"
#include "UStrategySalvageMapWidget.generated.h"

class UBaseManagerSubsystem;
class UStrategyCampaignSubsystem;
class UStrategyEventDispatcher;

/** Slate-painted overlay that draws fog-aware salvage wreck markers on the strategic map. */
UCLASS(Blueprintable)
class STRATEGICSIMULATIONPLUGIN_API UStrategySalvageMapWidget : public UStrategyUserWidget
{
    GENERATED_BODY()

public:
    /** Disables Blueprint tick; subclasses use NativeTick for map refresh. */
    UStrategySalvageMapWidget(const FObjectInitializer& ObjectInitializer);

    /** Faction whose fog-of-war rules apply to wreck visibility (default: Human player). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Salvage Map")
    EFactionType ViewerFaction = EFactionType::Human;

    /** Uniform scale multiplier applied on top of aspect-fit map scaling. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Salvage Map", meta = (ClampMin = "0.1", ClampMax = "2.0"))
    float MapScaleMultiplier = 0.85f;

    /** Triangle icon size in widget pixels (before map scale). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Salvage Map", meta = (ClampMin = "4.0", ClampMax = "48.0"))
    float MarkerSize = 10.0f;

    /** Line thickness for wreck triangles. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Salvage Map", meta = (ClampMin = "1.0", ClampMax = "8.0"))
    float MarkerLineThickness = 2.0f;

    /** Radius used for hover hit-testing around each marker center. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Salvage Map", meta = (ClampMin = "4.0", ClampMax = "64.0"))
    float HoverRadius = 14.0f;

    /** Seconds a discovery toast remains visible. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Salvage Map", meta = (ClampMin = "1.0", ClampMax = "30.0"))
    float ToastDurationSeconds = 5.0f;

    /** Returns true when campaign flags enable the player salvage map layer. */
    UFUNCTION(BlueprintPure, Category = "Salvage Map")
    bool IsSalvageLayerEnabled() const;

    /** Returns the last cached marker list built by RefreshSalvageMarkers. */
    UFUNCTION(BlueprintPure, Category = "Salvage Map")
    TArray<FSalvageMapMarker> GetSalvageMarkers() const { return CachedMarkers; }

    /** Returns tooltip text for the marker currently under the cursor. */
    UFUNCTION(BlueprintPure, Category = "Salvage Map")
    FText GetHoveredTooltipText() const { return HoveredTooltip; }

    /** Rebuilds CachedMarkers from visible salvage sites and invalidates paint. */
    UFUNCTION(BlueprintCallable, Category = "Salvage Map")
    void RefreshSalvageMarkers();

protected:
    /** Binds site events, sets hit-test-invisible visibility, and refreshes markers. */
    virtual void NativeConstruct() override;

    /** Unbinds site events before destruction. */
    virtual void NativeDestruct() override;

    /** Prunes toasts, refreshes markers on resize, and updates hover tooltip. */
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    /** Draws wreck triangles, hover tooltip, and the active discovery toast. */
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
    UPROPERTY(Transient)
    TArray<FSalvageMapMarker> CachedMarkers;

    UPROPERTY(Transient)
    FText HoveredTooltip;

    FVector2D LastCachedWidgetSize = FVector2D::ZeroVector;

    struct FPendingToast
    {
        FText Message;
        double ExpiresAt = 0.0;
    };

    TArray<FPendingToast> PendingToasts;

    /** Event handler: queues a toast and refreshes markers when a salvage site is discovered. */
    UFUNCTION()
    void OnSiteDiscovered(EFactionType Faction, UStrategySiteDefinition* Site, EDiscoveryReason Reason);

    /** Event handler: refreshes markers when a new wreck site is created. */
    UFUNCTION()
    void OnSalvageSiteCreated(EFactionType WreckOwnerFaction, const TArray<EFactionType>& KnownFactions, UStrategySiteDefinition* Site);

    /** Event handler: refreshes markers when a wreck is removed from the map. */
    UFUNCTION()
    void OnSalvageSiteRemoved(FGuid SiteId, EFactionType LastSalvagingFaction);

    /** Subscribes to salvage-related UStrategyEventDispatcher delegates. */
    void BindSiteEvents();

    /** Removes salvage-related UStrategyEventDispatcher delegate bindings. */
    void UnbindSiteEvents();

    /** Adds a timed discovery toast when ViewerFaction learns of a new wreck. */
    void QueueDiscoveryToast(EFactionType Faction, UStrategySiteDefinition* Site, EDiscoveryReason Reason);

    /** Sets HoveredTooltip from the marker nearest the cursor within HoverRadius. */
    void UpdateHoveredTooltip(const FGeometry& MyGeometry);

    /** Removes expired toasts and invalidates paint when the queue shrinks. */
    void PruneExpiredToasts(double Now);

    /** Draws an outlined triangle marker at Center using Slate line elements. */
    void DrawSalvageTriangle(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32& LayerId,
        const FVector2D& Center, float Size, float LineThickness, const FLinearColor& Color) const;

    /** Draws a small tooltip panel anchored near the cursor. */
    void DrawTooltipPanel(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32& LayerId,
        const FVector2D& Anchor, const FText& Text) const;

    /** Draws the topmost active discovery toast centered near the widget top. */
    void DrawToastPanel(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32& LayerId,
        const FText& Text) const;

    /** Returns UBaseManagerSubsystem from the widget's game instance. */
    UBaseManagerSubsystem* GetBaseManager() const;

    /** Returns UStrategyCampaignSubsystem from the widget's game instance. */
    UStrategyCampaignSubsystem* GetCampaign() const;

    /** Returns UStrategyEventDispatcher from the widget's game instance. */
    UStrategyEventDispatcher* GetEventDispatcher() const;
};