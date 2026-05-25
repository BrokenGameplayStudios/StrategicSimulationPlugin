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

    // === DATABASES (force-load everything to prevent "not set" warnings) ===
    if (UFacilityDatabase* FacDB = FacilityDatabaseAsset.Get())
    {
        for (TSoftObjectPtr<UFacilityDefinition>& SoftDef : FacDB->AvailableFacilities)
            SoftDef.LoadSynchronous();
        Campaign->FacilityDatabaseAsset = FacilityDatabaseAsset;
    }
    if (USoldierClassDatabase* SoldierDB = SoldierClassDatabaseAsset.Get())
    {
        for (TSoftObjectPtr<USoldierClassDefinition>& SoftClass : SoldierDB->AvailableSoldierClasses)
            SoftClass.LoadSynchronous();
        Campaign->SoldierClassDatabaseAsset = SoldierClassDatabaseAsset;
    }
    if (UResearchDatabase* ResearchDB = ResearchDatabaseAsset.Get())
    {
        for (TSoftObjectPtr<UResearchTechDefinition>& SoftTech : ResearchDB->AvailableTechs)
            SoftTech.LoadSynchronous();
        Campaign->ResearchDatabaseAsset = ResearchDatabaseAsset;
    }
    UE_LOG(LogTemp, Display, TEXT("✅ GameInitializer: All Databases registered and pre-loaded"));

    // === START THE SIMULATION (AI now runs automatically every day) ===
    Campaign->StartSimulation();
    UE_LOG(LogTemp, Display, TEXT("🚀 Simulation STARTED — AI will act every day"));
}