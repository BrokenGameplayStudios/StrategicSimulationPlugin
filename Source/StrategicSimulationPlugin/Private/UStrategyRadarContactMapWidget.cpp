#include "UStrategyRadarContactMapWidget.h"
#include "UMissionManagerSubsystem.h"
#include "UStrategyBase.h"
#include "UStrategyVehicle.h"
#include "UStrategyEventDispatcher.h"
#include "UStrategyCampaignSubsystem.h"
#include "Engine/GameInstance.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

UStrategyRadarContactMapWidget::UStrategyRadarContactMapWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    bHasScriptImplementedTick = false;
}

bool UStrategyRadarContactMapWidget::IsRadarContactLayerEnabled() const
{
    return UStrategicSimulationDisplayHelpers::IsPlayerRadarContactLayerEnabled(GetCampaign());
}

void UStrategyRadarContactMapWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetVisibility(ESlateVisibility::Visible);
    BindRadarEvents();
    RefreshRadarContactMarkers();
}

void UStrategyRadarContactMapWidget::NativeDestruct()
{
    UnbindRadarEvents();
    Super::NativeDestruct();
}

void UStrategyRadarContactMapWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
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
        RefreshRadarContactMarkers();
    }

    UpdateHoveredTooltip(MyGeometry);
}

int32 UStrategyRadarContactMapWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
    const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    int32 MaxLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

    if (!IsRadarContactLayerEnabled())
    {
        return MaxLayer;
    }

    const float ScaledMarkerSize = MarkerSize * UStrategicSimulationDisplayHelpers::GetMapUniformScale(
        AllottedGeometry.GetLocalSize(), GetCampaign()) * MapScaleMultiplier;

    for (const FRadarContactMapMarker& Marker : CachedMarkers)
    {
        DrawContactDiamond(AllottedGeometry, OutDrawElements, MaxLayer, Marker.WidgetPosition,
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

FReply UStrategyRadarContactMapWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && IsRadarContactLayerEnabled())
    {
        const FVector2D LocalPos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
        if (TryInterceptContactAtWidgetPosition(LocalPos))
        {
            return FReply::Handled();
        }
    }

    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UStrategyRadarContactMapWidget::RefreshRadarContactMarkers()
{
    const FVector2D WidgetSize = GetCachedGeometry().GetLocalSize();
    CachedMarkers = UStrategicSimulationDisplayHelpers::BuildRadarContactMapMarkers(
        this, ViewerFaction, WidgetSize, MapScaleMultiplier);
    Invalidate(EInvalidateWidgetReason::Paint);
}

bool UStrategyRadarContactMapWidget::TryInterceptContactAtWidgetPosition(FVector2D LocalWidgetPosition)
{
    const FRadarContactMapMarker* Marker = FindMarkerAtPosition(LocalWidgetPosition);
    if (!Marker)
    {
        return false;
    }

    return TryInterceptContactById(Marker->ContactId);
}

bool UStrategyRadarContactMapWidget::TryInterceptContactById(FGuid ContactId)
{
    UMissionManagerSubsystem* MissionMgr = GetMissionManager();
    if (!MissionMgr || !ContactId.IsValid())
    {
        return false;
    }

    UStrategyBase* OriginBase = nullptr;
    UStrategyVehicle* Vehicle = nullptr;
    if (!MissionMgr->TryLaunchInterceptionAtContactAuto(ViewerFaction, ContactId, OriginBase, Vehicle))
    {
        return false;
    }

    const FString BaseName = OriginBase ? OriginBase->BaseName.ToString() : TEXT("Unknown");
    const FString VehicleName = Vehicle && Vehicle->VehicleDefinition
        ? Vehicle->VehicleDefinition->VehicleName.ToString()
        : TEXT("Gunship");

    const FText ToastText = FText::FromString(FString::Printf(
        TEXT("Interception launched from %s (%s)"), *BaseName, *VehicleName));
    const double ExpiresAt = GetWorld() ? GetWorld()->GetTimeSeconds() + ToastDurationSeconds : 0.0;
    PendingToasts.Add({ ToastText, ExpiresAt });
    RefreshRadarContactMarkers();
    Invalidate(EInvalidateWidgetReason::Paint);

    UE_LOG(LogTemp, Display, TEXT("[RADAR MAP] Player interception from '%s' via %s"), *BaseName, *VehicleName);
    return true;
}

void UStrategyRadarContactMapWidget::BindRadarEvents()
{
    if (UStrategyEventDispatcher* EventDisp = GetEventDispatcher())
    {
        EventDisp->OnRadarContactUpdated.AddDynamic(this, &UStrategyRadarContactMapWidget::OnRadarContactUpdated);
        EventDisp->OnRadarContactExpired.AddDynamic(this, &UStrategyRadarContactMapWidget::OnRadarContactExpired);
        EventDisp->OnOpposingFactionRadarAlert.AddDynamic(this, &UStrategyRadarContactMapWidget::OnOpposingFactionRadarAlert);
    }
}

void UStrategyRadarContactMapWidget::UnbindRadarEvents()
{
    if (UStrategyEventDispatcher* EventDisp = GetEventDispatcher())
    {
        EventDisp->OnRadarContactUpdated.RemoveDynamic(this, &UStrategyRadarContactMapWidget::OnRadarContactUpdated);
        EventDisp->OnRadarContactExpired.RemoveDynamic(this, &UStrategyRadarContactMapWidget::OnRadarContactExpired);
        EventDisp->OnOpposingFactionRadarAlert.RemoveDynamic(this, &UStrategyRadarContactMapWidget::OnOpposingFactionRadarAlert);
    }
}

void UStrategyRadarContactMapWidget::OnRadarContactUpdated(EFactionType Faction, FRadarContact Contact)
{
    if (Faction != ViewerFaction || !IsRadarContactLayerEnabled() || !Contact.ContactId.IsValid())
    {
        return;
    }

    const bool bIsNew = !SeenContactIds.Contains(Contact.ContactId);
    SeenContactIds.Add(Contact.ContactId);

    if (bIsNew)
    {
        QueueContactToast(Contact);
    }

    RefreshRadarContactMarkers();
}

void UStrategyRadarContactMapWidget::OnRadarContactExpired(EFactionType Faction, FRadarContact Contact)
{
    if (Faction != ViewerFaction)
    {
        return;
    }

    SeenContactIds.Remove(Contact.ContactId);
    RefreshRadarContactMarkers();
}

void UStrategyRadarContactMapWidget::OnOpposingFactionRadarAlert(FRadarContact Contact, FText AlertMessage)
{
    if (ViewerFaction != EFactionType::Human || AlertMessage.IsEmpty())
    {
        return;
    }

    const double ExpiresAt = GetWorld() ? GetWorld()->GetTimeSeconds() + ToastDurationSeconds : 0.0;
    PendingToasts.Add({ AlertMessage, ExpiresAt });
    Invalidate(EInvalidateWidgetReason::Paint);

    UE_LOG(LogTemp, Display, TEXT("[RADAR MAP] %s"), *AlertMessage.ToString());
}

void UStrategyRadarContactMapWidget::QueueContactToast(const FRadarContact& Contact)
{
    const FText ToastText = UStrategicSimulationDisplayHelpers::FormatRadarContactDiscoveryToast(Contact);
    if (ToastText.IsEmpty())
    {
        return;
    }

    const double ExpiresAt = GetWorld() ? GetWorld()->GetTimeSeconds() + ToastDurationSeconds : 0.0;
    PendingToasts.Add({ ToastText, ExpiresAt });
    Invalidate(EInvalidateWidgetReason::Paint);

    UE_LOG(LogTemp, Display, TEXT("[RADAR MAP] %s"), *ToastText.ToString());
}

void UStrategyRadarContactMapWidget::UpdateHoveredTooltip(const FGeometry& MyGeometry)
{
    HoveredTooltip = FText::GetEmpty();

    if (!IsRadarContactLayerEnabled())
    {
        return;
    }

    FVector2D MousePos = FSlateApplication::Get().GetCursorPos();
    const FVector2D LocalMouse = MyGeometry.AbsoluteToLocal(MousePos);
    if (const FRadarContactMapMarker* Marker = FindMarkerAtPosition(LocalMouse))
    {
        HoveredTooltip = Marker->Tooltip;
    }
}

const FRadarContactMapMarker* UStrategyRadarContactMapWidget::FindMarkerAtPosition(const FVector2D& LocalPosition) const
{
    const float HitRadiusSq = HoverRadius * HoverRadius;

    for (const FRadarContactMapMarker& Marker : CachedMarkers)
    {
        if (FVector2D::DistSquared(Marker.WidgetPosition, LocalPosition) <= HitRadiusSq)
        {
            return &Marker;
        }
    }

    return nullptr;
}

void UStrategyRadarContactMapWidget::PruneExpiredToasts(double Now)
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

void UStrategyRadarContactMapWidget::DrawContactDiamond(const FGeometry& AllottedGeometry,
    FSlateWindowElementList& OutDrawElements, int32& LayerId, const FVector2D& Center, float Size,
    float LineThickness, const FLinearColor& Color) const
{
    const FVector2D Top(Center.X, Center.Y - Size * 0.5f);
    const FVector2D Right(Center.X + Size * 0.5f, Center.Y);
    const FVector2D Bottom(Center.X, Center.Y + Size * 0.5f);
    const FVector2D Left(Center.X - Size * 0.5f, Center.Y);

    TArray<FVector2D> Points;
    Points.Add(Top);
    Points.Add(Right);
    Points.Add(Bottom);
    Points.Add(Left);
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

void UStrategyRadarContactMapWidget::DrawTooltipPanel(const FGeometry& AllottedGeometry,
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

void UStrategyRadarContactMapWidget::DrawToastPanel(const FGeometry& AllottedGeometry,
    FSlateWindowElementList& OutDrawElements, int32& LayerId, const FText& Text) const
{
    const FSlateFontInfo FontInfo = FCoreStyle::GetDefaultFontStyle("Bold", 12);
    const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
    const FVector2D TextSize = FontMeasure->Measure(Text, FontInfo);

    const FVector2D TextPadding(12.0f, 8.0f);
    const FVector2D BoxSize = TextSize + TextPadding * 2.0f;
    const FVector2D BoxPos(
        (AllottedGeometry.GetLocalSize().X - BoxSize.X) * 0.5f,
        72.0f);

    FSlateDrawElement::MakeBox(
        OutDrawElements,
        LayerId++,
        AllottedGeometry.ToPaintGeometry(BoxSize, FSlateLayoutTransform(BoxPos)),
        FCoreStyle::Get().GetBrush("NotificationList_ItemBackground"),
        ESlateDrawEffect::None,
        FLinearColor(0.18f, 0.08f, 0.02f, 0.95f));

    FSlateDrawElement::MakeText(
        OutDrawElements,
        LayerId++,
        AllottedGeometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(BoxPos + TextPadding)),
        Text,
        FontInfo,
        ESlateDrawEffect::None,
        FLinearColor(1.0f, 0.9f, 0.75f, 1.0f));
}

UMissionManagerSubsystem* UStrategyRadarContactMapWidget::GetMissionManager() const
{
    if (UGameInstance* GI = GetGameInstance())
    {
        return GI->GetSubsystem<UMissionManagerSubsystem>();
    }
    return nullptr;
}

UStrategyCampaignSubsystem* UStrategyRadarContactMapWidget::GetCampaign() const
{
    if (UGameInstance* GI = GetGameInstance())
    {
        return GI->GetSubsystem<UStrategyCampaignSubsystem>();
    }
    return nullptr;
}

UStrategyEventDispatcher* UStrategyRadarContactMapWidget::GetEventDispatcher() const
{
    if (UGameInstance* GI = GetGameInstance())
    {
        return GI->GetSubsystem<UStrategyEventDispatcher>();
    }
    return nullptr;
}