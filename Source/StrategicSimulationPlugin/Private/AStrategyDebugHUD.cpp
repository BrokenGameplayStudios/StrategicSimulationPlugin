#include "AStrategyDebugHUD.h"
#include "Engine/Engine.h"
#include "UStrategyCampaignSubsystem.h"
#include "UTimeManagerSubsystem.h"
#include "UBaseManagerSubsystem.h"
#include "StrategicSiteDefinition.h"
#include "USoldierManagerSubsystem.h"
#include "UStrategyBase.h"
#include "UMissionGroup.h"
#include "UMissionManagerSubsystem.h"
#include "Engine/Canvas.h"

AStrategyDebugHUD::AStrategyDebugHUD()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AStrategyDebugHUD::BeginPlay()
{
    Super::BeginPlay();
    ToggleDebugHUD();
}

void AStrategyDebugHUD::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (!bDebugVisible || !GEngine) return;

    UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();
    if (!Campaign) return;

    UBaseManagerSubsystem* BaseMgr = Campaign->GetBaseManager();
    UResourceManagerSubsystem* ResourceMgr = Campaign->GetResourceManager();
    if (!BaseMgr || !ResourceMgr) return;

    FString DebugText = FString::Printf(TEXT("=== STRATEGIC SIMULATION DEBUG ===\nDAY: %d\n\n"),
        Campaign->GetTimeManager()->GetCurrentDay());

    // === HUMAN RESOURCES ===
    FResourceStockpile HumanRes = ResourceMgr->GetResources(EFactionType::Human);
    DebugText += FString::Printf(TEXT("[AI] EFactionType::Human AI — Day %d decision - Bases: %d | 💰%d | 🛠️%d | 🧬%d | ⚗️%d | 🌌%d | 📚%d\n"),
        Campaign->GetTimeManager()->GetCurrentDay(),
        BaseMgr->GetBases(EFactionType::Human).Num(),
        HumanRes.Money, HumanRes.Metals, HumanRes.Biologicals,
        HumanRes.Chemicals, HumanRes.ExoticMaterial, HumanRes.ResearchPoints);

    // === ENEMY RESOURCES ===
    FResourceStockpile EnemyRes = ResourceMgr->GetResources(EFactionType::Enemy);
    DebugText += FString::Printf(TEXT("[AI] EFactionType::Enemy AI — Day %d decision - Bases: %d | 💰%d | 🛠️%d | 🧬%d | ⚗️%d | 🌌%d | 📚%d\n"),
        Campaign->GetTimeManager()->GetCurrentDay(),
        BaseMgr->GetBases(EFactionType::Enemy).Num(),
        EnemyRes.Money, EnemyRes.Metals, EnemyRes.Biologicals,
        EnemyRes.Chemicals, EnemyRes.ExoticMaterial, EnemyRes.ResearchPoints);

    DebugText += TEXT("\n");

    // === BASE STATES (clean log-style) ===
    DebugText += BaseMgr->GetBaseStateDebugString(EFactionType::Human);
    DebugText += TEXT("\n");
    DebugText += BaseMgr->GetBaseStateDebugString(EFactionType::Enemy);

    GEngine->AddOnScreenDebugMessage(999, 0.0f, FColor::Cyan, DebugText);
}

void AStrategyDebugHUD::ToggleDebugHUD()
{
    bDebugVisible = !bDebugVisible;
    UE_LOG(LogTemp, Display, TEXT("Debug HUD %s"), bDebugVisible ? TEXT("ENABLED") : TEXT("DISABLED"));
}

void AStrategyDebugHUD::ToggleStrategyMap()
{
    bShowStrategyMap = !bShowStrategyMap;
    UE_LOG(LogTemp, Display, TEXT("[DEBUG HUD] Strategy Map %s"), bShowStrategyMap ? TEXT("ENABLED") : TEXT("DISABLED"));
}

