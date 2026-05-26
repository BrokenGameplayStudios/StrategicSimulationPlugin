#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "StrategicSimulationTypes.generated.h"

// Faction symmetry
UENUM(BlueprintType)
enum class EFactionType : uint8
{
    Human		UMETA(DisplayName = "Human"),
    Enemy		UMETA(DisplayName = "Enemy"),
    Neutral		UMETA(DisplayName = "Neutral")
};

// Resources
UENUM(BlueprintType)
enum class EResourceType : uint8
{
    Money			UMETA(DisplayName = "Money"),
    Supplies		UMETA(DisplayName = "Supplies"),
    ExoticMaterial	UMETA(DisplayName = "Exotic Material"),
    ResearchPoints	UMETA(DisplayName = "Research Points")
};

// Tech categories (melee, ballistic, etc. — same for both sides)
UENUM(BlueprintType)
enum class ETechCategory : uint8
{
    Melee,
    Ballistic,
    Explosive,
    Energy,
    Bio,
    Gas,
    Psychic
};

// Tech tiers
UENUM(BlueprintType)
enum class ETechTier : uint8
{
    Tier0,
    Tier1,
    Tier2,
    Tier3,
    Tier4
};

// Facility types
UENUM(BlueprintType)
enum class EFacilityType : uint8
{
    Command,
    LivingQuarters,
    Laboratory,
    Workshop,
    Storage,
    Defense,
    Medical,
    PowerPlant,
    Special
};

// Soldier base stats (used by soldier classes and runtime soldiers)
USTRUCT(BlueprintType)
struct FSoldierStats
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Health = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Aim = 65;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Defense = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Willpower = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Mobility = 12;
};

// Simple resource cost (used by research, production, facilities)
USTRUCT(BlueprintType)
struct FResourceStockpile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Money = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Supplies = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ExoticMaterial = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ResearchPoints = 0;
};