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

    if (ItemDatabaseAsset.IsValid())
    {
        Campaign->ItemDatabaseAsset = ItemDatabaseAsset;
        UE_LOG(LogTemp, Display, TEXT("✅ GameInitializer: ItemDatabaseAsset successfully set to %s"), *ItemDatabaseAsset->GetName());
    }
    else if (!ItemDatabaseAsset.IsNull())
    {
        // Force load if it's a valid soft reference but not yet loaded
        UItemDatabase* LoadedDB = ItemDatabaseAsset.LoadSynchronous();
        if (LoadedDB)
        {
            Campaign->ItemDatabaseAsset = ItemDatabaseAsset;
            UE_LOG(LogTemp, Display, TEXT("✅ GameInitializer: ItemDatabaseAsset loaded synchronously and set"));
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("❌ GameInitializer: Failed to load ItemDatabaseAsset"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("❌ GameInitializer: No ItemDatabaseAsset assigned in actor!"));
    }
}