// ==================== PASTE THIS FULL FUNCTION OVER THE EXISTING DrawHUD() ====================
void AStrategyDebugHUD::DrawHUD()
{
    Super::DrawHUD();

    if (!bShowStrategyMap || !Canvas) return;

    UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
    UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>();
    if (!BaseMgr) return;

    // === EXISTING: Draw bases (unchanged) ===
    for (UStrategyBase* Base : BaseMgr->GetBases(EFactionType::Human))
        DrawBase(Base, FLinearColor::Blue);

    for (UStrategyBase* Base : BaseMgr->GetBases(EFactionType::Enemy))
        DrawBase(Base, FLinearColor::Red);

    // === EXISTING: Draw active missions (unchanged) ===
    if (MissionMgr)
    {
        for (UMissionGroup* Mission : MissionMgr->ActiveMissions)
            DrawMission(Mission);
    }

    // === EXISTING: Draw live vehicles (unchanged) ===
    if (MissionMgr)
    {
        for (UMissionGroup* Mission : MissionMgr->ActiveMissions)
        {
            if (!Mission) continue;

            for (UStrategyVehicle* Vehicle : Mission->VehiclesInFleet)
            {
                if (Vehicle)
                    DrawVehicle(Vehicle);
            }
        }
    }

    // === PHASE 1: Draw ALL potential nodes + fog-of-war markers ===
    DrawAllPotentialSites();

    // Legend (updated)
    Canvas->DrawText(GEngine->GetSmallFont(), FText::FromString("BLUE  = Human Bases"), 50, 50, 1.0f, 1.0f, FFontRenderInfo());
    Canvas->DrawText(GEngine->GetSmallFont(), FText::FromString("RED   = Enemy Bases"), 50, 80, 1.0f, 1.0f, FFontRenderInfo());
    Canvas->DrawText(GEngine->GetSmallFont(), FText::FromString("GREEN = Human Vehicles  |  RED = Enemy Vehicles"), 50, 110, 1.0f, 1.0f, FFontRenderInfo());
    Canvas->DrawText(GEngine->GetSmallFont(), FText::FromString("Yellow lines = Active Mission paths"), 50, 140, 1.0f, 1.0f, FFontRenderInfo());
    Canvas->DrawText(GEngine->GetSmallFont(), FText::FromString("WHITE SQUARE = Node | Blue/Red dots = Discovered by faction"), 50, 170, 1.0f, 1.0f, FFontRenderInfo());
}

// ==================== NEW FUNCTION - ADD THIS ANYWHERE IN THE FILE (e.g. after DrawDiscoveredSites) ====================
void AStrategyDebugHUD::DrawAllPotentialSites()
{
    if (!Canvas) return;

    UBaseManagerSubsystem* BaseManager = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
    if (!BaseManager) return;

    float Scale = GetCurrentMapScale();

    for (UStrategySiteDefinition* Site : BaseManager->AllPotentialSites)
    {
        if (!Site) continue;

        FVector2D ScreenPos = GetScreenPosition(Site->Location);

        // Main node (white square) — scales with map
        float NodeSize = 10.0f * Scale;
        Canvas->K2_DrawBox(ScreenPos - FVector2D(NodeSize * 0.5f, NodeSize * 0.5f),
            FVector2D(NodeSize, NodeSize), 1.0f, FLinearColor(0.8f, 0.8f, 0.8f, 0.7f));

        // 4x4 discovery markers (blue left / red right) — also scale
        float MarkerSize = 4.0f * Scale;
        if (BaseManager->DiscoveredSitesHuman.Contains(Site))
        {
            Canvas->K2_DrawBox(ScreenPos - FVector2D(8.0f * Scale, 12.0f * Scale),
                FVector2D(MarkerSize, MarkerSize), 1.0f, FLinearColor::Blue);
        }
        if (BaseManager->DiscoveredSitesEnemy.Contains(Site))
        {
            Canvas->K2_DrawBox(ScreenPos + FVector2D(4.0f * Scale, -12.0f * Scale),
                FVector2D(MarkerSize, MarkerSize), 1.0f, FLinearColor::Red);
        }
    }

    // Live count (fixed size so it's always readable)
    FString CountText = FString::Printf(TEXT("Nodes: %d | Human discovered: %d | Enemy discovered: %d"),
        BaseManager->AllPotentialSites.Num(),
        BaseManager->DiscoveredSitesHuman.Num(),
        BaseManager->DiscoveredSitesEnemy.Num());
    Canvas->DrawText(GEngine->GetSmallFont(), FText::FromString(CountText), 50.0f, 200.0f, 1.0f, 1.0f, FFontRenderInfo());
}

