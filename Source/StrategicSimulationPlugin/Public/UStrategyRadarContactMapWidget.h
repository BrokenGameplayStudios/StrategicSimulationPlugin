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

/** Slate-painted overlay for passive radar contacts; hover for intel, optional click-to-intercept. */
UCLASS(Blueprintable)
class STRATEGICSIMULATIONPLUGIN_API UStrategyRadarContactMapWidget : public UStrategyUserWidget
{
    GENERATED_BODY()

public:
    /** Disables Blueprint tick; subclasses use NativeTick for contact refresh. */
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

    /**
     * When true, also draws the opposing faction's contacts (magenta for Enemy, cyan for Human).
     * Auto-enabled while both factions run AI simulation.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radar Contact Map")
    bool bShowOpposingFactionContacts = true;

    /**
     * When true, left-click on a contact launches interception even if faction AI is enabled.
     * Leave false in AI-vs-AI spectate; designers use TryInterceptContactById from a button instead.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radar Contact Map")
    bool bAllowPlayerClickToIntercept = false;

    /** Returns true when campaign flags enable the player radar contact layer. */
    UFUNCTION(BlueprintPure, Category = "Radar Contact Map")
    bool IsRadarContactLayerEnabled() const;

    /** Returns the last cached contact marker list built by RefreshRadarContactMarkers. */
    UFUNCTION(BlueprintPure, Category = "Radar Contact Map")
    TArray<FRadarContactMapMarker> GetRadarContactMarkers() const { return CachedMarkers; }

    /** Returns tooltip text for the contact marker currently under the cursor. */
    UFUNCTION(BlueprintPure, Category = "Radar Contact Map")
    FText GetHoveredTooltipText() const { return HoveredTooltip; }

    /** Contact under the cursor, if any (for designer intercept buttons). */
    UFUNCTION(BlueprintPure, Category = "Radar Contact Map")
    FGuid GetHoveredContactId() const { return HoveredContactId; }

    /** Faction that owns the hovered contact. */
    UFUNCTION(BlueprintPure, Category = "Radar Contact Map")
    EFactionType GetHoveredContactFaction() const { return HoveredContactFaction; }

    /** False when faction AI handles interceptions (spectate / AI-vs-AI). */
    UFUNCTION(BlueprintPure, Category = "Radar Contact Map")
    bool IsClickToInterceptAllowed() const;

    /** True when opposing-faction contacts are included in the overlay. */
    UFUNCTION(BlueprintPure, Category = "Radar Contact Map")
    bool ShouldShowOpposingFactionContacts() const;

    /** Rebuilds CachedMarkers from faction radar contacts and invalidates paint. */
    UFUNCTION(BlueprintCallable, Category = "Radar Contact Map")
    void RefreshRadarContactMarkers();

    /** Player click path: auto-pick nearest idle gunship and launch interception. */
    UFUNCTION(BlueprintCallable, Category = "Radar Contact Map")
    bool TryInterceptContactAtWidgetPosition(FVector2D LocalWidgetPosition);

    /** Designer / player button: dispatch interception for ViewerFaction. */
    UFUNCTION(BlueprintCallable, Category = "Radar Contact Map")
    bool TryInterceptContactById(FGuid ContactId);

    /** Designer button: dispatch interception for a specific faction's contact. */
    UFUNCTION(BlueprintCallable, Category = "Radar Contact Map")
    bool TryInterceptContactByIdForFaction(EFactionType Faction, FGuid ContactId);

protected:
    /** Binds radar events, sets visible input mode, and refreshes contact markers. */
    virtual void NativeConstruct() override;

    /** Unbinds radar events before destruction. */
    virtual void NativeDestruct() override;

    /** Prunes toasts, refreshes markers on resize, and updates hover tooltip. */
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    /** Draws contact diamonds, hover tooltip, and the active alert toast. */
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

    /** Handles left-click on a contact marker when click-to-intercept is allowed. */
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
    UPROPERTY(Transient)
    TArray<FRadarContactMapMarker> CachedMarkers;

    UPROPERTY(Transient)
    FText HoveredTooltip;

    UPROPERTY(Transient)
    FGuid HoveredContactId;

    UPROPERTY(Transient)
    EFactionType HoveredContactFaction = EFactionType::Neutral;

    FVector2D LastCachedWidgetSize = FVector2D::ZeroVector;

    struct FPendingToast
    {
        FText Message;
        double ExpiresAt = 0.0;
    };

    TArray<FPendingToast> PendingToasts;
    TSet<FGuid> SeenContactIds;

    /** Event handler: queues toast for new contacts and refreshes markers on update. */
    UFUNCTION()
    void OnRadarContactUpdated(EFactionType Faction, FRadarContact Contact);

    /** Event handler: forgets expired contact IDs and refreshes markers. */
    UFUNCTION()
    void OnRadarContactExpired(EFactionType Faction, FRadarContact Contact);

    /** Event handler: shows an opposing-faction inbound threat alert toast to the player. */
    UFUNCTION()
    void OnOpposingFactionRadarAlert(FRadarContact Contact, FText AlertMessage);

    /** Subscribes to radar-related UStrategyEventDispatcher delegates. */
    void BindRadarEvents();

    /** Removes radar-related UStrategyEventDispatcher delegate bindings. */
    void UnbindRadarEvents();

    /** Adds a timed discovery toast for a newly seen radar contact. */
    void QueueContactToast(const FRadarContact& Contact);

    /** Sets HoveredTooltip from the contact marker nearest the cursor within HoverRadius. */
    void UpdateHoveredTooltip(const FGeometry& MyGeometry);

    /** Removes expired toasts and invalidates paint when the queue shrinks. */
    void PruneExpiredToasts(double Now);

    /** Returns the first marker within HoverRadius of LocalPosition, or nullptr. */
    const FRadarContactMapMarker* FindMarkerAtPosition(const FVector2D& LocalPosition) const;

    /** Draws an outlined diamond marker at Center using Slate line elements. */
    void DrawContactDiamond(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32& LayerId,
        const FVector2D& Center, float Size, float LineThickness, const FLinearColor& Color) const;

    /** Draws a small tooltip panel anchored near the cursor. */
    void DrawTooltipPanel(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32& LayerId,
        const FVector2D& Anchor, const FText& Text) const;

    /** Draws the topmost active alert toast centered below the widget top. */
    void DrawToastPanel(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32& LayerId,
        const FText& Text) const;

    /** Returns UMissionManagerSubsystem from the widget's game instance. */
    UMissionManagerSubsystem* GetMissionManager() const;

    /** Returns UStrategyCampaignSubsystem from the widget's game instance. */
    UStrategyCampaignSubsystem* GetCampaign() const;

    /** Returns UStrategyEventDispatcher from the widget's game instance. */
    UStrategyEventDispatcher* GetEventDispatcher() const;

    /** Returns UAIControllerSubsystem from the widget's game instance. */
    class UAIControllerSubsystem* GetAIController() const;

    /** True when left-click may launch interception for the given contact owner faction. */
    bool IsClickToInterceptAllowedForFaction(EFactionType Faction) const;

    /** Shared launch path for click and designer-button dispatch. */
    bool LaunchInterceptionForContact(EFactionType Faction, FGuid ContactId);
};