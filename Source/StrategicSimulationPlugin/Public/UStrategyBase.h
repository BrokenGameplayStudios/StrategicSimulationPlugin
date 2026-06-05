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

    /** NEW: POW / Prisoners System — captured enemy soldiers held at this base */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Base|Prisoners")
    TArray<UStrategySoldier*> CapturedPrisoners;

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