#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UItemDatabase.h"
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
};