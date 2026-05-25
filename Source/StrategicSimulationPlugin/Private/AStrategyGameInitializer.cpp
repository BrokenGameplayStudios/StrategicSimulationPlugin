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

    // === NEW DATABASES (data-driven) ===
    if (!FacilityDatabaseAsset.IsNull())
    {
        FacilityDatabaseAsset.LoadSynchronous();
        Campaign->FacilityDatabaseAsset = FacilityDatabaseAsset;
    }
    if (!SoldierClassDatabaseAsset.IsNull())
    {
        SoldierClassDatabaseAsset.LoadSynchronous();
        Campaign->SoldierClassDatabaseAsset = SoldierClassDatabaseAsset;
    }
    if (!ResearchDatabaseAsset.IsNull())
    {
        ResearchDatabaseAsset.LoadSynchronous();
        Campaign->ResearchDatabaseAsset = ResearchDatabaseAsset;
    }
    UE_LOG(LogTemp, Display, TEXT("✅ GameInitializer: All Databases registered"));

    // === START THE SIMULATION (AI now runs automatically every day) ===
    Campaign->StartSimulation();
    UE_LOG(LogTemp, Display, TEXT("🚀 Simulation STARTED — AI will act every day"));
}