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

    /** Logical map size in pixels (the "world" coordinates) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation", meta = (ClampMin = "800.0", ClampMax = "3840.0"))
    float LogicalMapWidth = 1920.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation", meta = (ClampMin = "600.0", ClampMax = "2160.0"))
    float LogicalMapHeight = 1080.0f;

    /** Everything spawns inside this border (100 px = playable area) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation", meta = (ClampMin = "50.0", ClampMax = "300.0"))
    float MapBorderPadding = 100.0f;

    /** Minimum distance between Human and Enemy starting Command Centers */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation", meta = (ClampMin = "200.0", ClampMax = "1500.0"))
    float MinDistanceBetweenFactions = 700.0f;

    /** Maximum bases each faction can own (AI expansion and base-building checks) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation", meta = (ClampMin = "1", ClampMax = "20"))
    int32 MaxFactionBases = 4;

    /** Spread each base's daily vehicle launches evenly across the 24-hour game day */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Simulation")
    bool bStaggerMissionLaunches = true;

    /** In-game day when fighters may begin scheduling Offensive (base attack) missions */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Simulation", meta = (ClampMin = "1", ClampMax = "60"))
    int32 OffensiveMissionsStartDay = 5;

    /** Minimum offensive rating required before AI engages in vehicular combat */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Simulation", meta = (ClampMin = "0", ClampMax = "500"))
    int32 MinOffenseToEngage = 10;

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