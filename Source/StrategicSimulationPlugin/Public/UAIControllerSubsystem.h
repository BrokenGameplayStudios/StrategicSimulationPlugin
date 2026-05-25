#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "StrategicSimulationTypes.h"
#include "UItemDefinition.h"
#include "UTimeManagerSubsystem.h"
#include "UAIControllerSubsystem.generated.h"

UCLASS()
class STRATEGICSIMULATIONPLUGIN_API UAIControllerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    UFUNCTION(BlueprintCallable, Category = "Debug")
    void Debug_RunAI();

    // Public so Campaign can call it if needed
    void RunAIForFaction(EFactionType Faction, int32 CurrentDay);

private:
    UFUNCTION()
    void OnDayPassed(int32 NewDay);

    bool TryRecruit(EFactionType Faction);
    bool TryBuyAndEquip(EFactionType Faction);
};