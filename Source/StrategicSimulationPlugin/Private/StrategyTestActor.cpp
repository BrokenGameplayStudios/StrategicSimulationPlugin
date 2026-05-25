#include "StrategyTestActor.h"
#include "Engine/Engine.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UStrategyEventDispatcher.h"
#include "UTimeManagerSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/UserWidget.h"

AStrategyTestActor::AStrategyTestActor()
{
    PrimaryActorTick.bCanEverTick = false;
}

void AStrategyTestActor::BeginPlay()
{
    Super::BeginPlay();
    RunPhase13Test();
}

void AStrategyTestActor::RunPhase13Test()
{
    UE_LOG(LogTemp, Display, TEXT("=== STRATEGICSIMULATIONPLUGIN PHASE 13 TEST START ==="));

    // Spawn the Strategic HUD on screen
    TSubclassOf<UUserWidget> HUDClass = LoadClass<UUserWidget>(nullptr, TEXT("/StrategicSimulationPlugin/UI/WBP_StrategicHUD.WBP_StrategicHUD_C"));
    if (HUDClass)
    {
        UUserWidget* HUD = CreateWidget(GetWorld(), HUDClass);
        if (HUD)
        {
            HUD->AddToViewport(0);
            UE_LOG(LogTemp, Display, TEXT("WBP_StrategicHUD spawned on screen"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to load WBP_StrategicHUD — check the path!"));
    }

    // Subscribe to events (for logging)
    UStrategyEventDispatcher* EventDisp = GetGameInstance()->GetSubsystem<UStrategyEventDispatcher>();
    if (EventDisp)
    {
        EventDisp->OnSoldierRecruited.AddDynamic(this, &AStrategyTestActor::OnSoldierRecruited_Test);
        EventDisp->OnResearchCompleted.AddDynamic(this, &AStrategyTestActor::OnResearchCompleted_Test);
    }

    UE_LOG(LogTemp, Display, TEXT("Event dispatcher is live — use the button in the HUD to test"));

    if (GEngine)
        GEngine->AddOnScreenDebugMessage(-1, 30.f, FColor::Green, TEXT("HUD is on screen!\nUse the test button inside WBP_StrategicHUD"));

    UE_LOG(LogTemp, Display, TEXT("=== PHASE 13 TEST COMPLETE ==="));
}

UFUNCTION()
void AStrategyTestActor::OnSoldierRecruited_Test(EFactionType Faction, UStrategySoldier* Soldier)
{
    UE_LOG(LogTemp, Display, TEXT("[EVENT] Soldier recruited: %s for %s"), *Soldier->SoldierName, *UEnum::GetValueAsString(Faction));
}

UFUNCTION()
void AStrategyTestActor::OnResearchCompleted_Test(EFactionType Faction, UResearchTechDefinition* Tech)
{
    UE_LOG(LogTemp, Display, TEXT("[EVENT] Research completed: %s for %s"), *Tech->ProjectName.ToString(), *UEnum::GetValueAsString(Faction));
}