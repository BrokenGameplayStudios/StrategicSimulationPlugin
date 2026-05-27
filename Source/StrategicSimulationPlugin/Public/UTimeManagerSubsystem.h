#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Misc/DateTime.h"
#include "StrategicSimulationTypes.h"
#include "UTimeManagerSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDayPassed, int32, NewDay);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSimulationStarted);

UCLASS()
class STRATEGICSIMULATIONPLUGIN_API UTimeManagerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** The in-game date/time when the current simulation run was started */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Simulation")
    FDateTime SimulationStartDate;

    /** Fires when SetStartingDate or StartSimulation is called (perfect for New Game / Reset logic) */
    UPROPERTY(BlueprintAssignable, Category = "Simulation")
    FOnSimulationStarted OnSimulationStarted;

    /** Returns true if the simulation is actively running */
    UFUNCTION(BlueprintCallable, Category = "Simulation")
    bool IsSimulating() const;

    /** Returns how many days the current simulation run has been active */
    UFUNCTION(BlueprintCallable, Category = "Simulation")
    int32 GetTotalSimulationDays() const;

    /** Starts (or resumes) the simulation — does NOT change dates */
    UFUNCTION(BlueprintCallable, Category = "Simulation")
    void StartSimulation();

    UFUNCTION(BlueprintCallable, Category = "Time")
    void SetStartingDate(FDateTime NewStartDate);

    UFUNCTION(BlueprintCallable, Category = "Time")
    void StopSimulation();

    UFUNCTION(BlueprintCallable, Category = "Time")
    void AdvanceDays(int32 NumDays);

    UFUNCTION(BlueprintCallable, Category = "Time")
    void SetTimeScale(float NewScale);

    UFUNCTION(BlueprintCallable, Category = "Time")
    void TogglePause();

    UFUNCTION(BlueprintCallable, Category = "Time")
    int32 GetCurrentDay() const;

    UFUNCTION(BlueprintCallable, Category = "Time")
    FDateTime GetCurrentGameDate() const;

    UFUNCTION(BlueprintCallable, Category = "Time")
    FText GetCurrentDayOfWeekName() const;

    UFUNCTION(BlueprintCallable, Category = "Time")
    float GetTimeScale() const { return TimeScale; }

    UFUNCTION(BlueprintCallable, Category = "Time")
    bool IsPaused() const { return bIsPaused; }

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnDayPassed OnDayPassed;

private:
    FDateTime CurrentGameDate;

    float TimeScale = 1.0f;
    bool bIsPaused = true;

    FTimerHandle RealTimeTimer;

    void RealTimeTick();
};