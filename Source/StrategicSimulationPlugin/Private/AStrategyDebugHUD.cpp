#include "AStrategyDebugHUD.h"
#include "Engine/Engine.h"
#include "UStrategyCampaignSubsystem.h"
#include "UTimeManagerSubsystem.h"
#include "UBaseManagerSubsystem.h"
#include "USoldierManagerSubsystem.h"

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

    FString DebugText = FString::Printf(TEXT(
        "DAY: %d\n"
        "Human Net Power: %d\n"
        "Enemy Net Power: %d\n"
        "Human Soldiers: %d | Enemy Soldiers: %d"),
        Campaign->GetTimeManager()->GetCurrentDay(),
        Campaign->GetBaseManager()->GetNetPower(EFactionType::Human),
        Campaign->GetBaseManager()->GetNetPower(EFactionType::Enemy),
        Campaign->GetSoldierManager()->GetRoster(EFactionType::Human).Num(),
        Campaign->GetSoldierManager()->GetRoster(EFactionType::Enemy).Num());

    GEngine->AddOnScreenDebugMessage(999, 0.0f, FColor::Cyan, DebugText);
}

void AStrategyDebugHUD::ToggleDebugHUD()
{
    bDebugVisible = !bDebugVisible;
    UE_LOG(LogTemp, Display, TEXT("Debug HUD %s"), bDebugVisible ? TEXT("ENABLED") : TEXT("DISABLED"));
}