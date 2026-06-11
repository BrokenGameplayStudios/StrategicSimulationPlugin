#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UItemDatabase.h"
#include "UFacilityDatabase.h"
#include "USoldierClassDatabase.h"
#include "UResearchDatabase.h"
#include "UVehicleDatabase.h" // NEW
#include "AStrategyGameInitializer.generated.h"

UCLASS()
class STRATEGICSIMULATIONPLUGIN_API AStrategyGameInitializer : public AActor
{
    GENERATED_BODY()

public:
    AStrategyGameInitializer();

    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Simulation")
    bool bStartWithHumanAI = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Simulation")
    bool bStartWithEnemyAI = true;

    /** Master toggle for extra debug logging (Facility Ticks, etc.) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bVerboseLogging = false;

    /** Show [UNLOCK] messages when something new becomes available */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bShowUnlockMessages = true;

    /** Show detailed facility tick logs (can be very noisy with many bases) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bShowFacilityTicks = false;

    // === NEW: MAP / STRATEGIC SITE GENERATION (editable in level) ===
   /** How many potential base sites to spawn on the strategy map */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation", meta = (ClampMin = "5", ClampMax = "100"))
    int32 NumberOfStrategicSites = 25;

    /** Minimum distance (in world units) between any two sites. Higher = more spread out. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation", meta = (ClampMin = "100.0", ClampMax = "800.0"))
    float MinimumDistanceBetweenSites = 350.0f;

    // === Starting Resources (editable in editor) ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Starting Resources")
    FResourceStockpile HumanStartingStockpile;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Starting Resources")
    FResourceStockpile EnemyStartingStockpile;

    // === DATABASES ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Databases")
    TSoftObjectPtr<UItemDatabase> ItemDatabaseAsset;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Databases")
    TSoftObjectPtr<UFacilityDatabase> FacilityDatabaseAsset;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Databases")
    TSoftObjectPtr<USoldierClassDatabase> SoldierClassDatabaseAsset;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Databases")
    TSoftObjectPtr<UResearchDatabase> ResearchDatabaseAsset;

    // NEW: Vehicle Database
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Databases")
    TSoftObjectPtr<UVehicleDatabase> VehicleDatabaseAsset;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Databases")
    TSoftObjectPtr<UItemDatabase> VehicleItemDatabaseAsset;
};