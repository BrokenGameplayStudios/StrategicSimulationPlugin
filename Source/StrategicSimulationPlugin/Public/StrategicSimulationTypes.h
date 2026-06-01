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

// Resources (now more granular while keeping old fields for compatibility)
UENUM(BlueprintType)
enum class EResourceType : uint8
{
    Money			UMETA(DisplayName = "Money"),
    Supplies		UMETA(DisplayName = "Supplies"),
    ExoticMaterial	UMETA(DisplayName = "Exotic Material"),
    ResearchPoints	UMETA(DisplayName = "Research Points")
};

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

    // === NEW GRANULAR RESOURCES (Phase 3.5) ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Metals = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Biologicals = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Chemicals = 0;

    // Helper operators (makes code much cleaner later)
    FResourceStockpile operator+(const FResourceStockpile& Other) const;
    FResourceStockpile operator-(const FResourceStockpile& Other) const;
    bool operator>=(const FResourceStockpile& Other) const;
    void Add(const FResourceStockpile& Other);
    void Subtract(const FResourceStockpile& Other);
};

// === CLEAN: Unified Item Category (used by soldiers, vehicles, research, production) ===
UENUM(BlueprintType)
enum class EItemCategory : uint8
{
    None                    UMETA(DisplayName = "None"),

    // Soldier Equipment
    SoldierWeapon           UMETA(DisplayName = "Soldier Weapon"),
    SoldierArmor            UMETA(DisplayName = "Soldier Armor"),

    // Vehicle Equipment
    VehicleWeapon           UMETA(DisplayName = "Vehicle Weapon"),
    VehicleDefense          UMETA(DisplayName = "Vehicle Defense System"),

    // Shared / Consumables
    Consumable              UMETA(DisplayName = "Consumable / Ammo"),

    // Research / Tech Flavor (kept for research tree filtering)
    Melee                   UMETA(DisplayName = "Melee"),
    Ballistic               UMETA(DisplayName = "Ballistic"),
    Explosive               UMETA(DisplayName = "Explosive"),
    Energy                  UMETA(DisplayName = "Energy"),
    Bio                     UMETA(DisplayName = "Bio"),
    Gas                     UMETA(DisplayName = "Gas"),
    Psychic                 UMETA(DisplayName = "Psychic"),
    Medical                 UMETA(DisplayName = "Medical"),
    Utility                 UMETA(DisplayName = "Utility")
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
    VehicleRepair, // Will be used for VehicleRepair bay
    Storage,
    Defense,
    Hanger,
    Medical,
    PowerPlant,
    Special
};

UENUM(BlueprintType)
enum class EVehicleType : uint8
{
    Transport,
    Gunship,
    Support,
    Scout,
    Heavy
};

// === NEW: Vehicle Damage System ===
UENUM(BlueprintType)
enum class EVehicleDamageState : uint8
{
    Undamaged       UMETA(DisplayName = "Undamaged"),
    LightlyDamaged  UMETA(DisplayName = "Lightly Damaged"),
    HeavilyDamaged  UMETA(DisplayName = "Heavily Damaged"),
    Destroyed       UMETA(DisplayName = "Destroyed")
};

// === NEW: Soldier Medical System ===
UENUM(BlueprintType)
enum class ESoldierStatus : uint8
{
    Healthy         UMETA(DisplayName = "Healthy"),
    Wounded         UMETA(DisplayName = "Wounded"),
    Critical        UMETA(DisplayName = "Critical"),
    Dead            UMETA(DisplayName = "Dead")
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

// === NEW: Mission Types ===
UENUM(BlueprintType)
enum class EMissionType : uint8
{
    Interception      UMETA(DisplayName = "Interception"),   // Vehicle-to-vehicle encounter in transit
    Defensive         UMETA(DisplayName = "Defensive"),      // Defend a location from attackers
    Offensive         UMETA(DisplayName = "Offensive")       // Attack/destroy enemy location/base
};

// Vehicle stats (used by UVehicleDefinition and UStrategyVehicle)
USTRUCT(BlueprintType)
struct FVehicleStats
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
    int32 MaxHealth = 100;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
    int32 AttackPower = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
    int32 SoldierCapacity = 4;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
    int32 MaxMissionDurationDays = 15;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
    int32 ProductionDays = 20;
};