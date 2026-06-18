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

UCLASS(Blueprintable)
class STRATEGICSIMULATIONPLUGIN_API UStrategySalvageMapWidget : public UStrategyUserWidget
{
    GENERATED_BODY()

public:
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

    UFUNCTION(BlueprintPure, Category = "Salvage Map")
    bool IsSalvageLayerEnabled() const;

    UFUNCTION(BlueprintPure, Category = "Salvage Map")
    TArray<FSalvageMapMarker> GetSalvageMarkers() const { return CachedMarkers; }

    UFUNCTION(BlueprintPure, Category = "Salvage Map")
    FText GetHoveredTooltipText() const { return HoveredTooltip; }

    UFUNCTION(BlueprintCallable, Category = "Salvage Map")
    void RefreshSalvageMarkers();

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
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

    UFUNCTION()
    void OnSiteDiscovered(EFactionType Faction, UStrategySiteDefinition* Site, EDiscoveryReason Reason);

    UFUNCTION()
    void OnSalvageSiteCreated(EFactionType WreckOwnerFaction, const TArray<EFactionType>& KnownFactions, UStrategySiteDefinition* Site);

    UFUNCTION()
    void OnSalvageSiteRemoved(FGuid SiteId, EFactionType LastSalvagingFaction);

    void BindSiteEvents();
    void UnbindSiteEvents();
    void QueueDiscoveryToast(EFactionType Faction, UStrategySiteDefinition* Site, EDiscoveryReason Reason);
    void UpdateHoveredTooltip(const FGeometry& MyGeometry);
    void PruneExpiredToasts(double Now);
    void DrawSalvageTriangle(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32& LayerId,
        const FVector2D& Center, float Size, float LineThickness, const FLinearColor& Color) const;
    void DrawTooltipPanel(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32& LayerId,
        const FVector2D& Anchor, const FText& Text) const;
    void DrawToastPanel(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32& LayerId,
        const FText& Text) const;

    UBaseManagerSubsystem* GetBaseManager() const;
    UStrategyCampaignSubsystem* GetCampaign() const;
    UStrategyEventDispatcher* GetEventDispatcher() const;
};