#include "UStrategySalvageMapWidget.h"
#include "UStrategyEventDispatcher.h"
#include "UStrategyCampaignSubsystem.h"
#include "UBaseManagerSubsystem.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Engine/GameInstance.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

/** Disables Blueprint tick; subclasses use NativeTick for map refresh. */
UStrategySalvageMapWidget::UStrategySalvageMapWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    bHasScriptImplementedTick = false;
}

/** Returns true when campaign flags enable the player salvage map layer. */
bool UStrategySalvageMapWidget::IsSalvageLayerEnabled() const
{
    return UStrategicSimulationDisplayHelpers::IsPlayerSalvageMapLayerEnabled(GetCampaign());
}

/** Binds site events, sets hit-test-invisible visibility, and refreshes markers. */
void UStrategySalvageMapWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetVisibility(ESlateVisibility::HitTestInvisible);
    BindSiteEvents();
    RefreshSalvageMarkers();
}

/** Unbinds site events before destruction. */
void UStrategySalvageMapWidget::NativeDestruct()
{
    UnbindSiteEvents();
    Super::NativeDestruct();
}

/** Prunes toasts, refreshes markers on resize, and updates hover tooltip. */
void UStrategySalvageMapWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (UWorld* World = GetWorld())
    {
        PruneExpiredToasts(World->GetTimeSeconds());
    }

    const FVector2D CurrentSize = MyGeometry.GetLocalSize();
    if (!CurrentSize.Equals(LastCachedWidgetSize, 1.0f))
    {
        LastCachedWidgetSize = CurrentSize;
        RefreshSalvageMarkers();
    }

    UpdateHoveredTooltip(MyGeometry);
}

/** Draws wreck triangles, hover tooltip, and the active discovery toast. */
int32 UStrategySalvageMapWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
    const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    int32 MaxLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

    if (!IsSalvageLayerEnabled())
    {
        return MaxLayer;
    }

    const float ScaledMarkerSize = MarkerSize * UStrategicSimulationDisplayHelpers::GetMapUniformScale(
        AllottedGeometry.GetLocalSize(), GetCampaign()) * MapScaleMultiplier;

    for (const FSalvageMapMarker& Marker : CachedMarkers)
    {
        DrawSalvageTriangle(AllottedGeometry, OutDrawElements, MaxLayer, Marker.WidgetPosition,
            ScaledMarkerSize, MarkerLineThickness, Marker.Color);
    }

    if (!HoveredTooltip.IsEmpty())
    {
        FVector2D MousePos = FSlateApplication::Get().GetCursorPos();
        const FVector2D LocalMouse = AllottedGeometry.AbsoluteToLocal(MousePos);
        DrawTooltipPanel(AllottedGeometry, OutDrawElements, MaxLayer, LocalMouse + FVector2D(16.0f, 16.0f), HoveredTooltip);
    }

    if (UWorld* World = GetWorld())
    {
        const double Now = World->GetTimeSeconds();
        for (const FPendingToast& Toast : PendingToasts)
        {
            if (Toast.ExpiresAt > Now)
            {
                DrawToastPanel(AllottedGeometry, OutDrawElements, MaxLayer, Toast.Message);
                break;
            }
        }
    }

    return MaxLayer;
}

/** Rebuilds CachedMarkers from visible salvage sites and invalidates paint. */
void UStrategySalvageMapWidget::RefreshSalvageMarkers()
{
    const FVector2D WidgetSize = GetCachedGeometry().GetLocalSize();
    CachedMarkers = UStrategicSimulationDisplayHelpers::BuildSalvageMapMarkers(
        this, ViewerFaction, WidgetSize, MapScaleMultiplier);
    Invalidate(EInvalidateWidgetReason::Paint);
}

/** Subscribes to salvage-related UStrategyEventDispatcher delegates. */
void UStrategySalvageMapWidget::BindSiteEvents()
{
    if (UStrategyEventDispatcher* EventDisp = GetEventDispatcher())
    {
        EventDisp->OnSiteDiscovered.AddDynamic(this, &UStrategySalvageMapWidget::OnSiteDiscovered);
        EventDisp->OnSalvageSiteCreated.AddDynamic(this, &UStrategySalvageMapWidget::OnSalvageSiteCreated);
        EventDisp->OnSalvageSiteRemoved.AddDynamic(this, &UStrategySalvageMapWidget::OnSalvageSiteRemoved);
    }
}

