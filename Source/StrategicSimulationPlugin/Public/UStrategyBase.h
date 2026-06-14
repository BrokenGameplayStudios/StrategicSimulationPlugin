#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "StrategicSimulationTypes.h"
#include "StrategicSiteDefinition.h"
#include "UStrategyFacility.h"
#include "UFacilityDefinition.h"
#include "UStrategyBase.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBaseFacilitiesChanged, UStrategyBase*, Base);

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

    UFUNCTION(BlueprintCallable, Category = "POW/KIA")
    int32 GetPOWCount() const { return ContainedPOWs.Num(); }

    UFUNCTION(BlueprintCallable, Category = "POW/KIA")
    int32 GetKIABodyCount() const { return StoredKIABodies.Num(); }

    UFUNCTION(BlueprintCallable, Category = "POW/KIA")
    void AddPOW(UStrategySoldier* Soldier);

    UFUNCTION(BlueprintCallable, Category = "POW/KIA")
    void AddKIABody(UStrategySoldier* Soldier);

    UFUNCTION(BlueprintCallable, Category = "POW/KIA")
    void ProcessContainment();   // called by facility

    UFUNCTION(BlueprintCallable, Category = "POW/KIA")
    void ProcessAutopsy();       // called by facility

    // ====================== POW / KIA UI HELPERS ======================
    /** Returns all POWs currently held in this base's Containment */
    UFUNCTION(BlueprintCallable, Category = "POW/KIA")
    TArray<UStrategySoldier*> GetContainedPOWs() const;

    /** Releases a POW back into the regular roster (they become a normal stationed soldier again) */
    UFUNCTION(BlueprintCallable, Category = "POW/KIA")
    void ReleasePOW(UStrategySoldier* POW);

    /** Processes a KIA body in Autopsy (gives research bonus then removes the body) */
    UFUNCTION(BlueprintCallable, Category = "POW/KIA")
    void ProcessKIABody(UStrategySoldier* Body);

    // Capacity helpers (used by AddPOW / AddKIABody)
    UFUNCTION(BlueprintCallable, Category = "POW/KIA")
    int32 GetTotalContainmentSlots() const;

    UFUNCTION(BlueprintCallable, Category = "POW/KIA")
    int32 GetTotalAutopsySlots() const;

    /** Per-base power tracking */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Power")
    int32 PowerProvided = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Power")
    int32 PowerDraw = 0;

    UFUNCTION(BlueprintCallable, Category = "Power")
    int32 GetNetPower() const { return PowerProvided - PowerDraw; }

    UFUNCTION(BlueprintCallable, Category = "Power")
    void UpdatePowerFromFacilities();

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnBaseFacilitiesChanged OnFacilitiesChanged;

    // === COMMAND & OPERATIONAL GATES (used by Research + Soldier) ===
    UFUNCTION(BlueprintCallable, Category = "Base")
    bool HasOperationalCommandCenter() const;

    UFUNCTION(BlueprintCallable, Category = "Base")
    bool HasOperationalFacilityOfType(EFacilityType FacilityType) const;  
    
    UFUNCTION(BlueprintCallable, Category = "Base")
    bool HasAnyFacilityOfType(EFacilityType FacilityType) const;

    UFUNCTION(BlueprintCallable, Category = "Base")
    int32 GetTotalProductionSlots() const;

    UFUNCTION(BlueprintCallable, Category = "Base")
    int32 GetTotalCapacityForType(EFacilityType FacilityType) const;

    /** Calculates total resources this base extracts per day from its site (only from operational facilities) */
    UFUNCTION(BlueprintCallable, Category = "Base|Resources")
    FResourceStockpile GetDailyExtractionFromSite() const;

    UFUNCTION(BlueprintCallable, Category = "Facilities")
    int32 GetTotalBuiltOfType(EFacilityType FacilityType) const;

    UFUNCTION(BlueprintCallable, Category = "Base")
    bool HasFacilityOfType(EFacilityType FacilityType) const;

    UFUNCTION(BlueprintCallable, Category = "Base")
    int32 GetCountOfType(EFacilityType FacilityType) const;

    UFUNCTION(BlueprintCallable, Category = "Base")
    bool IsOperational() const;

    /** Returns true if this base meets all prerequisites for building the given facility type */
    UFUNCTION(BlueprintCallable, Category = "Facility")
    bool CanBuildFacilityType(EFacilityType FacilityType) const;

    UFUNCTION(BlueprintCallable, Category = "Base")
    void AddFacility(UStrategyFacility* NewFacility);

    UFUNCTION(BlueprintCallable, Category = "Soldiers")
    TArray<UStrategySoldier*> GetStationedSoldiers() const;

    // === Dynamic Counts for Debug / Inspector ===
    UFUNCTION(BlueprintCallable, Category = "Base|Personnel")
    int32 GetStationedSoldiersCount() const;

    UFUNCTION(BlueprintCallable, Category = "Base|Personnel")
    int32 GetSoldiersOnMissionCount() const;

    UFUNCTION(BlueprintCallable, Category = "Base|Vehicles")
    int32 GetStationedVehiclesCount() const;

    UFUNCTION(BlueprintCallable, Category = "Base|Vehicles")
    int32 GetVehiclesOnMissionCount() const;
};