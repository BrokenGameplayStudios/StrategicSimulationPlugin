#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UItemDatabase.h"
#include "UFacilityDatabase.h"
#include "USoldierClassDatabase.h"
#include "UResearchDatabase.h"
#include "UVehicleDatabase.h"          // NEW
#include "AStrategyGameInitializer.generated.h"

UCLASS()
class STRATEGICSIMULATIONPLUGIN_API AStrategyGameInitializer : public AActor
{
    GENERATED_BODY()

public:
    AStrategyGameInitializer();

    virtual void BeginPlay() override;

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
};