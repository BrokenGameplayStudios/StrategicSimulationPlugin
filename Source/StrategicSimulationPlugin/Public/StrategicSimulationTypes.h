#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "StrategicSimulationTypes.generated.h"

/** Shared tolerance (px) for site discovery nearest-match, waypoint resolution, and mission dedup. */
constexpr float SiteMatchTolerance = 128.f;

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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resources")
    int32 Money = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resources")
    int32 Metals = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resources")
    int32 Biologicals = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resources")
    int32 Chemicals = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resources")
    int32 ExoticMaterial = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resources")
    int32 ResearchPoints = 0;

    // Helper operators (makes code much cleaner later)
    FResourceStockpile operator+(const FResourceStockpile& Other) const;
    FResourceStockpile operator-(const FResourceStockpile& Other) const;
    bool operator>=(const FResourceStockpile& Other) const;
    void Add(const FResourceStockpile& Other);
    void Subtract(const FResourceStockpile& Other);
    bool IsEmpty() const
    {
        return Money <= 0 && Metals <= 0 && Biologicals <= 0 &&
            Chemicals <= 0 && ExoticMaterial <= 0;
    }
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
    Command            UMETA(DisplayName = "Command"),
    LivingQuarters     UMETA(DisplayName = "Living Quarters"),
    Laboratory         UMETA(DisplayName = "Laboratory"),
    Workshop           UMETA(DisplayName = "Workshop"),
    VehicleRepair      UMETA(DisplayName = "Vehicle Repair"),
    Storage            UMETA(DisplayName = "Storage"),
    Defense            UMETA(DisplayName = "Defense"),
    Hanger             UMETA(DisplayName = "Hanger"),
    Medical            UMETA(DisplayName = "Medical"),
    Containment        UMETA(DisplayName = "Containment"),
    Autopsy            UMETA(DisplayName = "Autopsy"),
    PowerPlant         UMETA(DisplayName = "Power Plant"),
    Special            UMETA(DisplayName = "Special")
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

/** Mountain / terrain blocker shape for radar LOS (PR-10). */
UENUM(BlueprintType)
enum class ERadarBlockerShape : uint8
{
    Circle  UMETA(DisplayName = "Circle"),
    Rect    UMETA(DisplayName = "Rectangle")
};

/** Enemy vehicle track from Command Center passive radar (PR-11). */
USTRUCT(BlueprintType)
struct FRadarContact
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Radar Contact")
    FGuid ContactId;

    UPROPERTY(BlueprintReadOnly, Category = "Radar Contact")
    EFactionType DetectingFaction = EFactionType::Neutral;

    UPROPERTY(BlueprintReadOnly, Category = "Radar Contact")
    FString DetectingBaseName;

    /** Map position where the contact first entered radar range (intercept waypoint for gunships). */
    UPROPERTY(BlueprintReadOnly, Category = "Radar Contact")
    FVector2D FirstDetectedPosition = FVector2D::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Radar Contact")
    bool bHasFirstDetectedPosition = false;

    UPROPERTY(BlueprintReadOnly, Category = "Radar Contact")
    FVector2D LastPosition = FVector2D::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Radar Contact")
    FVector2D EstimatedVelocity = FVector2D::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Radar Contact")
    float LastSeenGameHours = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Radar Contact")
    bool bIsInboundThreat = false;

    UPROPERTY(BlueprintReadOnly, Category = "Radar Contact")
    float EstimatedHeadingDegrees = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Radar Contact")
    FString ThreatenedBaseName;

    UPROPERTY(BlueprintReadOnly, Category = "Radar Contact")
    FString TrackedVehicleName;
};

USTRUCT(BlueprintType)
struct FRadarBlockerZone
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radar LOS")
    ERadarBlockerShape Shape = ERadarBlockerShape::Circle;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radar LOS")
    FVector2D Center = FVector2D::ZeroVector;

    /** Radius for circle blockers (logical map pixels). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radar LOS", meta = (ClampMin = "10.0", ClampMax = "800.0"))
    float Radius = 120.0f;

    /** Half-extents for rectangle blockers (logical map pixels). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radar LOS")
    FVector2D HalfExtent = FVector2D(80.0f, 60.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radar LOS")
    FString Label;
};

// === NEW: Mission Types ===
UENUM(BlueprintType)
enum class EMissionType : uint8
{
    Interception      UMETA(DisplayName = "Interception"),   // Vehicle-to-vehicle encounter in transit
    Defensive         UMETA(DisplayName = "Defensive"),      // Defend a location from attackers
    Offensive         UMETA(DisplayName = "Offensive"),      // Attack/destroy enemy location/base
    Recon             UMETA(DisplayName = "Recon"),      // Scouting / exploration mission
    Salvage           UMETA(DisplayName = "Salvage")     // Recover resources from vehicle wrecks
};

/** Faction A = Human, Faction B = Enemy in contested salvage resolution. */
UENUM(BlueprintType)
enum class ESalvageContestOutcome : uint8
{
    FactionAWins    UMETA(DisplayName = "Human Wins"),
    FactionBWins    UMETA(DisplayName = "Enemy Wins"),
    FactionAAborts  UMETA(DisplayName = "Human Aborts"),
    FactionBAborts  UMETA(DisplayName = "Enemy Aborts"),
    MutualRetreat   UMETA(DisplayName = "Mutual Retreat")
};

class UStrategyVehicle;
class UStrategySoldier;
class UStrategyBase;

USTRUCT(BlueprintType)
struct FSalvageContestForceSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Salvage Contest")
    EFactionType Faction = EFactionType::Neutral;

    UPROPERTY(BlueprintReadOnly, Category = "Salvage Contest")
    TObjectPtr<UStrategyBase> OriginBase = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "Salvage Contest")
    TArray<TObjectPtr<UStrategyVehicle>> Vehicles;

    UPROPERTY(BlueprintReadOnly, Category = "Salvage Contest")
    TArray<TObjectPtr<UStrategySoldier>> Soldiers;

    /** Placeholder for future cargo-hold tuning. */
    UPROPERTY(BlueprintReadOnly, Category = "Salvage Contest")
    int32 EstimatedSalvageCapacity = 0;
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

UENUM(BlueprintType)
enum class EVehicleBehavior : uint8
{
    Scouting        UMETA(DisplayName = "Scouting"),
    Patrolling      UMETA(DisplayName = "Patrolling"),
    Returning       UMETA(DisplayName = "Returning"),
    Evading         UMETA(DisplayName = "Evading"),
    Attacking       UMETA(DisplayName = "Attacking"),
    Escorting       UMETA(DisplayName = "Escorting"),
    Idle            UMETA(DisplayName = "Idle"),
    Ignore          UMETA(DisplayName = "Ignore")
};

/** Movement lifecycle phase — drives positioning; separate from tactical EVehicleBehavior */
UENUM(BlueprintType)
enum class EVehicleMissionPhase : uint8
{
    Docked      UMETA(DisplayName = "Docked"),
    EnRoute     UMETA(DisplayName = "En Route"),
    OnStation   UMETA(DisplayName = "On Station"),
    Combat      UMETA(DisplayName = "Combat"),
    Returning   UMETA(DisplayName = "Returning")
};