void AStrategyDebugHUD::DrawVehicle(UStrategyVehicle* Vehicle)
{
    if (!Vehicle || !Canvas) return;

    float Scale = GetCurrentMapScale();

    UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>();
    float CurrentHours = MissionMgr ? MissionMgr->GetCurrentGameHours() : 0.0f;

    FVector2D ScreenPos = GetScreenPosition(Vehicle->CurrentPosition);

    // Robust faction color
    FLinearColor VehicleColor = FLinearColor::Red;
    if (Vehicle->HomeBase)
        VehicleColor = (Vehicle->HomeBase->OwningFaction == EFactionType::Human) ? FLinearColor::Green : FLinearColor::Red;

    // Scaled vehicle dot
    float VehicleSize = 8.0f * Scale;
    Canvas->K2_DrawBox(ScreenPos - FVector2D(VehicleSize * 0.5f, VehicleSize * 0.5f),
        FVector2D(VehicleSize, VehicleSize), 2.0f * Scale, VehicleColor);

    // Scaled name label
    Canvas->DrawText(GEngine->GetSmallFont(),
        FText::FromString(Vehicle->VehicleDefinition ? Vehicle->VehicleDefinition->VehicleName.ToString() : TEXT("VEH")),
        ScreenPos.X + (12.0f * Scale), ScreenPos.Y - (8.0f * Scale),
        0.7f, 0.7f, FFontRenderInfo());

    // Waypoint paths + progress
    if (bShowVehiclePaths && Vehicle->CurrentWaypoints.Num() >= 2)
    {
        for (int32 i = 0; i < Vehicle->CurrentWaypoints.Num() - 1; ++i)
        {
            FVector2D A = GetScreenPosition(Vehicle->CurrentWaypoints[i]);
            FVector2D B = GetScreenPosition(Vehicle->CurrentWaypoints[i + 1]);
            Canvas->K2_DrawLine(A, B, 1.5f * Scale, FLinearColor::Yellow);
        }

        if (Vehicle->TotalTravelTimeHours > 0.0f)
        {
            float Progress = FMath::Clamp((CurrentHours - Vehicle->LaunchGameTimeHours) / Vehicle->TotalTravelTimeHours, 0.0f, 1.0f);
            FVector2D ProgressPos = Vehicle->GetPositionOnPath(Progress);
            FVector2D ScreenProgress = GetScreenPosition(ProgressPos);

            float ProgressSize = 6.0f * Scale;
            Canvas->K2_DrawBox(ScreenProgress - FVector2D(ProgressSize * 0.5f, ProgressSize * 0.5f),
                FVector2D(ProgressSize, ProgressSize), 1.5f * Scale, VehicleColor);   // Changed to VehicleColor for consistency
        }
    }

    // === RADAR CIRCLE (now correctly follows the moving vehicle) ===
    if (Vehicle->PingRadiusPixels > 0.0f)
    {
        float ScreenRadius = Vehicle->PingRadiusPixels * Scale;           // Use the Scale variable we already have
        FLinearColor RadarColor(0.0f, 1.0f, 1.0f, 0.35f); // Cyan

        // Draw circle using line segments
        const int32 NumSegments = 48;
        for (int32 i = 0; i < NumSegments; ++i)
        {
            float Angle1 = (float)i / NumSegments * 2.0f * PI;
            float Angle2 = (float)(i + 1) / NumSegments * 2.0f * PI;

            FVector2D P1(
                ScreenPos.X + FMath::Cos(Angle1) * ScreenRadius,
                ScreenPos.Y + FMath::Sin(Angle1) * ScreenRadius
            );
            FVector2D P2(
                ScreenPos.X + FMath::Cos(Angle2) * ScreenRadius,
                ScreenPos.Y + FMath::Sin(Angle2) * ScreenRadius
            );

            Canvas->K2_DrawLine(P1, P2, 1.0f, RadarColor);
        }

        // Show current radius value
        FString RadiusText = FString::Printf(TEXT("R: %.0f"), Vehicle->PingRadiusPixels);
        Canvas->DrawText(GEngine->GetSmallFont(), FText::FromString(RadiusText),
            ScreenPos.X + 15.0f * Scale,
            ScreenPos.Y - 20.0f * Scale,
            0.6f, 0.6f, FFontRenderInfo());
    }
}

