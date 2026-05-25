#include "AStrategyGameInitializer.h"
#include "UStrategyCampaignSubsystem.h"
#include "Engine/Engine.h"

AStrategyGameInitializer::AStrategyGameInitializer()
{
    PrimaryActorTick.bCanEverTick = false;
}

void AStrategyGameInitializer::BeginPlay()
{
    Super::BeginPlay();

    UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();
    if (!Campaign)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ GameInitializer: Could not find Campaign Subsystem!"));
        return;
    }

    // === ITEM DATABASE (unchanged) ===
    if (ItemDatabaseAsset.IsValid())
    {
        Campaign->ItemDatabaseAsset = ItemDatabaseAsset;
        UE_LOG(LogTemp, Display, TEXT("✅ GameInitializer: ItemDatabaseAsset set"));
    }
    else if (!ItemDatabaseAsset.IsNull())
    {
        UItemDatabase* LoadedDB = ItemDatabaseAsset.LoadSynchronous();
        if (LoadedDB)
        {
            Campaign->ItemDatabaseAsset = ItemDatabaseAsset;
            UE_LOG(LogTemp, Display, TEXT("✅ GameInitializer: ItemDatabaseAsset loaded synchronously"));
        }
    }

    // === NEW: FACILITY DATABASE SETUP (data-driven) ===
    if (BasicLivingQuartersAsset.IsValid()) Campaign->BasicLivingQuartersAsset = BasicLivingQuartersAsset;
    if (BasicWorkshopAsset.IsValid())       Campaign->BasicWorkshopAsset = BasicWorkshopAsset;
    if (BasicLaboratoryAsset.IsValid())     Campaign->BasicLaboratoryAsset = BasicLaboratoryAsset;
    if (BasicMedicalBayAsset.IsValid())     Campaign->BasicMedicalBayAsset = BasicMedicalBayAsset;

    UE_LOG(LogTemp, Display, TEXT("✅ GameInitializer: Facility assets registered"));

    if (BasicRookieClassAsset.IsValid()) Campaign->BasicRookieClassAsset = BasicRookieClassAsset;
    UE_LOG(LogTemp, Display, TEXT("✅ GameInitializer: Rookie soldier class registered"));

    // === START THE SIMULATION (AI now runs automatically every day) ===
    Campaign->StartSimulation();
    UE_LOG(LogTemp, Display, TEXT("🚀 Simulation STARTED — AI will act every day"));
}