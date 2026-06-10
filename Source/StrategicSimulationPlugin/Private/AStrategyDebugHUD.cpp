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

    // Draw EVERY generated node as a dim white square
    for (UStrategySiteDefinition* Site : BaseManager->AllPotentialSites)
    {
        if (!Site) continue;

        FVector2D ScreenPos = GetScreenPosition(Site->Location);

        // Main node: dim white square (10x10 pixels)
        Canvas->K2_DrawBox(ScreenPos - FVector2D(5.0f, 5.0f), FVector2D(10.0f, 10.0f), 1.0f, FLinearColor(0.8f, 0.8f, 0.8f, 0.7f));

        // === Fog-of-war markers (exactly as you requested) ===
        // 4x4 blue square top-left if Human knows about it
        if (BaseManager->DiscoveredSitesHuman.Contains(Site))
        {
            Canvas->K2_DrawBox(ScreenPos - FVector2D(8.0f, 12.0f), FVector2D(4.0f, 4.0f), 1.0f, FLinearColor::Blue);
        }

        // 4x4 red square top-right if Enemy knows about it
        if (BaseManager->DiscoveredSitesEnemy.Contains(Site))
        {
            Canvas->K2_DrawBox(ScreenPos + FVector2D(4.0f, -12.0f), FVector2D(4.0f, 4.0f), 1.0f, FLinearColor::Red);
        }
    }

    // Optional live count in corner
    FString CountText = FString::Printf(TEXT("Nodes: %d | Human discovered: %d | Enemy discovered: %d"),
        BaseManager->AllPotentialSites.Num(),
        BaseManager->DiscoveredSitesHuman.Num(),
        BaseManager->DiscoveredSitesEnemy.Num());
    Canvas->DrawText(GEngine->GetSmallFont(), FText::FromString(CountText), 50.0f, 200.0f, 1.0f, 1.0f, FFontRenderInfo());
}

void AStrategyDebugHUD::DrawVehicle(UStrategyVehicle* Vehicle)
{
    if (!Vehicle || !Canvas) return;

    UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>();
    float CurrentHours = MissionMgr ? MissionMgr->GetCurrentGameHours() : 0.0f;

    FVector2D ScreenPos = GetScreenPosition(Vehicle->CurrentPosition);

    // Robust faction color
    FLinearColor VehicleColor = FLinearColor::Red;
    if (Vehicle->HomeBase)
        VehicleColor = (Vehicle->HomeBase->OwningFaction == EFactionType::Human) ? FLinearColor::Green : FLinearColor::Red;

    // Draw the moving dot
    Canvas->K2_DrawBox(ScreenPos - FVector2D(4, 4), FVector2D(8, 8), 2.0f, VehicleColor);

    // Name label
    Canvas->DrawText(GEngine->GetSmallFont(),
        FText::FromString(Vehicle->VehicleDefinition ? Vehicle->VehicleDefinition->VehicleName.ToString() : TEXT("VEH")),
        ScreenPos.X + 12, ScreenPos.Y - 8, 0.7f, 0.7f, FFontRenderInfo());

    // Waypoint lines + progress (only if still on a live path)
    if (bShowVehiclePaths && Vehicle->CurrentWaypoints.Num() >= 2)
    {
        for (int32 i = 0; i < Vehicle->CurrentWaypoints.Num() - 1; ++i)
        {
            FVector2D A = GetScreenPosition(Vehicle->CurrentWaypoints[i]);
            FVector2D B = GetScreenPosition(Vehicle->CurrentWaypoints[i + 1]);
            Canvas->K2_DrawLine(A, B, 1.5f, FLinearColor::Yellow);
        }

        if (Vehicle->TotalTravelTimeHours > 0.0f)
        {
            float Progress = FMath::Clamp((CurrentHours - Vehicle->LaunchGameTimeHours) / Vehicle->TotalTravelTimeHours, 0.0f, 1.0f);
            FVector2D ProgressPos = Vehicle->GetPositionOnPath(Progress);
            FVector2D ScreenProgress = GetScreenPosition(ProgressPos);
            Canvas->K2_DrawBox(ScreenProgress - FVector2D(3, 3), FVector2D(6, 6), 1.5f, FLinearColor::White);
        }
    }
}

void AStrategyDebugHUD::DrawBase(UStrategyBase* Base, FLinearColor Color)
{
    if (!Base || !Canvas) return;

    // <<< CHANGE THIS TO YOUR ACTUAL POSITION MEMBER NAME >>>
    // Look in UStrategyBase.h for the FVector2D member (common names: Location, BaseLocation, Position, WorldLocation)
    FVector2D ScreenPos = GetScreenPosition(Base->MapLocation);   // ←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←

    Canvas->K2_DrawBox(ScreenPos - FVector2D(10, 10), FVector2D(20, 20), 2.0f, Color);

    Canvas->DrawText(GEngine->GetSmallFont(), FText::FromString(Base->BaseName.ToString()), ScreenPos.X + 25, ScreenPos.Y - 10, 1.0f, 1.0f, FFontRenderInfo());
}

void AStrategyDebugHUD::DrawMission(UMissionGroup* Mission)
{
    if (!Mission || !Mission->OriginBase || !Canvas) return;

    FVector2D Start = GetScreenPosition(Mission->OriginBase->MapLocation);
    FVector2D End = Start + FVector2D(120.0f, 40.0f); // temporary direction

    Canvas->K2_DrawLine(Start, End, 3.0f, FLinearColor::Yellow);

    FString Info = FString::Printf(TEXT("%s"), *UEnum::GetValueAsString(Mission->MissionType));
    Canvas->DrawText(GEngine->GetSmallFont(), FText::FromString(Info), End.X + 10, End.Y, 0.8f, 0.8f, FFontRenderInfo());
}

FVector2D AStrategyDebugHUD::GetScreenPosition(const FVector2D& WorldPos) const
{
    return (WorldPos * MapScale) + MapOffset;
}