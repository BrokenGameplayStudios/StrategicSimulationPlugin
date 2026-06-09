#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "StrategicSimulationTypes.h"
#include "StrategicSiteDefinition.generated.h"

UENUM(BlueprintType)
enum class EStrategySiteType : uint8
{
    PotentialBase,
    ResourceNode,
    SalvageSite,
    PointOfInterest
};

UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UStrategySiteDefinition : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Site")
    FVector2D Location;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Site")
    EStrategySiteType SiteType = EStrategySiteType::PotentialBase;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Site")
    EFactionType DiscoveringFaction = EFactionType::Human;

    UPROPERTY(BlueprintReadOnly, Category = "Site")
    bool bHasBeenUsed = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Site")
    FString SiteName = TEXT("Unnamed Site");
};