/** Removes salvage-related UStrategyEventDispatcher delegate bindings. */
void UStrategySalvageMapWidget::UnbindSiteEvents()
{
    if (UStrategyEventDispatcher* EventDisp = GetEventDispatcher())
    {
        EventDisp->OnSiteDiscovered.RemoveDynamic(this, &UStrategySalvageMapWidget::OnSiteDiscovered);
        EventDisp->OnSalvageSiteCreated.RemoveDynamic(this, &UStrategySalvageMapWidget::OnSalvageSiteCreated);
        EventDisp->OnSalvageSiteRemoved.RemoveDynamic(this, &UStrategySalvageMapWidget::OnSalvageSiteRemoved);
    }
}

/** Event handler: queues a toast and refreshes markers when a salvage site is discovered. */
void UStrategySalvageMapWidget::OnSiteDiscovered(EFactionType Faction, UStrategySiteDefinition* Site, EDiscoveryReason Reason)
{
    if (!Site || Site->SiteType != EStrategySiteType::SalvageSite)
    {
        return;
    }

    QueueDiscoveryToast(Faction, Site, Reason);
    RefreshSalvageMarkers();
}

/** Event handler: refreshes markers when a new wreck site is created. */
void UStrategySalvageMapWidget::OnSalvageSiteCreated(EFactionType WreckOwnerFaction,
    const TArray<EFactionType>& KnownFactions, UStrategySiteDefinition* Site)
{
    (void)WreckOwnerFaction;
    (void)KnownFactions;
    (void)Site;
    RefreshSalvageMarkers();
}

/** Event handler: refreshes markers when a wreck is removed from the map. */
void UStrategySalvageMapWidget::OnSalvageSiteRemoved(FGuid SiteId, EFactionType LastSalvagingFaction)
{
    (void)SiteId;
    (void)LastSalvagingFaction;
    RefreshSalvageMarkers();
}

/** Adds a timed discovery toast when ViewerFaction learns of a new wreck. */
void UStrategySalvageMapWidget::QueueDiscoveryToast(EFactionType Faction, UStrategySiteDefinition* Site, EDiscoveryReason Reason)
{
    if (Faction != ViewerFaction || !IsSalvageLayerEnabled() || !Site)
    {
        return;
    }

    if (!UStrategicSimulationDisplayHelpers::ShouldShowSalvageToFaction(Site, ViewerFaction, GetBaseManager()))
    {
        return;
    }

    const FText ToastText = UStrategicSimulationDisplayHelpers::FormatSalvageDiscoveryToast(Faction, Site, Reason);
    if (ToastText.IsEmpty())
    {
        return;
    }

    const double ExpiresAt = GetWorld() ? GetWorld()->GetTimeSeconds() + ToastDurationSeconds : 0.0;
    PendingToasts.Add({ ToastText, ExpiresAt });
    Invalidate(EInvalidateWidgetReason::Paint);

    UE_LOG(LogTemp, Display, TEXT("[SALVAGE MAP] %s"), *ToastText.ToString());
}

/** Sets HoveredTooltip from the marker nearest the cursor within HoverRadius. */
void UStrategySalvageMapWidget::UpdateHoveredTooltip(const FGeometry& MyGeometry)
{
    HoveredTooltip = FText::GetEmpty();

    if (!IsSalvageLayerEnabled())
    {
        return;
    }

    FVector2D MousePos = FSlateApplication::Get().GetCursorPos();
    const FVector2D LocalMouse = MyGeometry.AbsoluteToLocal(MousePos);
    const float HitRadiusSq = HoverRadius * HoverRadius;

    for (const FSalvageMapMarker& Marker : CachedMarkers)
    {
        if (FVector2D::DistSquared(Marker.WidgetPosition, LocalMouse) <= HitRadiusSq)
        {
            HoveredTooltip = Marker.Tooltip;
            break;
        }
    }
}

/** Removes expired toasts and invalidates paint when the queue shrinks. */
void UStrategySalvageMapWidget::PruneExpiredToasts(double Now)
{
    const int32 Before = PendingToasts.Num();
    PendingToasts.RemoveAll([Now](const FPendingToast& Toast)
    {
        return Toast.ExpiresAt <= Now;
    });

    if (Before != PendingToasts.Num())
    {
        Invalidate(EInvalidateWidgetReason::Paint);
    }
}

