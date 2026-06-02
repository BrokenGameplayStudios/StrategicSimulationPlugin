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

    // Apply debug settings from the actor in the level
    Campaign->bVerboseLogging = bVerboseLogging;
    Campaign->bShowUnlockMessages = bShowUnlockMessages;
    Campaign->bShowFacilityTicks = bShowFacilityTicks;

    UE_LOG(LogTemp, Display, TEXT("[DEBUG] Settings applied → Verbose: %s | Unlocks: %s | FacilityTicks: %s"),
        bVerboseLogging ? TEXT("ON") : TEXT("OFF"),
        bShowUnlockMessages ? TEXT("ON") : TEXT("OFF"),
        bShowFacilityTicks ? TEXT("ON") : TEXT("OFF"));

    // === DATABASE LOADING (unchanged) ===
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

    if (UFacilityDatabase* FacDB = FacilityDatabaseAsset.Get())
    {
        for (auto& Soft : FacDB->AvailableFacilities)
            Soft.LoadSynchronous();
        Campaign->FacilityDatabaseAsset = FacilityDatabaseAsset;
        UE_LOG(LogTemp, Display, TEXT("✅ Loaded %d facilities from FacilityDatabase"), FacDB->AvailableFacilities.Num());
    }

    if (USoldierClassDatabase* SoldierDB = SoldierClassDatabaseAsset.Get())
    {
        for (auto& Soft : SoldierDB->AvailableSoldierClasses)
            Soft.LoadSynchronous();
        Campaign->SoldierClassDatabaseAsset = SoldierClassDatabaseAsset;
        UE_LOG(LogTemp, Display, TEXT("✅ Loaded %d soldier classes from SoldierClassDatabase"), SoldierDB->AvailableSoldierClasses.Num());
    }

    if (UResearchDatabase* ResearchDB = ResearchDatabaseAsset.Get())
    {
        for (auto& Soft : ResearchDB->AvailableTechs)
            Soft.LoadSynchronous();
        Campaign->ResearchDatabaseAsset = ResearchDatabaseAsset;
        UE_LOG(LogTemp, Display, TEXT("✅ Loaded %d research projects from ResearchDatabase"), ResearchDB->AvailableTechs.Num());
    }

    if (UVehicleDatabase* VehicleDB = VehicleDatabaseAsset.Get())
    {
        for (auto& Soft : VehicleDB->AvailableVehicles)
            Soft.LoadSynchronous();
        Campaign->VehicleDatabaseAsset = VehicleDatabaseAsset;
        UE_LOG(LogTemp, Display, TEXT("✅ Loaded %d vehicles from VehicleDatabase"), VehicleDB->AvailableVehicles.Num());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[VEHICLE DATABASE] VehicleDatabaseAsset is NULL!"));
    }

    // === NEW: Load Vehicle Item Database (weapons, defense systems, ammo) ===
    if (VehicleItemDatabaseAsset.IsValid())
    {
        Campaign->VehicleItemDatabaseAsset = VehicleItemDatabaseAsset;
        UE_LOG(LogTemp, Display, TEXT("GameInitializer: VehicleItemDatabaseAsset set"));
    }
    else if (!VehicleItemDatabaseAsset.IsNull())
    {
        UItemDatabase* LoadedDB = VehicleItemDatabaseAsset.LoadSynchronous();
        if (LoadedDB)
        {
            Campaign->VehicleItemDatabaseAsset = VehicleItemDatabaseAsset;
            UE_LOG(LogTemp, Display, TEXT("GameInitializer: VehicleItemDatabaseAsset loaded synchronously"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[VEHICLE ITEMS] VehicleItemDatabaseAsset is NULL!"));
    }

    UAIControllerSubsystem* AIController = GetWorld()->GetGameInstance()->GetSubsystem<UAIControllerSubsystem>();
    if (AIController)
    {
        AIController->SetSimulateHumanAI(bStartWithHumanAI);
        AIController->SetSimulateEnemyAI(bStartWithEnemyAI);
        UE_LOG(LogTemp, Display, TEXT("AStrategyGameInitializer: Applied AI simulation settings - Human: %s | Enemy: %s"),
            bStartWithHumanAI ? TEXT("ON") : TEXT("OFF"),
            bStartWithEnemyAI ? TEXT("ON") : TEXT("OFF"));
    }


    UE_LOG(LogTemp, Display, TEXT("✅ GameInitializer: All Databases force-loaded and registered"));

    UResourceManagerSubsystem* ResourceMgr = GetWorld()->GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>();
    if (ResourceMgr)
    {
        ResourceMgr->SetHumanStartingResources(HumanStartingStockpile);
        ResourceMgr->SetEnemyStartingResources(EnemyStartingStockpile);
        UE_LOG(LogTemp, Display, TEXT("AStrategyGameInitializer: Applied custom starting resources"));
    }

    // === START THE SIMULATION ===
    Campaign->StartSimulation();
    UE_LOG(LogTemp, Display, TEXT("🚀 Simulation STARTED — AI will act every day"));
}