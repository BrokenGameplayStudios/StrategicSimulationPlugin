#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "StrategicSimulationTypes.h"
#include "UActiveProductionJob.h"
#include "UItemDefinition.h"
#include "UTimeManagerSubsystem.h"
#include "UEngineeringManagerSubsystem.generated.h"

UCLASS()
class STRATEGICSIMULATIONPLUGIN_API UEngineeringManagerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    UFUNCTION(BlueprintCallable, Category = "Procurement")
    bool PurchaseItem(EFactionType Faction, UItemDefinition* ItemDef, UStrategySoldier* TargetSoldier = nullptr);

    UFUNCTION(BlueprintCallable, Category = "Production")
    UActiveProductionJob* StartProduction(EFactionType Faction, UItemDefinition* ItemDef, int32 Quantity = 1);

    UFUNCTION(BlueprintCallable, Category = "Production")
    TArray<UActiveProductionJob*> GetActiveProduction(EFactionType Faction) const;

    // AI helper - smart production decision
    UFUNCTION(BlueprintCallable, Category = "Production")
    bool TryProduce(EFactionType Faction);

private:
    UPROPERTY(VisibleAnywhere, Transient, Category = "Production")
    TArray<UActiveProductionJob*> HumanProductionQueue;

    UPROPERTY(VisibleAnywhere, Transient, Category = "Production")
    TArray<UActiveProductionJob*> EnemyProductionQueue;

    UFUNCTION()
    void OnDayPassed(int32 NewDay);
};