/** Draws an outlined triangle marker at Center using Slate line elements. */
void UStrategySalvageMapWidget::DrawSalvageTriangle(const FGeometry& AllottedGeometry,
    FSlateWindowElementList& OutDrawElements, int32& LayerId, const FVector2D& Center, float Size,
    float LineThickness, const FLinearColor& Color) const
{
    const FVector2D Top(Center.X, Center.Y - Size * 0.5f);
    const FVector2D BottomLeft(Center.X - Size * 0.5f, Center.Y + Size * 0.5f);
    const FVector2D BottomRight(Center.X + Size * 0.5f, Center.Y + Size * 0.5f);

    TArray<FVector2D> Points;
    Points.Add(Top);
    Points.Add(BottomLeft);
    Points.Add(BottomRight);
    Points.Add(Top);

    FSlateDrawElement::MakeLines(
        OutDrawElements,
        LayerId++,
        AllottedGeometry.ToPaintGeometry(),
        Points,
        ESlateDrawEffect::None,
        Color,
        true,
        LineThickness);
}

/** Draws a small tooltip panel anchored near the cursor. */
void UStrategySalvageMapWidget::DrawTooltipPanel(const FGeometry& AllottedGeometry,
    FSlateWindowElementList& OutDrawElements, int32& LayerId, const FVector2D& Anchor, const FText& Text) const
{
    const FSlateFontInfo FontInfo = FCoreStyle::GetDefaultFontStyle("Regular", 10);
    const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
    const FVector2D TextSize = FontMeasure->Measure(Text, FontInfo);

    const FVector2D TextPadding(8.0f, 6.0f);
    const FVector2D BoxSize = TextSize + TextPadding * 2.0f;
    const FVector2D BoxPos = Anchor;

    FSlateDrawElement::MakeBox(
        OutDrawElements,
        LayerId++,
        AllottedGeometry.ToPaintGeometry(BoxSize, FSlateLayoutTransform(BoxPos)),
        FCoreStyle::Get().GetBrush("ToolTip.BrightBackground"),
        ESlateDrawEffect::None,
        FLinearColor(0.02f, 0.02f, 0.05f, 0.92f));

    FSlateDrawElement::MakeText(
        OutDrawElements,
        LayerId++,
        AllottedGeometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(BoxPos + TextPadding)),
        Text,
        FontInfo,
        ESlateDrawEffect::None,
        FLinearColor::White);
}

/** Draws the topmost active discovery toast centered near the widget top. */
void UStrategySalvageMapWidget::DrawToastPanel(const FGeometry& AllottedGeometry,
    FSlateWindowElementList& OutDrawElements, int32& LayerId, const FText& Text) const
{
    const FSlateFontInfo FontInfo = FCoreStyle::GetDefaultFontStyle("Bold", 12);
    const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
    const FVector2D TextSize = FontMeasure->Measure(Text, FontInfo);

    const FVector2D TextPadding(12.0f, 8.0f);
    const FVector2D BoxSize = TextSize + TextPadding * 2.0f;
    const FVector2D BoxPos(
        (AllottedGeometry.GetLocalSize().X - BoxSize.X) * 0.5f,
        24.0f);

    FSlateDrawElement::MakeBox(
        OutDrawElements,
        LayerId++,
        AllottedGeometry.ToPaintGeometry(BoxSize, FSlateLayoutTransform(BoxPos)),
        FCoreStyle::Get().GetBrush("NotificationList_ItemBackground"),
        ESlateDrawEffect::None,
        FLinearColor(0.05f, 0.12f, 0.22f, 0.95f));

    FSlateDrawElement::MakeText(
        OutDrawElements,
        LayerId++,
        AllottedGeometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(BoxPos + TextPadding)),
        Text,
        FontInfo,
        ESlateDrawEffect::None,
        FLinearColor(0.9f, 0.95f, 1.0f, 1.0f));
}

/** Returns UBaseManagerSubsystem from the widget's game instance. */
UBaseManagerSubsystem* UStrategySalvageMapWidget::GetBaseManager() const
{
    if (UGameInstance* GI = GetGameInstance())
    {
        return GI->GetSubsystem<UBaseManagerSubsystem>();
    }
    return nullptr;
}

/** Returns UStrategyCampaignSubsystem from the widget's game instance. */
UStrategyCampaignSubsystem* UStrategySalvageMapWidget::GetCampaign() const
{
    if (UGameInstance* GI = GetGameInstance())
    {
        return GI->GetSubsystem<UStrategyCampaignSubsystem>();
    }
    return nullptr;
}

/** Returns UStrategyEventDispatcher from the widget's game instance. */
UStrategyEventDispatcher* UStrategySalvageMapWidget::GetEventDispatcher() const
{
    if (UGameInstance* GI = GetGameInstance())
    {
        return GI->GetSubsystem<UStrategyEventDispatcher>();
    }
    return nullptr;
}