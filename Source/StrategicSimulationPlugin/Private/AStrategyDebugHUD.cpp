#include "AStrategyDebugHUD.h"
#include "Engine/Engine.h"
#include "UStrategyCampaignSubsystem.h"
#include "UTimeManagerSubsystem.h"
#include "UBaseManagerSubsystem.h"
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

void AStrategyDebugHUD::DrawHUD()
{
    Super::DrawHUD();

    if (!bShowStrategyMap || !Canvas) return;

    UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
    UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>();
    if (!BaseMgr) return;

    // Draw bases
    for (UStrategyBase* Base : BaseMgr->GetBases(EFactionType::Human))
        DrawBase(Base, FLinearColor::Blue);

    for (UStrategyBase* Base : BaseMgr->GetBases(EFactionType::Enemy))
        DrawBase(Base, FLinearColor::Red);

    // Draw active missions
    if (MissionMgr)
    {
        for (UMissionGroup* Mission : MissionMgr->ActiveMissions)
            DrawMission(Mission);
    }

    // Legend
    Canvas->DrawText(GEngine->GetSmallFont(), FText::FromString("BLUE = Human Bases"), 50, 50, 1.0f, 1.0f, FFontRenderInfo());
    Canvas->DrawText(GEngine->GetSmallFont(), FText::FromString("RED  = Enemy Bases"), 50, 80, 1.0f, 1.0f, FFontRenderInfo());
    Canvas->DrawText(GEngine->GetSmallFont(), FText::FromString("Yellow lines = Active Missions"), 50, 110, 1.0f, 1.0f, FFontRenderInfo());
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