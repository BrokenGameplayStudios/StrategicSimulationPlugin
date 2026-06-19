#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "StrategicSimulationTypes.h"
#include "UItemDefinition.h"
#include "UEngineeringManagerSubsystem.generated.h"

/** Game-instance subsystem for instant procurement and workshop-based item production. */
UCLASS()
class STRATEGICSIMULATIONPLUGIN_API UEngineeringManagerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /** Logs subsystem initialization; production slots are owned by workshop facilities. */
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /** Spends ItemDef->PurchaseCost and optionally adds the item to TargetSoldier's loadout. */
    UFUNCTION(BlueprintCallable, Category = "Procurement")
    bool PurchaseItem(EFactionType Faction, UItemDefinition* ItemDef, UStrategySoldier* TargetSoldier = nullptr);

    /** Purchases a weapon using full resource costs and equips it to a vehicle. Returns true on success. */
    UFUNCTION(BlueprintCallable, Category = "Procurement")
    bool PurchaseAndEquipVehicleWeapon(EFactionType Faction, UStrategyVehicle* TargetVehicle, UItemDefinition* WeaponDef);

    /** Buys ammo for a specific equipped weapon on a vehicle and refills it (cheap refill using Metals + Chemicals). */
    UFUNCTION(BlueprintCallable, Category = "Procurement")
    bool PurchaseAmmoForVehicle(EFactionType Faction, UStrategyVehicle* TargetVehicle, int32 WeaponIndex);

    /** Clears workshop item production jobs from all faction bases. */
    UFUNCTION(BlueprintCallable, Category = "Production")
    void ResetProduction();
};