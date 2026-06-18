#pragma once

#include "CoreMinimal.h"
#include "UStrategyUserWidget.h"
#include "StrategicSimulationTypes.h"
#include "UStrategicSimulationDisplayHelpers.h"
#include "UStrategyRadarContactMapWidget.generated.h"

class UBaseManagerSubsystem;
class UMissionManagerSubsystem;
class UStrategyCampaignSubsystem;
class UStrategyEventDispatcher;

UCLASS(Blueprintable)
class STRATEGICSIMULATIONPLUGIN_API UStrategyRadarContactMapWidget : public UStrategyUserWidget
{
    GENERATED_BODY()

public:
    UStrategyRadarContactMapWidget(const FObjectInitializer& ObjectInitializer);

    /** Faction whose passive radar contacts are shown (default: Human player). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radar Contact Map")
    EFactionType ViewerFaction = EFactionType::Human;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radar Contact Map", meta = (ClampMin = "0.1", ClampMax = "2.0"))
    float MapScaleMultiplier = 0.85f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radar Contact Map", meta = (ClampMin = "4.0", ClampMax = "48.0"))
    float MarkerSize = 12.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radar Contact Map", meta = (ClampMin = "1.0", ClampMax = "8.0"))
    float MarkerLineThickness = 2.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radar Contact Map", meta = (ClampMin = "4.0", ClampMax = "64.0"))
    float HoverRadius = 16.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radar Contact Map", meta = (ClampMin = "1.0", ClampMax = "30.0"))
    float ToastDurationSeconds = 6.0f;

    UFUNCTION(BlueprintPure, Category = "Radar Contact Map")
    bool IsRadarContactLayerEnabled() const;

    UFUNCTION(BlueprintPure, Category = "Radar Contact Map")
    TArray<FRadarContactMapMarker> GetRadarContactMarkers() const { return CachedMarkers; }

    UFUNCTION(BlueprintPure, Category = "Radar Contact Map")
    FText GetHoveredTooltipText() const { return HoveredTooltip; }

    UFUNCTION(BlueprintCallable, Category = "Radar Contact Map")
    void RefreshRadarContactMarkers();

    /** Player click path: auto-pick nearest idle gunship and launch interception. */
    UFUNCTION(BlueprintCallable, Category = "Radar Contact Map")
    bool TryInterceptContactAtWidgetPosition(FVector2D LocalWidgetPosition);

    UFUNCTION(BlueprintCallable, Category = "Radar Contact Map")
    bool TryInterceptContactById(FGuid ContactId);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
    UPROPERTY(Transient)
    TArray<FRadarContactMapMarker> CachedMarkers;

    UPROPERTY(Transient)
    FText HoveredTooltip;

    FVector2D LastCachedWidgetSize = FVector2D::ZeroVector;

    struct FPendingToast
    {
        FText Message;
        double ExpiresAt = 0.0;
    };

    TArray<FPendingToast> PendingToasts;
    TSet<FGuid> SeenContactIds;

    UFUNCTION()
    void OnRadarContactUpdated(EFactionType Faction, FRadarContact Contact);

    UFUNCTION()
    void OnRadarContactExpired(EFactionType Faction, FRadarContact Contact);

    void BindRadarEvents();
    void UnbindRadarEvents();
    void QueueContactToast(const FRadarContact& Contact);
    void UpdateHoveredTooltip(const FGeometry& MyGeometry);
    void PruneExpiredToasts(double Now);
    const FRadarContactMapMarker* FindMarkerAtPosition(const FVector2D& LocalPosition) const;
    void DrawContactDiamond(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32& LayerId,
        const FVector2D& Center, float Size, float LineThickness, const FLinearColor& Color) const;
    void DrawTooltipPanel(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32& LayerId,
        const FVector2D& Anchor, const FText& Text) const;
    void DrawToastPanel(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32& LayerId,
        const FText& Text) const;

    UMissionManagerSubsystem* GetMissionManager() const;
    UStrategyCampaignSubsystem* GetCampaign() const;
    UStrategyEventDispatcher* GetEventDispatcher() const;
};