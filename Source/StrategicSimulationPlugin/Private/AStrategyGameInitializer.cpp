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

    // === FACILITY ASSETS (synchronous load to prevent "not set" warnings) ===
    if (!BasicLivingQuartersAsset.IsNull())
    {
        BasicLivingQuartersAsset.LoadSynchronous();
        Campaign->BasicLivingQuartersAsset = BasicLivingQuartersAsset;
    }
    if (!BasicWorkshopAsset.IsNull())
    {
        BasicWorkshopAsset.LoadSynchronous();
        Campaign->BasicWorkshopAsset = BasicWorkshopAsset;
    }
    if (!BasicLaboratoryAsset.IsNull())
    {
        BasicLaboratoryAsset.LoadSynchronous();
        Campaign->BasicLaboratoryAsset = BasicLaboratoryAsset;
    }
    if (!BasicMedicalBayAsset.IsNull())
    {
        BasicMedicalBayAsset.LoadSynchronous();
        Campaign->BasicMedicalBayAsset = BasicMedicalBayAsset;
    }
    UE_LOG(LogTemp, Display, TEXT("✅ GameInitializer: Facility assets registered"));

    // === ROOKIE SOLDIER CLASS ===
    if (!BasicRookieClassAsset.IsNull())
    {
        BasicRookieClassAsset.LoadSynchronous();
        Campaign->BasicRookieClassAsset = BasicRookieClassAsset;
    }
    UE_LOG(LogTemp, Display, TEXT("✅ GameInitializer: Rookie soldier class registered"));

    // === START THE SIMULATION (AI now runs automatically every day) ===
    Campaign->StartSimulation();
    UE_LOG(LogTemp, Display, TEXT("🚀 Simulation STARTED — AI will act every day"));
}