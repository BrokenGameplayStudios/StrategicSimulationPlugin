#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "StrategicSimulationTypes.h"
#include "StrategicSiteDefinition.h"
#include "UStrategySaveGame.generated.h"

/** Minimum schema version that includes site-map round-trip data (PR-4). */
constexpr int32 StrategySiteMapSaveSchemaVersion = 2;

USTRUCT(BlueprintType)
struct FStrategySiteSaveData
{
    GENERATED_BODY()

    UPROPERTY()
    FGuid SiteId;

    UPROPERTY()
    FVector2D Location = FVector2D::ZeroVector;

    UPROPERTY()
    EStrategySiteType SiteType = EStrategySiteType::PotentialBase;

    UPROPERTY()
    EFactionType WreckOwnerFaction = EFactionType::Neutral;

    UPROPERTY()
    FString SiteName;

    UPROPERTY()
    FResourceStockpile MaxResources;

    UPROPERTY()
    FResourceStockpile CurrentResources;

    UPROPERTY()
    bool bHasBeenUsed = false;

    UPROPERTY()
    ESalvageSiteState SalvageState = ESalvageSiteState::Active;

    UPROPERTY()
    int32 CreatedOnSimulationDay = 0;

    UPROPERTY()
    int32 SalvageExpiresOnDay = 0;

    UPROPERTY()
    int32 KIACrashCount = 0;

    UPROPERTY()
    FSoftObjectPath SourceVehicleDefinitionPath;

    UPROPERTY()
    TArray<EFactionType> KnownFactions;

    UPROPERTY()
    bool bDiscoveredByHuman = false;

    UPROPERTY()
    bool bDiscoveredByEnemy = false;
};

UCLASS()
class STRATEGICSIMULATIONPLUGIN_API UStrategySaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    int32 SaveSchemaVersion = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    bool bIsContinuedCampaign = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    TArray<FStrategySiteSaveData> SavedSites;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    int32 CurrentDay = 1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    FResourceStockpile HumanResources;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    FResourceStockpile EnemyResources;

    // Metadata for save select screen
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    FDateTime LastSavedTime;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    FText HumanSummary;   // e.g. "12 Soldiers, 4 Facilities"

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    int32 HumanSoldierCount = 0;
};