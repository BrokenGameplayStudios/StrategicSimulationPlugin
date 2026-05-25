#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UItemDatabase.h"
#include "UFacilityDefinition.h"
#include "AStrategyGameInitializer.generated.h"

UCLASS()
class STRATEGICSIMULATIONPLUGIN_API AStrategyGameInitializer : public AActor
{
    GENERATED_BODY()

public:
    AStrategyGameInitializer();

    virtual void BeginPlay() override;

    // Drag your DA_ItemDatabase here in the level
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    TSoftObjectPtr<UItemDatabase> ItemDatabaseAsset;

    // === Facility Definitions (data-driven, same pattern as ItemDatabase) ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facilities")
    TSoftObjectPtr<UFacilityDefinition> BasicLivingQuartersAsset;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facilities")
    TSoftObjectPtr<UFacilityDefinition> BasicWorkshopAsset;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facilities")
    TSoftObjectPtr<UFacilityDefinition> BasicLaboratoryAsset;

    // Optional: MedicalBay can be added later if you want AI to build it
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facilities")
    TSoftObjectPtr<UFacilityDefinition> BasicMedicalBayAsset;
};