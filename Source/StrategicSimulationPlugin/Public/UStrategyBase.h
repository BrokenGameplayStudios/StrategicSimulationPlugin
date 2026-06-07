#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "StrategicSimulationTypes.h"
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
    bool HasOperationalFacilityOfType(EFacilityType FacilityType) const;   // <-- THIS IS THE NEW ONE WE NEED
    
    UFUNCTION(BlueprintCallable, Category = "Base")
    bool HasAnyFacilityOfType(EFacilityType FacilityType) const;

    UFUNCTION(BlueprintCallable, Category = "Base")
    int32 GetTotalProductionSlots() const;

    UFUNCTION(BlueprintCallable, Category = "Base")
    int32 GetTotalCapacityForType(EFacilityType FacilityType) const;

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
};