void AStrategyDebugHUD::DrawBase(UStrategyBase* Base, FLinearColor Color)
{
    if (!Base || !Canvas) return;

    float Scale = GetCurrentMapScale();

    FVector2D ScreenPos = GetScreenPosition(Base->MapLocation);

    // Scaled base box (20x20 logical pixels → grows/shrinks with map)
    float BaseSize = 20.0f * Scale;
    Canvas->K2_DrawBox(ScreenPos - FVector2D(BaseSize * 0.5f, BaseSize * 0.5f),
        FVector2D(BaseSize, BaseSize), 2.0f * Scale, Color);

    // Scaled name label offset
    Canvas->DrawText(GEngine->GetSmallFont(),
        FText::FromString(Base->BaseName.ToString()),
        ScreenPos.X + (25.0f * Scale), ScreenPos.Y - (10.0f * Scale),
        1.0f, 1.0f, FFontRenderInfo());
}

void AStrategyDebugHUD::DrawMission(UMissionGroup* Mission)
{
    if (!Mission || !Mission->OriginBase || !Canvas) return;

    FVector2D BasePos = GetScreenPosition(Mission->OriginBase->MapLocation);

    // === Clean mission label near the originating base ===
    FString Info = FString::Printf(TEXT("%s"), *UEnum::GetValueAsString(Mission->MissionType));
    Canvas->DrawText(GEngine->GetSmallFont(),
        FText::FromString(Info),
        BasePos.X + 15.0f, BasePos.Y - 5.0f,
        0.8f, 0.8f, FFontRenderInfo());

    // === Proper faint connection line from base to the mission group ===
    // (only draws if the mission has vehicles and at least one is moving)
    if (Mission->VehiclesInFleet.Num() > 0)
    {
        UStrategyVehicle* LeadVehicle = Mission->VehiclesInFleet[0];
        if (LeadVehicle && LeadVehicle->CurrentMission != nullptr)
        {
            FVector2D VehiclePos = GetScreenPosition(LeadVehicle->CurrentPosition);

            // Very faint yellow line so it doesn't fight with the detailed vehicle path
            Canvas->K2_DrawLine(BasePos, VehiclePos, 1.0f, FLinearColor(1.0f, 1.0f, 0.0f, 0.25f));
        }
    }
}

FVector2D AStrategyDebugHUD::GetScreenPosition(const FVector2D& LogicalPos) const
{
    if (!Canvas)
    {
        // Safety fallback
        return (LogicalPos * MapScale) + MapOffset;
    }

    const float LogicalWidth = 1920.0f;
    const float LogicalHeight = 1080.0f;

    float UniformScale = GetCurrentMapScale();

    FVector2D ScaledPos = LogicalPos * UniformScale;

    FVector2D CanvasCenter(Canvas->SizeX * 0.5f, Canvas->SizeY * 0.5f);
    FVector2D LogicalCenter(LogicalWidth * 0.5f, LogicalHeight * 0.5f);

    return CanvasCenter + (ScaledPos - LogicalCenter * UniformScale);
}

float AStrategyDebugHUD::GetCurrentMapScale() const
{
    if (!Canvas)
    {
        return 1.0f;
    }

    const float LogicalWidth = 1920.0f;
    const float LogicalHeight = 1080.0f;

    float ScaleX = Canvas->SizeX / LogicalWidth;
    float ScaleY = Canvas->SizeY / LogicalHeight;
    return FMath::Min(ScaleX, ScaleY);   // uniform scale, preserves aspect ratio
}