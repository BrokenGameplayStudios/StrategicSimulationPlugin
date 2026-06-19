#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "StrategicSimulationTypes.h"
#include "StrategicSiteDefinition.h"
#include "UStrategyFacility.h"
#include "UFacilityDefinition.h"
#include "UStrategyBase.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBaseFacilitiesChanged, UStrategyBase*, Base);

/**
 * Runtime object representing a faction base: facilities, power, site link,
 * POW/KIA storage, and personnel/vehicle accounting helpers.
 */
UCLASS(BlueprintType)
class STRATEGICSIMULATIONPLUGIN_API UStrategyBase : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Base")
    FText BaseName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Base")
    FVector2D MapLocation;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ownership")
    EFactionType OwningFaction = EFactionType::Human;

    /** The site this base was built on (if any). A site can only have one base. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Base|Site")
    UStrategySiteDefinition* BuiltOnSite = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Facilities")
    TArray<UStrategyFacility*> Facilities;

    // === POW / KIA STORAGE (per-base) ===
    UPROPERTY(VisibleAnywhere, Transient, Category = "POW/KIA")
    TArray<UStrategySoldier*> ContainedPOWs;     // POWs held in this base's Containment

    UPROPERTY(VisibleAnywhere, Transient, Category = "POW/KIA")
    TArray<UStrategySoldier*> StoredKIABodies;   // Enemy KIA bodies in this base's Autopsy

    /** Returns the number of POWs held in this base's containment facilities. */
    UFUNCTION(BlueprintCallable, Category = "POW/KIA")
    int32 GetPOWCount() const { return ContainedPOWs.Num(); }

    /** Returns the number of enemy KIA bodies stored for autopsy. */
    UFUNCTION(BlueprintCallable, Category = "POW/KIA")
    int32 GetKIABodyCount() const { return StoredKIABodies.Num(); }

    // ====================== POW / KIA UI HELPERS ======================
    /** Returns all POWs currently held in this base's Containment */
    UFUNCTION(BlueprintCallable, Category = "POW/KIA")
    TArray<UStrategySoldier*> GetContainedPOWs() const;

    /** Processes a KIA body in Autopsy (gives research bonus then removes the body) */
    UFUNCTION(BlueprintCallable, Category = "POW/KIA")
    void ProcessKIABody(UStrategySoldier* Body);

    /** Total POW slots from operational containment facilities. */
    UFUNCTION(BlueprintCallable, Category = "POW/KIA")
    int32 GetTotalContainmentSlots() const;

    /** Total KIA body slots from operational autopsy facilities. */
    UFUNCTION(BlueprintCallable, Category = "POW/KIA")
    int32 GetTotalAutopsySlots() const;

    /** Per-base power tracking */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Power")
    int32 PowerProvided = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Power")
    int32 PowerDraw = 0;

    /** Net power at this base (provided minus draw). */
    UFUNCTION(BlueprintCallable, Category = "Power")
    int32 GetNetPower() const { return PowerProvided - PowerDraw; }

    /** Recalculates PowerProvided and PowerDraw from operational facilities. */
    UFUNCTION(BlueprintCallable, Category = "Power")
    void UpdatePowerFromFacilities();

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnBaseFacilitiesChanged OnFacilitiesChanged;

    // === COMMAND & OPERATIONAL GATES (used by Research + Soldier) ===
    /** True when an operational Command facility exists at this base. */
    UFUNCTION(BlueprintCallable, Category = "Base")
    bool HasOperationalCommandCenter() const;

    /** True when an operational facility of the given type exists. */
    UFUNCTION(BlueprintCallable, Category = "Base")
    bool HasOperationalFacilityOfType(EFacilityType FacilityType) const;  
    
    /** True when any facility of the type exists (including under construction). */
    UFUNCTION(BlueprintCallable, Category = "Base")
    bool HasAnyFacilityOfType(EFacilityType FacilityType) const;

    /** Sum of production slots across all operational facilities. */
    UFUNCTION(BlueprintCallable, Category = "Base")
    int32 GetTotalProductionSlots() const;

    /** Sum of Capacity for operational facilities of the given type. */
    UFUNCTION(BlueprintCallable, Category = "Base")
    int32 GetTotalCapacityForType(EFacilityType FacilityType) const;

    /** Calculates total resources this base extracts per day from its site (only from operational facilities) */
    UFUNCTION(BlueprintCallable, Category = "Base|Resources")
    FResourceStockpile GetDailyExtractionFromSite() const;

    /** Count of operational facilities of the given type at this base. */
    UFUNCTION(BlueprintCallable, Category = "Facilities")
    int32 GetTotalBuiltOfType(EFacilityType FacilityType) const;

    /** True if any facility of the type exists (built or in progress). */
    UFUNCTION(BlueprintCallable, Category = "Base")
    bool HasFacilityOfType(EFacilityType FacilityType) const;

    /** Total count of facilities of the type including non-operational. */
    UFUNCTION(BlueprintCallable, Category = "Base")
    int32 GetCountOfType(EFacilityType FacilityType) const;

    /** True when the base has an operational Command Center. */
    UFUNCTION(BlueprintCallable, Category = "Base")
    bool IsOperational() const;

    /** Returns true if this base meets all prerequisites for building the given facility type */
    UFUNCTION(BlueprintCallable, Category = "Facility")
    bool CanBuildFacilityType(EFacilityType FacilityType) const;

    /** Appends a facility and broadcasts OnFacilitiesChanged. */
    UFUNCTION(BlueprintCallable, Category = "Base")
    void AddFacility(UStrategyFacility* NewFacility);

    /** Returns soldiers stationed at this base (via soldier manager roster). */
    UFUNCTION(BlueprintCallable, Category = "Soldiers")
    TArray<UStrategySoldier*> GetStationedSoldiers() const;

    // === Dynamic Counts for Debug / Inspector ===
    /** Count of roster soldiers with StationedBase equal to this base. */
    UFUNCTION(BlueprintCallable, Category = "Base|Personnel")
    int32 GetStationedSoldiersCount() const;

    /** Count of soldiers on active missions originating from this base. */
    UFUNCTION(BlueprintCallable, Category = "Base|Personnel")
    int32 GetSoldiersOnMissionCount() const;

    /** Count of vehicles parked in operational hangars at this base. */
    UFUNCTION(BlueprintCallable, Category = "Base|Vehicles")
    int32 GetStationedVehiclesCount() const;

    /** Count of vehicles on live missions originating from this base. */
    UFUNCTION(BlueprintCallable, Category = "Base|Vehicles")
    int32 GetVehiclesOnMissionCount() const;

    /** First operational hangar at this base, or nullptr */
    UFUNCTION(BlueprintCallable, Category = "Base|Vehicles")
    UStrategyFacility* FindFirstOperationalHangar() const;
};